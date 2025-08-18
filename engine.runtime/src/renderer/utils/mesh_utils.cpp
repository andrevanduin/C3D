
#include "mesh_utils.h"

#include "containers/hash_map.h"

namespace C3D
{

    bool MeshUtils::BuildMeshlets(Mesh& mesh)
    {
        Meshlet meshlet = {};

        DynamicArray<u8> meshletVertices;
        meshletVertices.ResizeAndFill(mesh.vertices.Size(), 0xFF);

        constexpr auto maxTriangles = 126 / 3;

        for (u32 i = 0; i < mesh.indices.Size(); i += 3)
        {
            u32 a = mesh.indices[i + 0];
            u32 b = mesh.indices[i + 1];
            u32 c = mesh.indices[i + 2];

            u8& av = meshletVertices[a];
            u8& bv = meshletVertices[b];
            u8& cv = meshletVertices[c];

            if (meshlet.vertexCount + (av == 0xFF) + (bv == 0xFF) + (cv == 0xFF) > 64 || meshlet.triangleCount + 1 > maxTriangles)
            {
                mesh.meshlets.EmplaceBack(meshlet);

                for (u32 j = 0; j < meshlet.vertexCount; ++j)
                {
                    meshletVertices[meshlet.vertices[j]] = 0xFF;
                }

                meshlet = {};
            }

            if (av == 0xFF)
            {
                av                                      = meshlet.vertexCount;
                meshlet.vertices[meshlet.vertexCount++] = a;
            }

            if (bv == 0xFF)
            {
                bv                                      = meshlet.vertexCount;
                meshlet.vertices[meshlet.vertexCount++] = b;
            }

            if (cv == 0xFF)
            {
                cv                                      = meshlet.vertexCount;
                meshlet.vertices[meshlet.vertexCount++] = c;
            }

            meshlet.indices[meshlet.triangleCount * 3 + 0] = av;
            meshlet.indices[meshlet.triangleCount * 3 + 1] = bv;
            meshlet.indices[meshlet.triangleCount * 3 + 2] = cv;

            meshlet.triangleCount++;
        }

        if (meshlet.triangleCount > 0)
        {
            mesh.meshlets.EmplaceBack(meshlet);
        }

        return true;
    }

    namespace
    {
        constexpr u32 SLOT_EMPTY = -1;

        // This work is based on:
        // Matthias Teschner, Bruno Heidelberger, Matthias Mueller, Danat Pomeranets, Markus Gross. Optimized Spatial Hashing for Collision Detection of
        // Deformable Objects. 2003 John McDonald, Mark Kilgard. Crack-Free Point-Normal Triangles using Adjacent Edge Normals. 2010 John Hable. Variable Rate
        // Shading with Visibility Buffer Rendering. 2024
        static unsigned int hashUpdate4(unsigned int h, const u8* key, size_t len)
        {
            // MurmurHash2
            const unsigned int m = 0x5bd1e995;
            const int r          = 24;

            while (len >= 4)
            {
                unsigned int k = *reinterpret_cast<const unsigned int*>(key);

                k *= m;
                k ^= k >> r;
                k *= m;

                h *= m;
                h ^= k;

                key += 4;
                len -= 4;
            }

            return h;
        }

        struct VertexHasher
        {
            const u8* data;
            u64 vertexSize;
            u64 vertexStride;

            u64 hash(u32 index) const { return hashUpdate4(0, data + index * vertexStride, vertexSize); }

            bool equal(u32 lhs, u32 rhs) const { return memcmp(data + lhs * vertexStride, data + rhs * vertexStride, vertexSize) == 0; }
        };

        u64 HashBuckets(size_t count)
        {
            u64 buckets = 1;
            while (buckets < count + count / 4) buckets *= 2;

            return buckets;
        }

        u32* HashLookup(u32* table, u64 buckets, const VertexHasher& hasher, u32 key)
        {
            C3D_ASSERT(buckets > 0);
            C3D_ASSERT((buckets & (buckets - 1)) == 0);

            u64 hashmod = buckets - 1;
            u64 bucket  = hasher.hash(key) & hashmod;

            for (u64 probe = 0; probe <= hashmod; ++probe)
            {
                u32& item = table[bucket];

                if (item == SLOT_EMPTY)
                {
                    // Slot is currently empty so we return so it can be filled
                    return &item;
                }

                if (hasher.equal(item, key))
                {
                    // We have this item. So we simply return it
                    return &item;
                }

                // Collision
                bucket = (bucket + probe + 1) & hashmod;
            }

            C3D_ASSERT_MSG(false, "Hash table is full.");
            return nullptr;
        }
    }  // namespace

    u32 MeshUtils::GenerateVertexRemap(const DynamicArray<Vertex>& vertices, u32 indexCount, DynamicArray<u32>& outRemap)
    {
        VertexHasher hasher = {};
        hasher.data         = reinterpret_cast<const u8*>(vertices.GetData());
        hasher.vertexSize   = sizeof(Vertex);
        hasher.vertexStride = sizeof(Vertex);

        u32 vertexCount = vertices.Size();

        // Remap table will be at most vertices.Size() large and we fill it with SLOT_EMPTY
        outRemap.ResizeAndFill(vertexCount, SLOT_EMPTY);

        // Determine the size we need for our hash table
        u64 hashTableSize = HashBuckets(vertexCount);
        // Create a block of memory for our hashtable
        u32* table = Memory.Allocate<u32>(MemoryType::Mesh, hashTableSize);
        // Initialize all entries as empty
        std::memset(table, SLOT_EMPTY, hashTableSize * sizeof(u32));

        // Keep track of the index of our next vertex
        u32 nextVertex = 0;

        for (u32 i = 0; i < indexCount; ++i)
        {
            C3D_ASSERT(i < vertexCount);

            if (outRemap[i] != SLOT_EMPTY)
            {
                // We have already mapped this index
                continue;
            }

            u32* entry = HashLookup(table, hashTableSize, hasher, i);
            if (*entry == SLOT_EMPTY)
            {
                *entry      = i;
                outRemap[i] = nextVertex++;
            }
            else
            {
                C3D_ASSERT(outRemap[*entry] != SLOT_EMPTY);
                // Not empty so we have seen this vertex before
                outRemap[i] = outRemap[*entry];
            }
        }

        // Cleanup our table
        Memory.Free(table);

        C3D_ASSERT_MSG(nextVertex <= vertexCount, "The remapped number of vertices must be <= the original number of vertices");
        return nextVertex;
    }

    void MeshUtils::RemapVertices(Mesh& mesh, u32 indexCount, const DynamicArray<Vertex>& vertices, const DynamicArray<u32>& remap)
    {
        for (u32 i = 0; i < indexCount; ++i)
        {
            if (remap[i] != SLOT_EMPTY)
            {
                // We have a unique vertex
                mesh.vertices[remap[i]] = vertices[i];
            }
        }
    }

    void MeshUtils::RemapIndices(Mesh& mesh, u32 indexCount, const DynamicArray<u32>& remap)
    {
        for (u32 i = 0; i < indexCount; ++i)
        {
            C3D_ASSERT(remap[i] != SLOT_EMPTY);

            mesh.indices[i] = remap[i];
        }
    }

}  // namespace C3D