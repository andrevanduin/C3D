
#include "mesh_utils.h"

#include "containers/hash_map.h"
#include "math/c3d_math.h"
#include "time/scoped_timer.h"

namespace C3D
{
    namespace
    {
        constexpr u32 SLOT_EMPTY          = -1;
        constexpr u64 K_CACHE_SIZE_MAX    = 16;
        constexpr u64 K_VALENCE_MAX       = 8;
        constexpr u64 K_MESHLET_MAX_SEEDS = 256;
        constexpr u64 K_MESHLET_ADD_SEEDS = 4;

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

            bool equal(u32 lhs, u32 rhs) const { return std::memcmp(data + lhs * vertexStride, data + rhs * vertexStride, vertexSize) == 0; }
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

        constexpr vec3 K_AXES[7] = {
            // X, Y, Z
            vec3(1, 0, 0),
            vec3(0, 1, 0),
            vec3(0, 0, 1),

            // XYZ, -XYZ, X-YZ, XY-Z; normalized to unit length
            vec3(0.57735026f, 0.57735026f, 0.57735026f),
            vec3(-0.57735026f, 0.57735026f, 0.57735026f),
            vec3(0.57735026f, -0.57735026f, 0.57735026f),
            vec3(0.57735026f, 0.57735026f, -0.57735026f),
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

        struct Cone
        {
            vec3 p;
            vec3 n;
        };

        struct KDNode
        {
            union {
                f32 split;
                u32 index;
            };

            // Leaves: axis = 3, children = number of extra points after this one (0 if 'index' is the only point)
            // Branches: axis != 3, left subtree = skip 1, right subtree = skip 1+children
            u32 axis : 2;
            u32 children : 30;
        };

        void ComputeBoundingSphere(vec4& result, const vec3* points, u64 count)
        {
            // find extremum points along all axes; for each axis we get a pair of points with min/max coordinates
            u64 pmin[3] = { 0, 0, 0 };
            u64 pmax[3] = { 0, 0, 0 };

            for (u64 i = 0; i < count; ++i)
            {
                const vec3& p = points[i];

                for (u32 axis = 0; axis < 3; ++axis)
                {
                    pmin[axis] = (p[axis] < points[pmin[axis]][axis]) ? i : pmin[axis];
                    pmax[axis] = (p[axis] > points[pmax[axis]][axis]) ? i : pmax[axis];
                }
            }

            // Find the pair of points with largest distance
            f32 paxisd2 = 0;
            i32 paxis   = 0;

            for (u64 axis = 0; axis < 3; ++axis)
            {
                const vec3& p1 = points[pmin[axis]];
                const vec3& p2 = points[pmax[axis]];

                vec3 p21 = p2 - p1;
                f32 d2   = glm::dot(p21, p21);
                if (d2 > paxisd2)
                {
                    paxisd2 = d2;
                    paxis   = axis;
                }
            }

            // Use the longest segment as the initial sphere diameter
            const vec3 p1 = points[pmin[paxis]];
            const vec3 p2 = points[pmax[paxis]];

            vec3 center = (p1 + p2) / 2.f;
            f32 radius  = Sqrt(paxisd2) / 2.f;

            // Iteratively adjust the sphere up until all points fit
            for (u64 i = 0; i < count; ++i)
            {
                const vec3& p = points[i];
                f32 d2        = glm::dot(p - center, p - center);
                if (d2 > radius * radius)
                {
                    f32 d = Sqrt(d2);
                    C3D_ASSERT(d > 0);

                    f32 k = 0.5f + (radius / d) / 2;

                    center = center * k + p * (1 - k);
                    radius = (radius + d) / 2;
                }
            }

            result.x = center.x;
            result.y = center.y;
            result.z = center.z;
            result.w = radius;
        }

        f32 GetDistance(const vec3& d, bool aa)
        {
            if (!aa)
            {
                return glm::length(d);
            }

            vec3 r  = glm::abs(d);
            f32 rxy = r.x > r.y ? r.x : r.y;
            return rxy > r.z ? rxy : r.z;
        }

        f32 GetMeshletScore(f32 distance, f32 spread, f32 coneWeight, f32 expectedRadius)
        {
            if (coneWeight < 0)
            {
                return 1 + distance / expectedRadius;
            }

            f32 cone        = 1.f - spread * coneWeight;
            f32 coneClamped = cone < 1e-3f ? 1e-3f : cone;

            return (1 + distance / expectedRadius * (1 - coneWeight)) * coneClamped;
        }

        Cone GetMeshletCone(const Cone& acc, u32 triangleCount)
        {
            Cone result = acc;

            f32 centerScale = triangleCount == 0 ? 0.f : 1.f / f32(triangleCount);

            result.p *= centerScale;

            f32 axisLength = glm::dot(result.n, result.n);
            f32 axisScale  = axisLength == 0.f ? 0.f : 1.f / std::sqrtf(axisLength);

            result.n *= axisScale;

            return result;
        }

        u64 KDTreePartition(u32* indices, u64 count, const vec3* points, u64 stride, u32 axis, f32 pivot)
        {
            u64 m = 0;

            // Invariant: elements in range [0, m) are < pivot, elements in range [m, i) are >= pivot
            for (u64 i = 0; i < count; ++i)
            {
                f32 v = points[indices[i] * stride][axis];

                // Swap(m, i) unconditionally
                u32 t      = indices[m];
                indices[m] = indices[i];
                indices[i] = t;

                // When v >= pivot, we swap i with m without advancing it, preserving invariants
                m += v < pivot;
            }

            return m;
        }

        u64 KDTreeBuildLeaf(u64 offset, KDNode* nodes, u64 nodeCount, u32* indices, u64 count)
        {
            C3D_ASSERT(offset + count <= nodeCount);

            KDNode& result = nodes[offset];

            result.index    = indices[0];
            result.axis     = 3;
            result.children = static_cast<u32>(count - 1);

            // All remaining points are stored in nodes immediately following the leaf
            for (u64 i = 1; i < count; ++i)
            {
                KDNode& tail = nodes[offset + i];

                tail.index    = indices[i];
                tail.axis     = 3;
                tail.children = INVALID_ID >> 2;  // Bogus value to prevent misuse
            }

            return offset + count;
        }

        u64 KDTreeBuild(u64 offset, KDNode* nodes, u64 nodeCount, const vec3* points, u64 stride, u32* indices, u64 count, u64 leafSize)
        {
            C3D_ASSERT(count > 0);
            C3D_ASSERT(offset < nodeCount);

            if (count <= leafSize)
            {
                return KDTreeBuildLeaf(offset, nodes, nodeCount, indices, count);
            }

            vec3 mean = {};
            vec3 vars = {};
            f32 runc = 1, runs = 1;

            // Gather statistics on the points in the subtree using Welford's algorithm
            for (u64 i = 0; i < count; ++i, runc += 1.f, runs = 1.f / runc)
            {
                const vec3& point = points[indices[i] * stride];
                vec3 delta        = point - mean;
                mean += delta * runs;
                vars += delta * (point - mean);
            }

            // Split axis is one where the variance is largest
            u32 axis = (vars[0] >= vars[1] && vars[0] >= vars[2]) ? 0 : (vars[1] >= vars[2] ? 1 : 2);

            f32 split  = mean[axis];
            u64 middle = KDTreePartition(indices, count, points, stride, axis, split);

            // When the partition is degenerate simply consolidate the points into a single node
            if (middle <= leafSize / 2 || middle >= count - leafSize / 2)
            {
                return KDTreeBuildLeaf(offset, nodes, nodeCount, indices, count);
            }

            KDNode& result = nodes[offset];

            result.split = split;
            result.axis  = axis;

            // Left subtree is right after our node
            u64 next_offset = KDTreeBuild(offset + 1, nodes, nodeCount, points, stride, indices, middle, leafSize);

            // Distance to the right subtree is represented explicitly
            result.children = static_cast<u32>(next_offset - offset - 1);

            return KDTreeBuild(next_offset, nodes, nodeCount, points, stride, indices + middle, count - middle, leafSize);
        }

        void KDTreeNearest(KDNode* nodes, u32 root, const vec3* points, const u8* emittedFlags, const vec3& position, bool aa, u32& result, f32& limit)
        {
            const KDNode& node = nodes[root];

            if (node.axis == 3)
            {
                // Leaf
                for (u32 i = 0; i <= node.children; ++i)
                {
                    u32 index = nodes[root + i].index;

                    if (emittedFlags[index]) continue;

                    vec3 point   = points[index];
                    f32 distance = GetDistance(point - position, aa);

                    if (distance < limit)
                    {
                        result = index;
                        limit  = distance;
                    }
                }
            }
            else
            {
                // Branch; we order recursion to process the node that search position is in first
                f32 delta  = position[node.axis] - node.split;
                u32 first  = (delta <= 0) ? 0 : node.children;
                u32 second = first ^ node.children;

                KDTreeNearest(nodes, root + 1 + first, points, emittedFlags, position, aa, result, limit);

                // Only process the other node if it can have a match based on closest distance so far
                if (std::fabsf(delta) <= limit)
                {
                    KDTreeNearest(nodes, root + 1 + second, points, emittedFlags, position, aa, result, limit);
                }
            }
        }

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

            for (u64 i = 0; i < vertexCount; ++i)
            {
                adjacency.offsets[i] = offset;
                offset += adjacency.counts[i];
            }

            C3D_ASSERT(offset == indexCount);

            // Fill triangle data
            for (u64 i = 0; i < faceCount; ++i)
            {
                u32 a = indices[i * 3 + 0], b = indices[i * 3 + 1], c = indices[i * 3 + 2];

                adjacency.data[adjacency.offsets[a]++] = static_cast<u32>(i);
                adjacency.data[adjacency.offsets[b]++] = static_cast<u32>(i);
                adjacency.data[adjacency.offsets[c]++] = static_cast<u32>(i);
            }

            // Fix offsets that have been disturbed by the previous pass
            for (u64 i = 0; i < vertexCount; ++i)
            {
                C3D_ASSERT(adjacency.offsets[i] >= adjacency.counts[i]);

                adjacency.offsets[i] -= adjacency.counts[i];
            }
        }

        void BuildTriangleAdjacencySparse(TriangleAdjacency& adjacency, u32* indices, u64 indexCount, u64 vertexCount)
        {
            // Sparse mode can build adjacency more quickly by ignoring unused vertices, using a bit to mark visited vertices
            constexpr u32 SPARSE_SEEN = 1u << 31;
            C3D_ASSERT(indexCount < SPARSE_SEEN);

            u64 faceCount = indexCount / 3;

            // Allocate arrays
            adjacency.Allocate(vertexCount, indexCount);

            // Fill triangle counts
            for (u64 i = 0; i < indexCount; ++i)
            {
                C3D_ASSERT(indices[i] < vertexCount);
                adjacency.counts[indices[i]] = 0;
            }

            for (u64 i = 0; i < indexCount; ++i)
            {
                adjacency.counts[indices[i]]++;
            }

            // Fill offset table; uses SPARSE_SEEN bit to tag visited vertices
            u32 offset = 0;

            for (u64 i = 0; i < indexCount; ++i)
            {
                u32 v = indices[i];

                if ((adjacency.counts[v] & SPARSE_SEEN) == 0)
                {
                    adjacency.offsets[v] = offset;
                    offset += adjacency.counts[v];
                    adjacency.counts[v] |= SPARSE_SEEN;
                }
            }

            C3D_ASSERT(offset == indexCount);

            // Fill triangle data
            for (u64 i = 0; i < faceCount; ++i)
            {
                u32 a = indices[i * 3 + 0], b = indices[i * 3 + 1], c = indices[i * 3 + 2];

                adjacency.data[adjacency.offsets[a]++] = static_cast<u32>(i);
                adjacency.data[adjacency.offsets[b]++] = static_cast<u32>(i);
                adjacency.data[adjacency.offsets[c]++] = static_cast<u32>(i);
            }

            // Fix offsets that have been disturbed by the previous pass
            // also fix counts (that were marked with SPARSE_SEEN by the first pass)
            for (u64 i = 0; i < indexCount; ++i)
            {
                u32 v = indices[i];

                if (adjacency.counts[v] & SPARSE_SEEN)
                {
                    adjacency.counts[v] &= ~SPARSE_SEEN;

                    C3D_ASSERT(adjacency.offsets[v] >= adjacency.counts[v]);
                    adjacency.offsets[v] -= adjacency.counts[v];
                }
            }
        }

        f32 ComputeTriangleCones(Cone* triangles, const DynamicArray<u32>& indices, const DynamicArray<Vertex>& vertices)
        {
            u64 vertexCount = vertices.Size();
            u64 indexCount  = indices.Size();
            u64 faceCount   = indexCount / 3;

            f32 meshArea = 0;

            for (u64 i = 0; i < faceCount; ++i)
            {
                u32 a = indices[i * 3 + 0], b = indices[i * 3 + 1], c = indices[i * 3 + 2];
                C3D_ASSERT(a < vertexCount && b < vertexCount && c < vertexCount);

                // TODO: Z component doesn't quite match??

                const vec3& p0 = vertices[a].pos;
                const vec3& p1 = vertices[b].pos;
                const vec3& p2 = vertices[c].pos;

                vec3 p10 = p1 - p0;
                vec3 p20 = p2 - p0;

                vec3 normal = glm::cross(p10, p20);

                f32 area    = glm::length(normal);
                f32 invarea = (area == 0.f) ? 0.f : 1.f / area;

                triangles[i].p = (p0 + p1 + p2) / 3.f;
                triangles[i].n = normal * invarea;

                meshArea += area;
            }

            return meshArea;
        }

#if 0

        void FinishMeshlet(MeshUtils::Meshlet& meshlet, DynamicArray<u8>& meshletTriangles)
        {
            u64 offset = meshlet.triangleOffset + meshlet.triangleCount * 3;

            // fill 4b padding with 0
            while (offset & 3)
            {
                meshletTriangles[offset++] = 0;
            }
        }

        bool AppendMeshlet(MeshUtils::Meshlet& meshlet, u32 a, u32 b, u32 c, i16* used, DynamicArray<MeshUtils::Meshlet>& meshlets,
                           DynamicArray<u32>& meshletVertices, DynamicArray<u8>& meshletTriangles, u64 meshletOffset, u64 maxVertices, u64 maxTriangles,
                           bool split = false)
        {
            i16& av = used[a];
            i16& bv = used[b];
            i16& cv = used[c];

            bool result = false;

            int used_extra = (av < 0) + (bv < 0) + (cv < 0);

            if (meshlet.vertexCount + used_extra > maxVertices || meshlet.triangleCount >= maxTriangles || split)
            {
                meshlets[meshletOffset] = meshlet;

                for (u64 j = 0; j < meshlet.vertexCount; ++j)
                {
                    used[meshletVertices[meshlet.vertexOffset + j]] = -1;
                }

                FinishMeshlet(meshlet, meshletTriangles);

                meshlet.vertexOffset += meshlet.vertexCount;
                meshlet.triangleOffset += (meshlet.triangleCount * 3 + 3) & ~3;  // 4b padding
                meshlet.vertexCount   = 0;
                meshlet.triangleCount = 0;

                result = true;
            }

            if (av < 0)
            {
                av                                                            = i16(meshlet.vertexCount);
                meshletVertices[meshlet.vertexOffset + meshlet.vertexCount++] = a;
            }

            if (bv < 0)
            {
                bv                                                            = i16(meshlet.vertexCount);
                meshletVertices[meshlet.vertexOffset + meshlet.vertexCount++] = b;
            }

            if (cv < 0)
            {
                cv                                                            = i16(meshlet.vertexCount);
                meshletVertices[meshlet.vertexOffset + meshlet.vertexCount++] = c;
            }

            meshletTriangles[meshlet.triangleOffset + meshlet.triangleCount * 3 + 0] = static_cast<u8>(av);
            meshletTriangles[meshlet.triangleOffset + meshlet.triangleCount * 3 + 1] = static_cast<u8>(bv);
            meshletTriangles[meshlet.triangleOffset + meshlet.triangleCount * 3 + 2] = static_cast<u8>(cv);
            meshlet.triangleCount++;

            return result;
        }


        u32 GetNeighborTriangle(const MeshUtils::Meshlet& meshlet, const Cone& meshletCone, const DynamicArray<u32>& meshletVertices,
                                const DynamicArray<u32>& indices, const TriangleAdjacency& adjacency, const Cone* triangles, const u32* liveTriangles,
                                const i16* used, f32 meshletExpectedRadius, f32 coneWeight)
        {
            u32 bestTriangle = INVALID_ID;
            i32 bestPriority = 5;
            f32 bestScore    = FLT_MAX;

            for (size_t i = 0; i < meshlet.vertexCount; ++i)
            {
                u32 index = meshletVertices[meshlet.vertexOffset + i];

                u32* neighbors    = &adjacency.data[0] + adjacency.offsets[index];
                u64 neighborsSize = adjacency.counts[index];

                for (u64 j = 0; j < neighborsSize; ++j)
                {
                    u32 triangle = neighbors[j];
                    u32 a = indices[triangle * 3 + 0], b = indices[triangle * 3 + 1], c = indices[triangle * 3 + 2];

                    i32 extra = (used[a] < 0) + (used[b] < 0) + (used[c] < 0);
                    C3D_ASSERT(extra <= 2);

                    i32 priority = -1;

                    // Triangles that don't add new vertices to meshlets are max. priority
                    if (extra == 0)
                    {
                        priority = 0;
                    }
                    // Artificially increase the priority of dangling triangles as they're expensive to add to new meshlets
                    else if (liveTriangles[a] == 1 || liveTriangles[b] == 1 || liveTriangles[c] == 1)
                    {
                        priority = 1;
                    }
                    // If two vertices have live count of 2, removing this triangle will make another triangle dangling which is good for overall flow
                    else if ((liveTriangles[a] == 2) + (liveTriangles[b] == 2) + (liveTriangles[c] == 2) >= 2)
                    {
                        priority = 1 + extra;
                    }
                    // Otherwise adjust priority to be after the above cases, 3 or 4 based on used[] count
                    else
                    {
                        priority = 2 + extra;
                    }

                    // Since topology-based priority is always more important than the score, we can skip scoring in some cases
                    if (priority > bestPriority) continue;

                    const Cone& triCone = triangles[triangle];

                    float distance = GetDistance(triCone.p - meshletCone.p, coneWeight < 0);
                    f32 spread     = glm::dot(triCone.n, meshletCone.n);

                    f32 score = GetMeshletScore(distance, spread, coneWeight, meshletExpectedRadius);

                    // Note that topology-based priority is always more important than the score
                    // this helps maintain reasonable effectiveness of meshlet data and reduces scoring cost
                    if (priority < bestPriority || score < bestScore)
                    {
                        bestTriangle = triangle;
                        bestPriority = priority;
                        bestScore    = score;
                    }
                }
            }

            return bestTriangle;
        }

        u64 AppendSeedTriangles(u32* seeds, const MeshUtils::Meshlet& meshlet, const DynamicArray<u32>& meshletVertices, const DynamicArray<u32>& indices,
                                const TriangleAdjacency& adjacency, const Cone* triangles, const u32* liveTriangles, const vec3& corner)
        {
            u32 bestSeeds[K_MESHLET_MAX_SEEDS];
            u32 bestLive[K_MESHLET_MAX_SEEDS];
            f32 bestScore[K_MESHLET_MAX_SEEDS];

            for (u64 i = 0; i < K_MESHLET_MAX_SEEDS; ++i)
            {
                bestSeeds[i] = INVALID_ID;
                bestLive[i]  = INVALID_ID;
                bestScore[i] = FLT_MAX;
            }

            for (u64 i = 0; i < meshlet.vertexCount; ++i)
            {
                u32 index = meshletVertices[meshlet.vertexOffset + i];

                u32 bestNeighbor     = INVALID_ID;
                u32 bestNeighborLive = INVALID_ID;

                // Find the neighbor with the smallest live metric
                u32* neighbors    = &adjacency.data[0] + adjacency.offsets[index];
                u64 neighborsSize = adjacency.counts[index];

                for (u64 j = 0; j < neighborsSize; ++j)
                {
                    u32 triangle = neighbors[j];
                    u32 a = indices[triangle * 3 + 0], b = indices[triangle * 3 + 1], c = indices[triangle * 3 + 2];

                    u32 live = liveTriangles[a] + liveTriangles[b] + liveTriangles[c];

                    if (live < bestNeighborLive)
                    {
                        bestNeighbor     = triangle;
                        bestNeighborLive = live;
                    }
                }

                // Add the neighbor to the list of seeds; the list is unsorted and the replacement criteria is approximate
                if (bestNeighbor == INVALID_ID) continue;

                f32 bestNeighborScore = GetDistance(triangles[bestNeighbor].p - corner, false);

                for (u64 j = 0; j < K_MESHLET_ADD_SEEDS; ++j)
                {
                    // Non-strict comparison reduces the number of duplicate seeds (triangles adjacent to multiple vertices)
                    if (bestNeighborLive < bestLive[j] || (bestNeighborLive == bestLive[j] && bestNeighborScore <= bestScore[j]))
                    {
                        bestSeeds[j] = bestNeighbor;
                        bestLive[j]  = bestNeighborLive;
                        bestScore[j] = bestNeighborScore;
                        break;
                    }
                }
            }

            // Add surviving seeds to the meshlet
            u64 seedCount = 0;

            for (u64 i = 0; i < K_MESHLET_ADD_SEEDS; ++i)
            {
                if (bestSeeds[i] != INVALID_ID)
                {
                    seeds[seedCount++] = bestSeeds[i];
                }
            }

            return seedCount;
        }

        u64 PruneSeedTriangles(u32* seeds, u64 seedCount, const u8* emittedFlags)
        {
            u64 result = 0;

            for (u64 i = 0; i < seedCount; ++i)
            {
                u32 index = seeds[i];

                seeds[result] = index;
                result += emittedFlags[index] == 0;
            }

            return result;
        }

        u32 SelectSeedTriangle(const u32* seeds, u64 seedCount, const DynamicArray<u32>& indices, const Cone* triangles, const u32* liveTriangles,
                               const vec3& corner)
        {
            u32 bestSeed  = INVALID_ID;
            u32 bestLive  = INVALID_ID;
            f32 bestScore = FLT_MAX;

            for (u64 i = 0; i < seedCount; ++i)
            {
                u32 index = seeds[i];
                u32 a = indices[index * 3 + 0], b = indices[index * 3 + 1], c = indices[index * 3 + 2];

                u32 live  = liveTriangles[a] + liveTriangles[b] + liveTriangles[c];
                f32 score = GetDistance(triangles[index].p - corner, false);

                if (live < bestLive || (live == bestLive && score < bestScore))
                {
                    bestSeed  = index;
                    bestLive  = live;
                    bestScore = score;
                }
            }

            return bestSeed;
        }
#endif

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

    u32 MeshUtils::DetermineMaxMeshlets(u64 indexCount, u64 maxVertices, u64 maxTriangles)
    {
        C3D_ASSERT(indexCount % 3 == 0);
        C3D_ASSERT_MSG(maxTriangles % 4 == 0, "Index data is 4b aligned so maxTriangles must be divisible by 4.");

        u64 maxVerticesConservative = maxVertices - 2;
        u64 meshletLimitVertices    = (indexCount + maxVerticesConservative - 1) / maxVerticesConservative;
        u64 meshletLimitTriangles   = (indexCount / 3 + maxTriangles - 1) / maxTriangles;

        return Max(meshletLimitVertices, meshletLimitTriangles);
    }

    u32 MeshUtils::GenerateMeshlets(const Mesh& mesh, DynamicArray<MeshUtils::Meshlet>& meshlets, f32 coneWeight)
    {
        ScopedTimer timer(String::FromFormat("Generating meshlets for: {}", mesh.name));

        u64 vertexCount = mesh.vertices.Size();
        u64 indexCount  = mesh.indices.Size();
        u64 faceCount   = indexCount / 3;

        C3D_ASSERT(indexCount % 3 == 0);

        Meshlet meshlet;

        // index of the vertex in the meshlet, 0xff if the vertex isn't used
        u8* used = Memory.Allocate<u8>(MemoryType::Array, vertexCount);
        std::memset(used, -1, vertexCount);

        size_t offset = 0;

        for (size_t i = 0; i < indexCount; i += 3)
        {
            u32 a = mesh.indices[i + 0], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
            C3D_ASSERT(a < vertexCount && b < vertexCount && c < vertexCount);

            u8& av = used[a];
            u8& bv = used[b];
            u8& cv = used[c];

            u32 usedExtra = (av == 0xff) + (bv == 0xff) + (cv == 0xff);

            if (meshlet.vertexCount + usedExtra > MESHLET_MAX_VERTICES || meshlet.triangleCount >= MESHLET_MAX_TRIANGLES)
            {
                meshlets[offset++] = meshlet;

                for (size_t j = 0; j < meshlet.vertexCount; ++j) used[meshlet.vertices[j]] = 0xff;

                std::memset(&meshlet, 0, sizeof(meshlet));
            }

            if (av == 0xff)
            {
                av = meshlet.vertexCount;

                meshlet.vertices[meshlet.vertexCount++] = a;
            }

            if (bv == 0xff)
            {
                bv = meshlet.vertexCount;

                meshlet.vertices[meshlet.vertexCount++] = b;
            }

            if (cv == 0xff)
            {
                cv = meshlet.vertexCount;

                meshlet.vertices[meshlet.vertexCount++] = c;
            }

            meshlet.indices[meshlet.triangleCount][0] = av;
            meshlet.indices[meshlet.triangleCount][1] = bv;
            meshlet.indices[meshlet.triangleCount][2] = cv;
            meshlet.triangleCount++;
        }

        if (meshlet.triangleCount) meshlets[offset++] = meshlet;

        C3D_ASSERT(offset <= DetermineMaxMeshlets(indexCount, MESHLET_MAX_VERTICES, MESHLET_MAX_TRIANGLES));

        Memory.Free(used);

        return offset;

#if 0
            TriangleAdjacency adjacency = {};
            if (vertexCount > indexCount && indexCount < (1u << 31))
            {
                BuildTriangleAdjacencySparse(adjacency, mesh.indices.GetData(), indexCount, vertexCount);
            }
            else
            {
                BuildTriangleAdjacency(adjacency, mesh.indices.GetData(), indexCount, vertexCount);
            }

            // Live triangle counts; note, we alias adjacency.counts as we remove triangles after emitting them so the counts always match
            u32* liveTriangles = adjacency.counts;

            u8* emittedFlags = Memory.Allocate<u8>(MemoryType::Array, faceCount);

            // For each triangle, precompute centroid & normal to use for scoring
            Cone* triangles = Memory.Allocate<Cone>(MemoryType::Array, faceCount);

            f32 meshArea = ComputeTriangleCones(triangles, mesh.indices, mesh.vertices);

            // Assuming each meshlet is a square patch, expected radius is sqrt(expected area)
            float triangleAreaAvg       = faceCount == 0 ? 0.f : meshArea / static_cast<f32>(faceCount) * 0.5f;
            float meshletExpectedRadius = std::sqrtf(triangleAreaAvg * MESHLET_MAX_TRIANGLES) * 0.5f;

            // Build a kd-tree for nearest neighbor lookup
            u32* kdindices = Memory.Allocate<u32>(MemoryType::Array, faceCount);
            for (size_t i = 0; i < faceCount; ++i)
            {
                kdindices[i] = static_cast<u32>(i);
            }

            KDNode* nodes = Memory.Allocate<KDNode>(MemoryType::Array, faceCount * 2);
            KDTreeBuild(0, nodes, faceCount * 2, &triangles[0].p, sizeof(Cone) / sizeof(vec3), kdindices, faceCount, /* leaf_size= */ 8);

            // Find a specific corner of the mesh to use as a starting point for meshlet flow
            vec3 corner = vec3(FLT_MAX);
            for (size_t i = 0; i < faceCount; ++i)
            {
                const Cone& tri = triangles[i];
                corner.x        = Min(corner.x, tri.p.x);
                corner.y        = Min(corner.y, tri.p.y);
                corner.z        = Min(corner.z, tri.p.z);
            }

            // Index of the vertex in the meshlet, -1 if the vertex isn't used
            i16* used = Memory.Allocate<i16>(MemoryType::Array, vertexCount);
            std::memset(used, -1, vertexCount * sizeof(i16));

            // Initial seed triangle is the one closest to the corner
            u32 initialSeed  = INVALID_ID;
            f32 initialScore = FLT_MAX;

            for (u64 i = 0; i < faceCount; ++i)
            {
                const Cone& tri = triangles[i];
                float score     = GetDistance(tri.p - corner, false);

                if (initialSeed == INVALID_ID || score < initialScore)
                {
                    initialSeed  = static_cast<u32>(i);
                    initialScore = score;
                }
            }

            // Seed triangles to continue meshlet flow
            u32 seeds[K_MESHLET_MAX_SEEDS] = {};
            u64 seedCount                  = 0;

            MeshUtils::Meshlet meshlet = {};
            u64 meshletOffset          = 0;

            Cone meshletConeAcc = {};

            for (;;)
            {
                Cone meshletCone = GetMeshletCone(meshletConeAcc, meshlet.triangleCount);

                u32 bestTriangle = INVALID_ID;

                // For the first triangle, we don't have a meshlet cone yet, so we use the initial seed
                // to continue the meshlet, we select an adjacent triangle based on connectivity and spatial scoring
                if (meshletOffset == 0 && meshlet.triangleCount == 0)
                {
                    bestTriangle = initialSeed;
                }
                else
                {
                    bestTriangle = GetNeighborTriangle(meshlet, meshletCone, meshletVertices, mesh.indices, adjacency, triangles, liveTriangles, used,
                                                       meshletExpectedRadius, coneWeight);
                }

                bool split = false;

                // When we run out of adjacent triangles we need to switch to spatial search; we currently just pick the closest triangle irrespective of
                // connectivity
                if (bestTriangle == INVALID_ID)
                {
                    u32 index    = INVALID_ID;
                    f32 distance = FLT_MAX;

                    KDTreeNearest(nodes, 0, &triangles[0].p, emittedFlags, meshletCone.p, coneWeight < 0.f, index, distance);

                    bestTriangle = index;
                    split        = meshlet.triangleCount >= MESHLET_MAX_TRIANGLES && distance > meshletExpectedRadius * 0.0f;
                }

                if (bestTriangle == INVALID_ID) break;

                i32 bestExtra = (used[mesh.indices[bestTriangle * 3 + 0]] < 0) + (used[mesh.indices[bestTriangle * 3 + 1]] < 0) +
                                (used[mesh.indices[bestTriangle * 3 + 2]] < 0);

                // If the best triangle doesn't fit into current meshlet, we re-select using seeds to maintain global flow
                if (split || (meshlet.vertexCount + bestExtra > MESHLET_MAX_VERTICES || meshlet.triangleCount >= MESHLET_MAX_TRIANGLES))
                {
                    seedCount = PruneSeedTriangles(seeds, seedCount, emittedFlags);
                    seedCount = (seedCount + K_MESHLET_ADD_SEEDS <= K_MESHLET_MAX_SEEDS) ? seedCount : K_MESHLET_MAX_SEEDS - K_MESHLET_ADD_SEEDS;
                    seedCount += AppendSeedTriangles(seeds + seedCount, meshlet, meshletVertices, mesh.indices, adjacency, triangles, liveTriangles, corner);

                    u32 bestSeed = SelectSeedTriangle(seeds, seedCount, mesh.indices, triangles, liveTriangles, corner);

                    // We may not find a valid seed triangle if the mesh is disconnected as seeds are based on adjacency
                    bestTriangle = bestSeed != INVALID_ID ? bestSeed : bestTriangle;
                }

                u32 a = mesh.indices[bestTriangle * 3 + 0], b = mesh.indices[bestTriangle * 3 + 1], c = mesh.indices[bestTriangle * 3 + 2];
                C3D_ASSERT(a < vertexCount && b < vertexCount && c < vertexCount);

                // Add meshlet to the output; when the current meshlet is full we reset the accumulated bounds
                if (AppendMeshlet(meshlet, a, b, c, used, meshlets, meshletVertices, meshletTriangles, meshletOffset, MESHLET_MAX_VERTICES,
                                  MESHLET_MAX_TRIANGLES, split))
                {
                    meshletOffset++;
                    std::memset(&meshletConeAcc, 0, sizeof(meshletConeAcc));
                }

                // Remove emitted triangle from adjacency data
                // this makes sure that we spend less time traversing these lists on subsequent iterations
                // live triangle counts are updated as a byproduct of these adjustments
                for (u64 k = 0; k < 3; ++k)
                {
                    u32 index = mesh.indices[bestTriangle * 3 + k];

                    u32* neighbors    = &adjacency.data[0] + adjacency.offsets[index];
                    u64 neighborsSize = adjacency.counts[index];

                    for (u64 i = 0; i < neighborsSize; ++i)
                    {
                        u32 tri = neighbors[i];

                        if (tri == bestTriangle)
                        {
                            neighbors[i] = neighbors[neighborsSize - 1];
                            adjacency.counts[index]--;
                            break;
                        }
                    }
                }

                // Update aggregated meshlet cone data for scoring subsequent triangles
                meshletConeAcc.p += triangles[bestTriangle].p;
                meshletConeAcc.n += triangles[bestTriangle].n;

                C3D_ASSERT(!emittedFlags[bestTriangle]);
                emittedFlags[bestTriangle] = 1;
            }

            if (meshlet.triangleCount)
            {
                FinishMeshlet(meshlet, meshletTriangles);

                meshlets[meshletOffset++] = meshlet;
            }

            C3D_ASSERT(meshletOffset <= DetermineMaxMeshlets(indexCount, MESHLET_MAX_VERTICES, MESHLET_MAX_TRIANGLES));

            // Pad out the number of meshlets to a number divisible by 32
            // TODO: We don't really need this but it insures we can assume we always need 32 meshlets in the task shader
            while (meshletOffset % 32) meshletOffset++;

            INFO_LOG("Estimated: {} meshlets, actual meshlets: {}", meshlets.Size(), meshletOffset);

            Memory.Free(emittedFlags);
            Memory.Free(triangles);
            Memory.Free(kdindices);
            Memory.Free(nodes);
            Memory.Free(used);

            adjacency.Destroy();

            return meshletOffset;
#endif
    }

    MeshletBounds MeshUtils::GenerateMeshletBounds(const Mesh& mesh, const Meshlet& meshlet)
    {
        u32 indices[MESHLET_MAX_TRIANGLES * 3];

        u64 vertexCount = mesh.vertices.Size();
        u64 indexCount  = meshlet.triangleCount * 3;

        for (u32 i = 0; i < meshlet.triangleCount; ++i)
        {
            u32 a = meshlet.vertices[meshlet.indices[i][0]];
            u32 b = meshlet.vertices[meshlet.indices[i][1]];
            u32 c = meshlet.vertices[meshlet.indices[i][2]];

            C3D_ASSERT(a < vertexCount && b < vertexCount && c < vertexCount);

            indices[i * 3 + 0] = a;
            indices[i * 3 + 1] = b;
            indices[i * 3 + 2] = c;
        }

        // Compute triangle normals and gather triangle corners
        vec3 normals[256];
        vec3 corners[256][3];
        u32 triangles = 0;

        for (u32 i = 0; i < indexCount; i += 3)
        {
            u32 a = indices[i + 0], b = indices[i + 1], c = indices[i + 2];

            const vec3& p0 = mesh.vertices[a].pos;
            const vec3& p1 = mesh.vertices[b].pos;
            const vec3& p2 = mesh.vertices[c].pos;

            vec3 p10 = p1 - p0;
            vec3 p20 = p2 - p0;

            vec3 normal = glm::cross(p10, p20);
            f32 area    = glm::length(normal);

            // No need to include degenerate triangles - they will be invisible anyway
            if (area == 0.f) continue;

            // Record triangle normals & corners for future use; normal and corner 0 define a plane equation
            normals[triangles]    = normal / area;
            corners[triangles][0] = p0;
            corners[triangles][1] = p1;
            corners[triangles][2] = p2;
            triangles++;
        }

        // Compute cluster bounding sphere; we'll use the center to determine normal cone apex as well
        vec4 psphere = {};
        ComputeBoundingSphere(psphere, corners[0], triangles * 3);

        vec3 center = { psphere.x, psphere.y, psphere.z };

        // Treating triangle normals as points, find the bounding sphere - the sphere center determines the optimal cone axis
        vec4 nsphere = {};
        ComputeBoundingSphere(nsphere, normals, triangles);

        vec3 axis         = { nsphere.x, nsphere.y, nsphere.z };
        f32 axislength    = glm::length(axis);
        f32 invaxislength = axislength == 0.f ? 0.f : 1.f / axislength;

        axis *= invaxislength;

        // Compute a tight cone around all normals, mindp = cos(angle/2)
        f32 mindp = 1.f;

        for (u32 i = 0; i < triangles; ++i)
        {
            f32 dp = glm::dot(normals[i], axis);
            mindp  = Min(dp, mindp);
        }

        // Prepare bounds with the bounding sphere; note that below we can return bounds without cone information for degenerate cones
        MeshletBounds bounds = {};

        bounds.center = center;
        bounds.radius = psphere.w;

        // Degenerate cluster, no valid triangles or normal cone is larger than a hemisphere
        // note that if mindp is positive but close to 0, the triangle intersection code below gets less stable
        // we arbitrarily decide that if a normal cone is ~168 degrees wide or more, the cone isn't useful
        if (triangles == 0 || mindp <= 0.1f)
        {
            bounds.coneCutoff = 1;
            return bounds;
        }

        f32 maxt = 0;

        // We need to find the point on center-t*axis ray that lies in negative half-space of all triangles
        for (u32 i = 0; i < triangles; ++i)
        {
            // dot(center-t*axis-corner, trinormal) = 0
            // dot(center-corner, trinormal) - t * dot(axis, trinormal) = 0

            vec3 c = center - corners[i][0];

            float dc = glm::dot(c, normals[i]);
            float dn = glm::dot(axis, normals[i]);

            // dn should be larger than mindp cutoff above
            C3D_ASSERT(dn > 0.f);
            f32 t = dc / dn;

            maxt = Max(t, maxt);
        }

        // Note: this axis is the axis of the normal cone, but our test for perspective camera effectively negates the axis
        bounds.coneAxis = axis;

        // Cos(a) for normal cone is mindp; we need to add 90 degrees on both sides and invert the cone
        // which gives us -cos(a+90) = -(-sin(a)) = sin(a) = sqrt(1 - cos^2(a))
        bounds.coneCutoff = Sqrt(1 - mindp * mindp);

        // Quantize axis & cutoff to 8-bit SNORM format
        bounds.coneAxisS8[0] = static_cast<i8>(QuantizeSnorm(bounds.coneAxis.x, 8));
        bounds.coneAxisS8[1] = static_cast<i8>(QuantizeSnorm(bounds.coneAxis.y, 8));
        bounds.coneAxisS8[2] = static_cast<i8>(QuantizeSnorm(bounds.coneAxis.z, 8));

        // For the 8-bit test to be conservative, we need to adjust the cutoff by measuring the max. error
        f32 coneAxisS8E0 = Abs(bounds.coneAxisS8[0] / 127.f - bounds.coneAxis.x);
        f32 coneAxisS8E1 = Abs(bounds.coneAxisS8[1] / 127.f - bounds.coneAxis.y);
        f32 coneAxisS8E2 = Abs(bounds.coneAxisS8[2] / 127.f - bounds.coneAxis.z);

        // Note that we need to round this up instead of rounding to nearest, hence +1
        i32 coneCutoffS8 = static_cast<i32>(127 * (bounds.coneCutoff + coneAxisS8E0 + coneAxisS8E1 + coneAxisS8E2) + 1);

        bounds.coneCutoffS8 = (coneCutoffS8 > 127) ? 127 : static_cast<i8>(coneCutoffS8);

        return bounds;

#if 0
// compute triangle normals and gather triangle corners
        vec3 normals[MESHLET_MAX_TRIANGLES];
        vec3 corners[MESHLET_MAX_TRIANGLES][3];
        u64 triangles = 0;

        for (u64 i = 0; i < meshlet.triangleCount * 3; i += 3)
        {
            u32 a = meshletVertices[meshlet.vertexOffset + i + 0];
            u32 b = meshletVertices[meshlet.vertexOffset + i + 1];
            u32 c = meshletVertices[meshlet.vertexOffset + i + 2];

            const vec3& p0 = mesh.vertices[a].pos;
            const vec3& p1 = mesh.vertices[b].pos;
            const vec3& p2 = mesh.vertices[c].pos;

            vec3 p10 = p1 - p0;
            vec3 p20 = p2 - p0;

            vec3 normal = glm::cross(p10, p20);
            f32 area    = glm::length(normal);

            // No need to include degenerate triangles - they will be invisible anyway
            if (area == 0.f) continue;

            // Record triangle normals & corners for future use; normal and corner 0 define a plane equation
            normals[triangles]    = normal / area;
            corners[triangles][0] = p0;
            corners[triangles][1] = p1;
            corners[triangles][2] = p2;
            triangles++;
        }

        MeshletBounds bounds = {};

        // Degenerate cluster, no valid triangles => trivial reject (cone data is 0)
        if (triangles == 0) return bounds;

        // Compute cluster bounding sphere; we'll use the center to determine normal cone apex as well
        vec4 psphere = {};
        ComputeBoundingSphere(psphere, corners[0], triangles * 3);

        vec3 center = { psphere.x, psphere.y, psphere.z };

        // Treating triangle normals as points, find the bounding sphere - the sphere center determines the optimal cone axis
        vec4 nsphere = {};
        ComputeBoundingSphere(nsphere, normals, triangles);

        vec3 axis         = { nsphere.x, nsphere.y, nsphere.z };
        f32 axislength    = glm::length(axis);
        f32 invaxislength = axislength == 0.f ? 0.f : 1.f / axislength;

        axis *= invaxislength;

        // Compute a tight cone around all normals, mindp = cos(angle/2)
        f32 mindp = 1.f;

        for (u64 i = 0; i < triangles; ++i)
        {
            f32 dp = glm::dot(normals[i], axis);
            mindp  = Min(mindp, dp);
        }

        // Fill bounding sphere info; note that below we can return bounds without cone information for degenerate cones
        bounds.center = center;
        bounds.radius = psphere.w;

        // Degenerate cluster, normal cone is larger than a hemisphere => trivial accept
        // note that if mindp is positive but close to 0, the triangle intersection code below gets less stable
        // we arbitrarily decide that if a normal cone is ~168 degrees wide or more, the cone isn't useful
        static int jandino = 0;
        if (mindp <= 0.1f)
        {
            INFO_LOG("jandino = {}", ++jandino);

            bounds.coneCutoff = 1;
            return bounds;
        }

        f32 maxt = 0;

        // We need to find the point on center-t*axis ray that lies in negative half-space of all triangles
        for (u64 i = 0; i < triangles; ++i)
        {
            // dot(center-t*axis-corner, trinormal) = 0
            // dot(center-corner, trinormal) - t * dot(axis, trinormal) = 0
            vec3 c = center - corners[i][0];

            f32 dc = glm::dot(c, normals[i]);
            f32 dn = glm::dot(axis, normals[i]);

            // dn should be larger than mindp cutoff above
            C3D_ASSERT(dn > 0.f);
            f32 t = dc / dn;

            maxt = (t > maxt) ? t : maxt;
        }

        // Cone apex should be in the negative half-space of all cluster triangles by construction
        bounds.coneApex = center - axis * maxt;

        // NOTE: this axis is the axis of the normal cone, but our test for perspective camera effectively negates the axis
        bounds.coneAxis = axis;

        // Cos(a) for normal cone is mindp; we need to add 90 degrees on both sides and invert the cone
        // which gives us -cos(a+90) = -(-sin(a)) = sin(a) = sqrt(1 - cos^2(a))
        bounds.coneCutoff = Sqrt(1 - mindp * mindp);

        return bounds;
#endif
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