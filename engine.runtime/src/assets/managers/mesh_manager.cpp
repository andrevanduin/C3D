
#include "mesh_manager.h"

#include "exceptions.h"
#include "math/c3d_math.h"
#include "platform/file_system.h"
#include "renderer/utils/mesh_utils.h"
#include "renderer/vertex.h"
#include "string/string_utils.h"
#include "system/system_manager.h"

namespace C3D
{
    MeshManager::MeshManager() : IAssetManager(MemoryType::Mesh, AssetType::Mesh, "models") {}

    bool MeshManager::Read(const String& name, Mesh& asset)
    {
        if (name.Empty())
        {
            ERROR_LOG("No valid name was provided.");
            return false;
        }

        String fullPath = String::FromFormat("{}/{}/{}.{}", m_assetPath, m_subFolder, name, "obj");

        // Check if the requested file exists with the current extension
        if (!File::Exists(fullPath))
        {
            ERROR_LOG("Unable to find a mesh file called: '{}'.", name);
            return false;
        }

        // Copy the path to the file
        asset.path = fullPath;
        // Copy the name of the asset
        asset.name = name;

        auto result = ImportObjFile(asset);
        if (!result)
        {
            ERROR_LOG("Failed to parse obj file.");
        }

        return result;
    }

    void MeshManager::Cleanup(Mesh& asset)
    {
        asset.name.Destroy();
        asset.path.Destroy();
        asset.vertices.Destroy();
        asset.indices.Destroy();
        asset.meshlets.Destroy();
        asset.meshletData.Destroy();
    }

    i32 MeshManager::FixIndex(i32 index, u32 size) { return (index >= 0) ? index - 1 : i32(size) + index; }

    bool MeshManager::ImportObjFile(Mesh& asset)
    {
        INFO_LOG("Importing obj file: '{}'", asset.path);

        DynamicArray<Vertex> vertices;
        u32 indexCount = 0;

        {
            ScopedTimer timer(String::FromFormat("Importing: '{}'.", asset.path));

            // Open our file
            FILE* file = fopen(asset.path.Data(), "rb");
            if (!file)
            {
                ERROR_LOG("Failed to read file: '{}'.", asset.path);
                return false;
            }

            // Start off all arrays at a reasonable capacity
            m_vs.Create();
            m_vns.Create();
            m_vts.Create();
            m_fs.Create();

            char buffer[65536];

            u64 bytesRead = 0;
            while (!std::feof(file))
            {
                bytesRead += std::fread(buffer + bytesRead, sizeof(char), sizeof(buffer) - bytesRead, file);

                u64 lineIndex = 0;

                while (lineIndex < bytesRead)
                {
                    // Try to get the end of the current line
                    const void* eol = std::memchr(buffer + lineIndex, '\n', bytesRead - lineIndex);
                    if (!eol)
                    {
                        break;
                    }

                    // Ensure we are zero-terminated for our Parse methods
                    u64 next     = static_cast<const char*>(eol) - buffer;
                    buffer[next] = '\0';

                    // Process the next line
                    ObjParseLine(buffer + lineIndex);

                    // We continue after our null character
                    lineIndex = next + 1;
                }

                // Ensure we did not read too far
                C3D_ASSERT(lineIndex <= bytesRead);

                // Move the prefix of the last line in the buffer to the beginning of the buffer for the next iteration
                std::memmove(buffer, buffer + lineIndex, bytesRead - lineIndex);
                bytesRead -= lineIndex;
            }

            if (bytesRead)
            {
                // We still have some bytes (a last line) to process
                C3D_ASSERT(bytesRead < sizeof(buffer));
                buffer[bytesRead] = 0;

                ObjParseLine(buffer);
            }

            std::fclose(file);

            // Reserve enough space for all the indices which is (m_fs.size / 3) since every face entry contains 3 elements (v/vt/vn)
            indexCount = m_fs.size / 3;

            // Reserve enough space for all the vertices that we parsed (which before optimization == indexCount)
            vertices.Reserve(indexCount);

            // Iterate over the faces and populate the vertices
            for (u32 i = 0; i < indexCount; ++i)
            {
                i32 vi  = m_fs.data[i * 3 + 0];
                i32 vti = m_fs.data[i * 3 + 1];
                i32 vni = m_fs.data[i * 3 + 2];

                Vertex vertex = {};

                vertex.pos = { m_vs.data[vi * 3 + 0], m_vs.data[vi * 3 + 1], m_vs.data[vi * 3 + 2] };

                // TODO: Fix rounding
                vertex.nx = (vni == FACE_INDEX_NOT_POPULATED) ? 127.f : static_cast<u8>(m_vns.data[vni * 3 + 0] * 127.f + 127.f);
                vertex.ny = (vni == FACE_INDEX_NOT_POPULATED) ? 127.f : static_cast<u8>(m_vns.data[vni * 3 + 1] * 127.f + 127.f);
                vertex.nz = (vni == FACE_INDEX_NOT_POPULATED) ? 127.f : static_cast<u8>(m_vns.data[vni * 3 + 2] * 127.f + 127.f);

                vertex.tx = (vti == FACE_INDEX_NOT_POPULATED) ? 0 : QuantizeHalf(m_vts.data[vti * 3 + 0]);
                vertex.ty = (vti == FACE_INDEX_NOT_POPULATED) ? 0 : QuantizeHalf(m_vts.data[vti * 3 + 1]);

                vertices.EmplaceBack(vertex);
            }
        }

        {
            ScopedTimer timer(String::FromFormat("Remap optimization of: '{}'.", asset.name));

            DynamicArray<u32> remap;
            u32 uniqueVertexCount = MeshUtils::GenerateVertexRemap(vertices, indexCount, remap);

            asset.vertices.Resize(uniqueVertexCount);
            asset.indices.Resize(indexCount);

            MeshUtils::RemapVertices(asset, indexCount, vertices, remap);
            MeshUtils::RemapIndices(asset, indexCount, remap);

            INFO_LOG("Went from {} to {} vertices (reduced by {:.2f}%).", vertices.Size(), uniqueVertexCount,
                     (static_cast<f32>(uniqueVertexCount) - vertices.Size()) / vertices.Size() * -100);
        }

        {
            ScopedTimer timer(String::FromFormat("Optimization for Vertex Cache and Fetch of: '{}'.", asset.name));

            MeshUtils::OptimizeForVertexCache(asset);
            MeshUtils::OptimizeForVertexFetch(asset);
        }

        // Cleanup our internal data and reset our counters etc.
        m_vs.Destroy();
        m_vns.Destroy();
        m_vts.Destroy();
        m_fs.Destroy();

        m_faceFormat = FaceFormat::Unknown;
        return true;
    }

    void MeshManager::ObjParseLine(const char* line)
    {
        if (line[0] == 'v')
        {
            ObjParseVertexLine(line);
        }
        else if (line[0] == 'f')
        {
            ObjParseFaceLine(line);
        }
        // Else we simply ignore the line
    }

    void MeshManager::ObjParseVertexLine(const char* s)
    {
        // Skip the 'v'
        s++;

        if (*s == 'n')
        {
            // 'vn' so this line contains normals
            // Skip 'n '
            s += 2;

            f32 x = StringUtils::ParseF32(s, &s);
            f32 y = StringUtils::ParseF32(s, &s);
            f32 z = StringUtils::ParseF32(s, &s);

            if (m_vns.size + 3 > m_vns.capacity)
            {
                m_vns.Grow();
            }

            m_vns.data[m_vns.size++] = x;
            m_vns.data[m_vns.size++] = y;
            m_vns.data[m_vns.size++] = z;
        }
        else if (*s == 't')
        {
            // 'vt' so this line contains texture coords
            // Skip 't '
            s += 2;

            f32 x = StringUtils::ParseF32(s, &s);
            f32 y = StringUtils::ParseF32(s, &s);
            f32 z = StringUtils::ParseF32(s, &s);

            if (m_vts.size + 3 > m_vts.capacity)
            {
                m_vts.Grow();
            }

            m_vts.data[m_vts.size++] = x;
            m_vts.data[m_vts.size++] = y;
            m_vts.data[m_vts.size++] = z;
        }
        else
        {
            // Only 'v' so this line contains position
            f32 x = StringUtils::ParseF32(s, &s);
            f32 y = StringUtils::ParseF32(s, &s);
            f32 z = StringUtils::ParseF32(s, &s);

            if (m_vs.size + 3 > m_vs.capacity)
            {
                m_vs.Grow();
            }

            m_vs.data[m_vs.size++] = x;
            m_vs.data[m_vs.size++] = y;
            m_vs.data[m_vs.size++] = z;
        }
    }

    const char* MeshManager::ParseFace(const char* s, i32& vi, i32& vti, i32& vni)
    {
        // Face format can look like:
        // 1. v1 v2 v3
        // 2. v1/vt1 v2/vt2 v3/vt3
        // 3. v1//vn1 v2//vn2 v3//vn3
        // 4. v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3

        s = StringUtils::SkipWhitespace(s);

        // We should always have atleast the vertex index
        vi = StringUtils::ParseI32(s, &s);

        if (*s != '/')
        {
            // Format 1.
            return s;
        }
        s++;

        if (*s != '/')
        {
            // Format is either 2 or 4.
            vti = StringUtils::ParseI32(s, &s);
        }

        // Format is 2, 3 or 4.
        if (*s != '/')
        {
            // Format is 2.
            return s;
        }
        s++;

        // Format is either 3 or 4.
        vni = StringUtils::ParseI32(s, &s);

        return s;
    }

    void MeshManager::ObjParseFaceLine(const char* line)
    {
        // Skip the "f "
        const char* s = line + 2;

        u32 fi      = 0;
        i32 f[3][3] = {};

        u32 vSize  = m_vs.size / 3;
        u32 vtSize = m_vts.size / 3;
        u32 vnSize = m_vns.size / 3;

        while (*s)
        {
            i32 vi = 0, vti = 0, vni = 0;
            s = ParseFace(s, vi, vti, vni);

            if (vi == 0) break;

            f[fi][0] = FixIndex(vi, vSize);
            f[fi][1] = FixIndex(vti, vtSize);
            f[fi][2] = FixIndex(vni, vnSize);

            if (fi == 2)
            {
                if (m_fs.size + 9 > m_fs.capacity)
                {
                    m_fs.Grow();
                }

                // We have parsed everything
                std::memcpy(m_fs.data + m_fs.size, f, 9 * sizeof(i32));
                m_fs.size += 9;
                break;
            }
            else
            {
                fi++;
            }
        }
    }
}  // namespace C3D
