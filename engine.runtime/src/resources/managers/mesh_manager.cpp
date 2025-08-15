
#include "mesh_manager.h"

#include "exceptions.h"
#include "platform/file_system.h"
#include "renderer/vertex.h"
#include "resources/resource_system.h"
#include "string/string_utils.h"

namespace C3D
{
    ResourceManager<Mesh>::ResourceManager() : IResourceManager(MemoryType::Mesh, ResourceType::Mesh, nullptr, "models") {}

    bool ResourceManager<Mesh>::Read(const String& name, Mesh& resource)
    {
        if (name.Empty())
        {
            ERROR_LOG("No valid name was provided.");
            return false;
        }

        String fullPath = String::FromFormat("{}/{}/{}.{}", Resources.GetBasePath(), typePath, name, "obj");

        // Check if the requested file exists with the current extension
        if (!File::Exists(fullPath))
        {
            ERROR_LOG("Unable to find a mesh file called: '{}'.", name);
            return false;
        }

        if (!m_file.Open(fullPath, FileModeRead))
        {
            ERROR_LOG("Unable to read file: '{}'.", fullPath);
            return false;
        }

        // Copy the path to the file
        resource.fullPath = fullPath;
        // Copy the name of the resource
        resource.name = name;

        auto result = ImportObjFile(resource);
        if (!result)
        {
            ERROR_LOG("Failed to parse obj file.");
        }

        // Always close the file
        m_file.Close();

        return result;
    }

    void ResourceManager<Mesh>::Cleanup(Mesh& resource) const
    {
        resource.name.Destroy();
        resource.fullPath.Destroy();
        resource.vertices.Destroy();
        resource.indices.Destroy();
    }

    bool ResourceManager<Mesh>::ImportObjFile(Mesh& resource)
    {
        // Reserve enough space in the line to hold all data we might find
        m_line.Reserve(512);

        // Count the number of lines and reserve space in all our dynamic arrays
        CountLinesAndReserveSpace();

        // Reset the file to the beginning so we can read it again
        m_file.Seek(0);

        // Read the actual data this time
        while (m_file.ReadLine(m_line))
        {
            // Skip blank lines
            if (m_line.Empty()) continue;

            // Check based on the first character in the line
            switch (m_line[0])
            {
                case '#':   // Comment so we skip this line
                case ' ':   // Lines starting with whitespace we can safely ignore
                case '\n':  // Lines starting with a newline character we can ignore
                    continue;
                case 'v':  // Line starts with 'v' meaning it will contain vertex data
                    ObjParseVertexLine();
                    break;
                case 'f':  // Line starts with 'f' meaning it will contain face data
                    ObjParseFaceLine();
                    break;
                default:
                    INFO_LOG("Skipped line: '{}' since we have no parsing rule for it.", m_line);
                    break;
            }
        }

        // Construct a Mesh out of the parsed data

        // Reserve enough space for all the vertices that we parsed
        resource.vertices.Reserve(m_vs.Size());

        // Reserve enough space for all the indices which is (m_fs.Size() / 3) since every face entry contains 3 elements (v/vt/vn)
        u32 indexCount = m_fs.Size() / 3;
        resource.indices.Reserve(indexCount);

        // Iterate over the faces and populate the vertices
        for (u32 i = 0; i < indexCount; ++i)
        {
            i32 vi  = m_fs[i * 3 + 0] - 1;
            i32 vti = m_fs[i * 3 + 1] - 1;
            i32 vni = m_fs[i * 3 + 2] - 1;

            Vertex vertex   = {};
            vertex.position = m_vs[vi];
            vertex.normal   = (vni == FACE_INDEX_NOT_POPULATED) ? vec3(0.f, 0.f, 1.f) : m_vns[vni];
            vertex.texture  = (vti == FACE_INDEX_NOT_POPULATED) ? vec2(0.f) : m_vts[vti];

            resource.vertices.EmplaceBack(vertex);
        }

        // Populate the indices
        for (u32 i = 0; i < indexCount; ++i)
        {
            resource.indices.EmplaceBack(i);
        }

        // TODO: Remap vertices so we remove duplicated vertices (and add indices to compensate)

        // TODO: Optimize the mesh further to improve rendering performance

        // Cleanup our internal data and reset our counters etc.
        m_line.Destroy();
        m_vs.Destroy();
        m_vns.Destroy();
        m_vts.Destroy();
        m_fs.Destroy();

        m_fI         = 0;
        m_faceFormat = FaceFormat::Unknown;

        return true;
    }

    void ResourceManager<Mesh>::CountLinesAndReserveSpace()
    {
        u32 vCount = 0, vnCount = 0, vtCount = 0, fCount = 0;

        // We first parse the entire file to figure out how many elements we have
        while (m_file.ReadLine(m_line))
        {
            // Skip empty lines
            if (m_line.Empty()) continue;

            // Check based on the first character in the line
            switch (m_line[0])
            {
                case '#':   // Comment so we skip this line
                case ' ':   // Lines starting with whitespace we can safely ignore
                case '\n':  // Lines starting with a newline character we can ignore
                    continue;
                case 'v':  // Line starts with 'v' meaning it will contain vertex data
                    switch (m_line[1])
                    {
                        case ' ':  // Only 'v' so this line contains position
                        {
                            vCount++;
                            break;
                        }
                        case 'n':  // 'vn' so this line contains normal
                        {
                            vnCount++;
                            break;
                        }
                        case 't':  // 'vt' so this line contains texture coords
                        {
                            vtCount++;
                            break;
                        }
                        default:
                            WARN_LOG("Unexpected character after 'v' found: '{}'.", m_line[1]);
                            break;
                    }
                    break;
                case 'f':
                    fCount++;
                    break;
                default:
                    INFO_LOG("Skipped line: '{}' since we have no parsing rule for it.", m_line);
                    break;
            }
        }

        // Reserve enough space in all dynamic arrays for the number of lines we found of each type
        m_vs.Reserve(vCount);
        m_vns.Reserve(vnCount);
        m_vts.Reserve(vtCount);

        // TODO: We assume 3 vertices per face (all faces create triangles) which is not required by the spec
        // We take 3 entries per vertex (v,vt,vn) and then 3 vertices per face
        m_fs.Resize(fCount * 3 * 3);
        // Start of all entries at 0 which indicates there was no index in the OBJ file
        std::memset(m_fs.GetData(), 0, sizeof(i32) * m_fs.Size());
    }

    void ResourceManager<Mesh>::ObjParseVertexLine()
    {
        // Check the second char in the line
        switch (m_line[1])
        {
            case ' ':  // Only 'v' so this line contains position
            {
                vec3 pos;
                char tp[3];
                sscanf(m_line.Data(), "%s %f %f %f", tp, &pos.x, &pos.y, &pos.z);
                m_vs.PushBack(pos);
                break;
            }
            case 'n':  // 'vn' so this line contains normal
            {
                vec3 norm;
                char tn[3];
                sscanf(m_line.Data(), "%s %f %f %f", tn, &norm.x, &norm.y, &norm.z);
                m_vns.PushBack(norm);
                break;
            }
            case 't':  // 'vt' so this line contains texture coords
            {
                vec2 tex;
                char tx[3];
                sscanf(m_line.Data(), "%s %f %f", tx, &tex.x, &tex.y);
                m_vts.PushBack(tex);
                break;
            }
            default:
                WARN_LOG("Unexpected character after 'v' found: '{}'.", m_line[1]);
                break;
        }
    }

    void ResourceManager<Mesh>::ObjParseFaceLine()
    {
        // TODO: This only supports faces with 3 indices per face
        // TODO: This does not support negative indices yet

        switch (m_faceFormat)
        {
            case FaceFormat::V_vt_vn:
            {
                // format looks like f v0/vt0/vn0 v1/vt1/vn1 v2/vt2/vn2 ...
                char t[2];
                auto vi0 = (m_fI + 3 * 0) + 0;
                auto vi1 = (m_fI + 3 * 1) + 0;
                auto vi2 = (m_fI + 3 * 2) + 0;

                auto vti0 = (m_fI + 3 * 0) + 1;
                auto vti1 = (m_fI + 3 * 1) + 1;
                auto vti2 = (m_fI + 3 * 2) + 1;

                auto vni0 = (m_fI + 3 * 0) + 2;
                auto vni1 = (m_fI + 3 * 1) + 2;
                auto vni2 = (m_fI + 3 * 2) + 2;

                sscanf(m_line.Data(), "%s %d/%d/%d %d/%d/%d %d/%d/%d", t, &m_fs[vi0], &m_fs[vti0], &m_fs[vni0], &m_fs[vi1], &m_fs[vti1], &m_fs[vni1],
                       &m_fs[vi2], &m_fs[vti2], &m_fs[vni2]);
                break;
            }
            case FaceFormat::V_vt:
            {
                // format looks like f v0/vt0 v1/vt1 v2/vt2 ...
                char t[2];
                auto vi0 = (m_fI + 3 * 0) + 0;
                auto vi1 = (m_fI + 3 * 1) + 0;
                auto vi2 = (m_fI + 3 * 2) + 0;

                auto vti0 = (m_fI + 3 * 0) + 1;
                auto vti1 = (m_fI + 3 * 1) + 1;
                auto vti2 = (m_fI + 3 * 2) + 1;

                sscanf(m_line.Data(), "%s %d/%d %d/%d %d/%d", t, &m_fs[vi0], &m_fs[vti0], &m_fs[vi1], &m_fs[vti1], &m_fs[vi2], &m_fs[vti2]);
                break;
            }
            case FaceFormat::V_vn:
            {
                // format looks like f v0//vn0 v1//vn1 v2//vn2 ...
                char t[2];
                auto vi0 = (m_fI + 3 * 0) + 0;
                auto vi1 = (m_fI + 3 * 1) + 0;
                auto vi2 = (m_fI + 3 * 2) + 0;

                auto vni0 = (m_fI + 3 * 0) + 2;
                auto vni1 = (m_fI + 3 * 1) + 2;
                auto vni2 = (m_fI + 3 * 2) + 2;

                sscanf(m_line.Data(), "%s %d//%d %d//%d %d//%d", t, &m_fs[vi0], &m_fs[vni0], &m_fs[vi1], &m_fs[vni1], &m_fs[vi2], &m_fs[vni2]);
            }
            case FaceFormat::V:
            {
                // format looks like f v0 v1 v2 ...
                char t[2];
                auto vi0 = (m_fI + 3 * 0) + 0;
                auto vi1 = (m_fI + 3 * 1) + 0;
                auto vi2 = (m_fI + 3 * 2) + 0;

                sscanf(m_line.Data(), "%s %d %d %d", t, &m_fs[vi0], &m_fs[vi1], &m_fs[vi2]);
            }
            case FaceFormat::Unknown:
            {
                // Determine the format before parsing
                if (!m_vts.Empty() && !m_vns.Empty())
                {
                    // format looks like f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3 ...
                    m_faceFormat = FaceFormat::V_vt_vn;
                }
                else if (!m_vts.Empty())
                {
                    // format looks like f v1/vt1 v2/vt2 v3/vt3 ...
                    m_faceFormat = FaceFormat::V_vt;
                }
                else if (!m_vns.Empty())
                {
                    // format looks like f v1//vn1 v2//vn2 v3//vn3 ...
                    m_faceFormat = FaceFormat::V_vn;
                }
                else
                {
                    // format looks like f v1 v2 v3 ...
                    m_faceFormat = FaceFormat::V;
                }
                break;
            }
        }

        m_fI += 9;
    }
}  // namespace C3D
