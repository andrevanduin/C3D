
#include "mesh_utils.h"

#include "containers/hash_map.h"
#include "math/c3d_math.h"

namespace C3D
{
#define SHOW_NUM_CULLED 1

    namespace
    {
        constexpr u32 SLOT_EMPTY       = -1;
        constexpr u64 K_CACHE_SIZE_MAX = 16;
        constexpr u64 K_VALENCE_MAX    = 8;

        // This work is based on:
        // Matthias Teschner, Bruno Heidelberger, Matthias Mueller, Danat Pomeranets, Markus Gross. Optimized Spatial Hashing for Collision Detection of
        // Deformable Objects. 2003 John McDonald, Mark Kilgard. Crack-Free Point-Normal Triangles using Adjacent Edge Normals. 2010 John Hable. Variable Rate
        // Shading with Visibility Buffer Rendering. 2024
        u32 hashUpdate4(u32 h, const u8* key, u64 len)
        {
            // MurmurHash2
            const u32 m = 0x5bd1e995;
            const i32 r = 24;

            while (len >= 4)
            {
                u32 k = *reinterpret_cast<const u32*>(key);

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

        struct VertexScoreTable
        {
            f32 cache[1 + K_CACHE_SIZE_MAX];
            f32 live[1 + K_VALENCE_MAX];
        };

        // Tuned to minimize the ACMR of a GPU that has a cache profile similar to NVidia and AMD
        constexpr VertexScoreTable K_VERTEX_SCORE_TABLE = {
            { 0.f, 0.779f, 0.791f, 0.789f, 0.981f, 0.843f, 0.726f, 0.847f, 0.882f, 0.867f, 0.799f, 0.642f, 0.613f, 0.600f, 0.568f, 0.372f, 0.234f },
            { 0.f, 0.995f, 0.713f, 0.450f, 0.404f, 0.059f, 0.005f, 0.147f, 0.006f },
        };

        struct TriangleAdjacency
        {
            u32* counts  = nullptr;
            u32* offsets = nullptr;
            u32* data    = nullptr;

            void Allocate(u64 vertexCount, u64 indexCount)
            {
                counts  = Memory.Allocate<u32>(MemoryType::Array, vertexCount);
                offsets = Memory.Allocate<u32>(MemoryType::Array, vertexCount);
                data    = Memory.Allocate<u32>(MemoryType::Array, indexCount);
            }

            void Destroy()
            {
                if (counts) Memory.Free(counts);
                if (offsets) Memory.Free(offsets);
                if (data) Memory.Free(data);
            }
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

        void BuildTriangleAdjacency(TriangleAdjacency& adjacency, u32* indices, u64 indexCount, u64 vertexCount)
        {
            u64 faceCount = indexCount / 3;

            // Allocate arrays
            adjacency.Allocate(vertexCount, indexCount);

            for (u64 i = 0; i < indexCount; ++i)
            {
                C3D_ASSERT(indices[i] < vertexCount);

                adjacency.counts[indices[i]]++;
            }

            // Fill offset table
            u32 offset = 0;

            for (size_t i = 0; i < vertexCount; ++i)
            {
                adjacency.offsets[i] = offset;
                offset += adjacency.counts[i];
            }

            C3D_ASSERT(offset == indexCount);

            // Fill triangle data
            for (size_t i = 0; i < faceCount; ++i)
            {
                u32 a = indices[i * 3 + 0], b = indices[i * 3 + 1], c = indices[i * 3 + 2];

                adjacency.data[adjacency.offsets[a]++] = static_cast<u32>(i);
                adjacency.data[adjacency.offsets[b]++] = static_cast<u32>(i);
                adjacency.data[adjacency.offsets[c]++] = static_cast<u32>(i);
            }

            // Fix offsets that have been disturbed by the previous pass
            for (size_t i = 0; i < vertexCount; ++i)
            {
                C3D_ASSERT(adjacency.offsets[i] >= adjacency.counts[i]);

                adjacency.offsets[i] -= adjacency.counts[i];
            }
        }

        u32 GetNextVertexDeadEnd(const u32* deadEnd, u32& deadEndTop, u32& inputCursor, const u32* liveTriangles, u64 vertexCount)
        {
            // Check dead-end stack
            while (deadEndTop)
            {
                u32 vertex = deadEnd[--deadEndTop];

                if (liveTriangles[vertex] > 0) return vertex;
            }

            // Input order
            while (inputCursor < vertexCount)
            {
                if (liveTriangles[inputCursor] > 0) return inputCursor;

                ++inputCursor;
            }

            return INVALID_ID;
        }

        u32 GetNextVertexNeighbor(const u32* nextCandidatesBegin, const u32* nextCandidatesEnd, const u32* liveTriangles, const u32* cacheTimestamps,
                                  u32 timestamp, u32 cacheSize)
        {
            u32 bestCandidate = INVALID_ID;
            i32 bestPriority  = -1;

            for (const u32* nextCandidate = nextCandidatesBegin; nextCandidate != nextCandidatesEnd; ++nextCandidate)
            {
                u32 vertex = *nextCandidate;

                // Otherwise we don't need to process it
                if (liveTriangles[vertex] > 0)
                {
                    i32 priority = 0;

                    // Will it be in cache after fanning?
                    if (2 * liveTriangles[vertex] + timestamp - cacheTimestamps[vertex] <= cacheSize)
                    {
                        priority = timestamp - cacheTimestamps[vertex];  // position in cache
                    }

                    if (priority > bestPriority)
                    {
                        bestCandidate = vertex;
                        bestPriority  = priority;
                    }
                }
            }

            return bestCandidate;
        }

        f32 VertexScore(i32 cachePosition, u32 liveTriangles)
        {
            C3D_ASSERT(cachePosition >= -1 && cachePosition < static_cast<i32>(K_CACHE_SIZE_MAX));

            u32 liveTrianglesClamped = liveTriangles < K_VALENCE_MAX ? liveTriangles : K_VALENCE_MAX;

            return K_VERTEX_SCORE_TABLE.cache[1 + cachePosition] + K_VERTEX_SCORE_TABLE.live[liveTrianglesClamped];
        }

        u32 GetNextTriangleDeadEnd(u32& inputCursor, const u8* emitted_flags, u64 faceCount)
        {
            // Input order
            while (inputCursor < faceCount)
            {
                if (!emitted_flags[inputCursor]) return inputCursor;

                ++inputCursor;
            }

            return INVALID_ID;
        }
    }  // namespace

    bool MeshUtils::BuildMeshlets(Mesh& mesh)
    {
        Meshlet meshlet = {};

        DynamicArray<u8> meshletVertices;
        meshletVertices.ResizeAndFill(mesh.vertices.Size(), 0xFF);

        for (u32 i = 0; i < mesh.indices.Size(); i += 3)
        {
            u32 a = mesh.indices[i + 0];
            u32 b = mesh.indices[i + 1];
            u32 c = mesh.indices[i + 2];

            u8& av = meshletVertices[a];
            u8& bv = meshletVertices[b];
            u8& cv = meshletVertices[c];

            if (meshlet.vertexCount + (av == 0xFF) + (bv == 0xFF) + (cv == 0xFF) > MESHLET_MAX_VERTICES || meshlet.triangleCount >= MESHLET_MAX_TRIANGLES)
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

        while (mesh.meshlets.Size() % 32) mesh.meshlets.EmplaceBack();

        return true;
    }

    bool MeshUtils::BuildMesletCones(Mesh& mesh)
    {
        for (auto& meshlet : mesh.meshlets)
        {
            vec3 normals[126] = {};

            for (u32 i = 0; i < meshlet.triangleCount; ++i)
            {
                u32 a = meshlet.indices[i * 3 + 0];
                u32 b = meshlet.indices[i * 3 + 1];
                u32 c = meshlet.indices[i * 3 + 2];

                const Vertex& va = mesh.vertices[meshlet.vertices[a]];
                const Vertex& vb = mesh.vertices[meshlet.vertices[b]];
                const Vertex& vc = mesh.vertices[meshlet.vertices[c]];

                vec3 p10 = vb.pos - va.pos;
                vec3 p20 = vc.pos - va.pos;

                vec3 normal = glm::cross(p10, p20);

                float area    = glm::length(normal);
                float invArea = (area == 0.f) ? 0.f : 1 / area;

                normals[i].x = normal.x * invArea;
                normals[i].y = normal.y * invArea;
                normals[i].z = normal.z * invArea;
            }

            vec3 avgNormal = {};
            for (u32 i = 0; i < meshlet.triangleCount; ++i)
            {
                avgNormal.x += normals[i][0];
                avgNormal.y += normals[i][1];
                avgNormal.z += normals[i][2];
            }

            float avgLength = glm::length(avgNormal);

            if (avgLength == 0.f)
            {
                avgNormal.x = 1.f;
                avgNormal.y = 0.f;
                avgNormal.z = 0.f;
            }
            else
            {
                avgNormal /= avgLength;
            }

            float mindp = 1.f;

            for (u32 i = 0; i < meshlet.triangleCount; ++i)
            {
                float dp = glm::dot(normals[i], avgNormal);
                mindp    = std::min(mindp, dp);
            }

            f32 conew = mindp <= 0.f ? 1 : sqrtf(1 - mindp * mindp);

            meshlet.cone.x = avgNormal.x;
            meshlet.cone.y = avgNormal.y;
            meshlet.cone.z = avgNormal.z;
            meshlet.cone.w = conew;
        }

#if SHOW_NUM_CULLED
        constexpr vec3 view = vec3(0, 0, 1);
        u32 numCulled       = 0;

        for (auto& meshlet : mesh.meshlets)
        {
            vec3 cone = { meshlet.cone.x, meshlet.cone.y, meshlet.cone.z };
            if (glm::dot(cone, view) > meshlet.cone.w)
            {
                numCulled++;
            }
        }

        INFO_LOG("Number of meshlets culled with view (0, 0, 1): {}/{} ({:.2f}%)", numCulled, mesh.meshlets.Size(),
                 (static_cast<f32>(numCulled) / mesh.meshlets.Size()) * 100.f);
#endif

        return true;
    }

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

    void MeshUtils::OptimizeForVertexCache(Mesh& mesh)
    {
        C3D_ASSERT(mesh.indices.Size() % 3 == 0);

        // Guard for empty meshes
        if (mesh.vertices.Empty() || mesh.indices.Empty()) return;

        u64 indexCount  = mesh.indices.Size();
        u64 vertexCount = mesh.vertices.Size();
        u64 faceCount   = indexCount / 3;

        // Copy the indices so we can do an in-place update
        u32* indices = Memory.Allocate<u32>(MemoryType::Array, mesh.indices.Size());
        std::memcpy(indices, mesh.indices.GetData(), sizeof(u32) * mesh.indices.Size());

        // Build adjacency information
        TriangleAdjacency adjacency = {};
        BuildTriangleAdjacency(adjacency, indices, indexCount, vertexCount);

        // live triangle counts; note, we alias adjacency.counts as we remove triangles after emitting them so the counts always match
        unsigned int* liveTriangles = adjacency.counts;

        // Emitted flags
        u8* emittedFlags = Memory.Allocate<u8>(MemoryType::Array, faceCount);

        // Compute initial vertex scores
        f32* vertexScores = Memory.Allocate<f32>(MemoryType::Array, vertexCount);

        for (u64 i = 0; i < vertexCount; ++i)
        {
            vertexScores[i] = VertexScore(-1, liveTriangles[i]);
        }

        // Compute triangle scores
        f32* triangleScores = Memory.Allocate<f32>(MemoryType::Array, faceCount);

        for (u64 i = 0; i < faceCount; ++i)
        {
            u32 a = indices[i * 3 + 0];
            u32 b = indices[i * 3 + 1];
            u32 c = indices[i * 3 + 2];

            triangleScores[i] = vertexScores[a] + vertexScores[b] + vertexScores[c];
        }

        u32 cacheHolder[2 * (K_CACHE_SIZE_MAX + 4)];
        u32* cache     = cacheHolder;
        u32* cacheNew  = cacheHolder + K_CACHE_SIZE_MAX + 4;
        u64 cacheCount = 0;

        u32 currentTriangle = 0;
        u32 inputCursor     = 1;

        u32 outputTriangle = 0;

        while (currentTriangle != INVALID_ID)
        {
            C3D_ASSERT(outputTriangle < faceCount);

            u32 a = indices[currentTriangle * 3 + 0];
            u32 b = indices[currentTriangle * 3 + 1];
            u32 c = indices[currentTriangle * 3 + 2];

            // Output indices
            mesh.indices[outputTriangle * 3 + 0] = a;
            mesh.indices[outputTriangle * 3 + 1] = b;
            mesh.indices[outputTriangle * 3 + 2] = c;
            outputTriangle++;

            // Update emitted flags
            emittedFlags[currentTriangle]   = true;
            triangleScores[currentTriangle] = 0;

            // New triangle
            size_t cacheWrite      = 0;
            cacheNew[cacheWrite++] = a;
            cacheNew[cacheWrite++] = b;
            cacheNew[cacheWrite++] = c;

            // Old triangles
            for (u64 i = 0; i < cacheCount; ++i)
            {
                u32 index = cache[i];

                cacheNew[cacheWrite] = index;
                cacheWrite += (index != a) & (index != b) & (index != c);
            }

            u32* cacheTemp = cache;
            cache = cacheNew, cacheNew = cacheTemp;
            cacheCount = std::min(cacheWrite, K_CACHE_SIZE_MAX);

            // Remove emitted triangle from adjacency data
            // this makes sure that we spend less time traversing these lists on subsequent iterations
            // live triangle counts are updated as a byproduct of these adjustments
            for (size_t k = 0; k < 3; ++k)
            {
                u32 index = indices[currentTriangle * 3 + k];

                u32* neighbors    = &adjacency.data[0] + adjacency.offsets[index];
                u64 neighborsSize = adjacency.counts[index];

                for (u64 i = 0; i < neighborsSize; ++i)
                {
                    u32 tri = neighbors[i];

                    if (tri == currentTriangle)
                    {
                        neighbors[i] = neighbors[neighborsSize - 1];
                        adjacency.counts[index]--;
                        break;
                    }
                }
            }

            u32 bestTriangle = INVALID_ID;
            f32 bestScore    = 0;

            // Update cache positions, vertex scores and triangle scores, and find next best triangle
            for (u64 i = 0; i < cacheWrite; ++i)
            {
                u32 index = cache[i];

                // No need to update scores if we are never going to use this vertex
                if (adjacency.counts[index] == 0) continue;

                i32 cachePosition = i >= K_CACHE_SIZE_MAX ? -1 : static_cast<i32>(i);

                // Update vertex score
                f32 score     = VertexScore(cachePosition, liveTriangles[index]);
                f32 scoreDiff = score - vertexScores[index];

                vertexScores[index] = score;

                // Update scores of vertex triangles
                const u32* neighborsBegin = &adjacency.data[0] + adjacency.offsets[index];
                const u32* neighborsEnd   = neighborsBegin + adjacency.counts[index];

                for (const u32* it = neighborsBegin; it != neighborsEnd; ++it)
                {
                    u32 tri = *it;
                    C3D_ASSERT(!emittedFlags[tri]);

                    f32 triScore = triangleScores[tri] + scoreDiff;
                    C3D_ASSERT(triScore > 0);

                    bestTriangle = bestScore < triScore ? tri : bestTriangle;
                    bestScore    = bestScore < triScore ? triScore : bestScore;

                    triangleScores[tri] = triScore;
                }
            }

            // Step through input triangles in order if we hit a dead-end
            currentTriangle = bestTriangle;

            if (currentTriangle == INVALID_ID)
            {
                currentTriangle = GetNextTriangleDeadEnd(inputCursor, &emittedFlags[0], faceCount);
            }
        }

        C3D_ASSERT(inputCursor == faceCount);
        C3D_ASSERT(outputTriangle == faceCount);

        Memory.Free(indices);
        Memory.Free(emittedFlags);
        Memory.Free(vertexScores);
        Memory.Free(triangleScores);

        adjacency.Destroy();
    }

    void MeshUtils::OptimizeForVertexFetch(Mesh& mesh)
    {
        u64 indexCount  = mesh.indices.Size();
        u64 vertexCount = mesh.vertices.Size();

        C3D_ASSERT(indexCount % 3 == 0);

        // Copy the vertices so we can do an in-place update
        Vertex* vertices = Memory.Allocate<Vertex>(MemoryType::Array, vertexCount);
        std::memcpy(vertices, mesh.vertices.GetData(), vertexCount * sizeof(Vertex));

        // Build a vertex remap table
        u32* vertexRemap = Memory.Allocate<u32>(MemoryType::Array, vertexCount);
        std::memset(vertexRemap, INVALID_ID, vertexCount * sizeof(u32));

        u32 nextVertex = 0;

        for (u64 i = 0; i < indexCount; ++i)
        {
            u32 index = mesh.indices[i];
            C3D_ASSERT(index < vertexCount);

            u32& remap = vertexRemap[index];

            if (remap == INVALID_ID)
            {
                mesh.vertices[nextVertex] = vertices[index];

                remap = nextVertex++;
            }

            // Modify indices in place
            mesh.indices[i] = remap;
        }

        C3D_ASSERT(nextVertex <= vertexCount);

        Memory.Free(vertices);
        Memory.Free(vertexRemap);
    }

}  // namespace C3D