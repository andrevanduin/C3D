
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
        struct Hasher
        {
            const u8* data = nullptr;
            u64 size       = 0;
            u64 stride     = 0;

            u64 hash(u32 index) const
            {
                // MurmurHash2
                const u32 m = 0x5bd1e995;
                const i32 r = 24;

                u32 h   = 0;
                u64 len = size;

                const u8* key = data + index * stride;

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

            bool equal(u32 lhs, u32 rhs) const { return std::memcmp(data + lhs * stride, data + rhs * stride, size) == 0; }
        };

        struct VertexScoreTable
        {
            f32 cache[1 + K_CACHE_SIZE_MAX];
            f32 live[1 + K_VALENCE_MAX];
        };

        enum VertexKind : u8
        {
            Manifold,  // Not on an attribute seam, not on any boundary
            Border,    // Not on an attribute seam, has exactly two open edges
            Seam,      // On an attribute seam with exactly two attribute seam edges
            Locked,    // None of the above; these vertices can't move
            Count
        };

        // Nanifold vertices can collapse on anything except locked
        // border/seam vertices can only be collapsed onto border/seam respectively
        constexpr u8 K_CAN_COLLAPSE[VertexKind::Count][VertexKind::Count] = {
            { 1, 1, 1, 1 },
            { 0, 1, 0, 0 },
            { 0, 0, 1, 0 },
            { 0, 0, 0, 0 },
        };

        // If a vertex is manifold or seam, adjoining edges are guaranteed to have an opposite edge
        // note that for seam edges, the opposite edge isn't present in the attribute-based topology
        // but is present if you consider a position-only mesh variant
        constexpr u8 K_HAS_OPPOSITE[VertexKind::Count][VertexKind::Count] = {
            { 1, 1, 1, 1 },
            { 1, 0, 1, 0 },
            { 1, 1, 1, 1 },
            { 1, 0, 1, 0 },
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

        struct Adjacency
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

        struct Quadric
        {
            f32 a00;
            f32 a10, a11;
            f32 a20, a21, a22;
            f32 b0, b1, b2, c;

            Quadric& operator+=(const Quadric& other)
            {
                a00 += other.a00;
                a10 += other.a10;
                a11 += other.a11;
                a20 += other.a20;
                a21 += other.a21;
                a22 += other.a22;
                b0 += other.b0;
                b1 += other.b1;
                b2 += other.b2;
                c += other.c;
                return *this;
            }

            Quadric& operator*=(f32 scalar)
            {
                a00 *= scalar;
                a10 *= scalar;
                a11 *= scalar;
                a20 *= scalar;
                a21 *= scalar;
                a22 *= scalar;
                b0 *= scalar;
                b1 *= scalar;
                b2 *= scalar;
                c *= scalar;
                return *this;
            }

            f32 Error(const vec3& v) const
            {
                f32 rx = b0;
                f32 ry = b1;
                f32 rz = b2;

                rx += a10 * v.y;
                ry += a21 * v.z;
                rz += a20 * v.x;

                rx *= 2;
                ry *= 2;
                rz *= 2;

                rx += a00 * v.x;
                ry += a11 * v.y;
                rz += a22 * v.z;

                f32 r = c;
                r += rx * v.x;
                r += ry * v.y;
                r += rz * v.z;

                return Abs(r);
            }
        };

        struct Collapse
        {
            u32 v0;
            u32 v1;

            union {
                u32 bidi;
                f32 error;
                u32 errorui;
            };
        };

        void ComputeBoundingSphere(vec4& result, const vec3* points, u64 count)
        {
            // Find extremum points along all axes; for each axis we get a pair of points with min/max coordinates
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
            while (buckets < count) buckets *= 2;

            return buckets;
        }

        u32* HashLookup(DynamicArray<u32>& table, const Hasher& hasher, u32 key)
        {
            u64 buckets = table.Size();

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

        void BuildTriangleAdjacency(Adjacency& adjacency, u32* indices, u64 indexCount, u64 vertexCount)
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

        void BuildEdgeAdjacency(Adjacency& adjacency, u32* indices, u64 indexCount, u64 vertexCount)
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

            // Fill edge data
            for (u64 i = 0; i < faceCount; ++i)
            {
                u32 a = indices[i * 3 + 0], b = indices[i * 3 + 1], c = indices[i * 3 + 2];

                adjacency.data[adjacency.offsets[a]++] = static_cast<u32>(b);
                adjacency.data[adjacency.offsets[b]++] = static_cast<u32>(c);
                adjacency.data[adjacency.offsets[c]++] = static_cast<u32>(a);
            }

            // Fix offsets that have been disturbed by the previous pass
            for (u64 i = 0; i < vertexCount; ++i)
            {
                C3D_ASSERT(adjacency.offsets[i] >= adjacency.counts[i]);

                adjacency.offsets[i] -= adjacency.counts[i];
            }
        }

        void BuildTriangleAdjacencySparse(Adjacency& adjacency, u32* indices, u64 indexCount, u64 vertexCount)
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

        void BuildPositionRemap(DynamicArray<u32>& remap, DynamicArray<u32>& wedge, const DynamicArray<Vertex>& vertices)
        {
            Hasher hasher;
            hasher.data   = reinterpret_cast<const u8*>(vertices.GetData());
            hasher.size   = sizeof(vec3);
            hasher.stride = sizeof(Vertex);

            u64 vertexCount = vertices.Size();

            u64 tableSize = HashBuckets(vertexCount);
            DynamicArray<u32> table(tableSize);
            table.Fill(INVALID_ID);

            // Build forward remap: for each vertex, which other (canonical) vertex does it map to?
            // We use position equivalence for this, and remap vertices to other existing vertices
            for (u32 i = 0; i < vertexCount; ++i)
            {
                u32 index  = i;
                u32* entry = HashLookup(table, hasher, index);

                if (*entry == INVALID_ID)
                {
                    *entry = index;

                    remap[index] = index;
                }
                else
                {
                    C3D_ASSERT(remap[*entry] != INVALID_ID);

                    remap[index] = remap[*entry];
                }
            }

            // Build wedge table: for each vertex, which other vertex is the next wedge that also maps to the same vertex?
            // Entries in table form a (cyclic) wedge loop per vertex; for manifold vertices, wedge[i] == remap[i] == i
            for (u32 i = 0; i < vertexCount; ++i)
            {
                wedge[i] = i;
            }

            for (u64 i = 0; i < vertexCount; ++i)
            {
                if (remap[i] != i)
                {
                    u32 r = remap[i];

                    wedge[i] = wedge[r];
                    wedge[r] = static_cast<u32>(i);
                }
            }
        }

        bool HasEdge(const Adjacency& adjacency, u32 a, u32 b)
        {
            u32 count       = adjacency.counts[a];
            const u32* data = adjacency.data + adjacency.offsets[a];

            for (u32 i = 0; i < count; ++i)
            {
                if (data[i] == b)
                {
                    return true;
                }
            }

            return false;
        }

        u32 FindWedgeEdge(const Adjacency& adjacency, const DynamicArray<u32>& wedge, u32 a, u32 b)
        {
            u32 v = a;

            do
            {
                if (HasEdge(adjacency, v, b))
                {
                    return v;
                }

                v = wedge[v];
            } while (v != a);

            return INVALID_ID;
        }

        u64 CountOpenEdges(const Adjacency& adjacency, u32 vertex, u32* last = nullptr)
        {
            u64 result = 0;

            u32 count       = adjacency.counts[vertex];
            const u32* data = adjacency.data + adjacency.offsets[vertex];

            for (u64 i = 0; i < count; ++i)
            {
                if (!HasEdge(adjacency, data[i], vertex))
                {
                    result++;

                    if (last)
                    {
                        *last = data[i];
                    }
                }
            }

            return result;
        }

        void ClassifyVertices(DynamicArray<VertexKind>& result, DynamicArray<u32>& loop, u64 vertexCount, const Adjacency& adjacency,
                              const DynamicArray<u32>& remap, const DynamicArray<u32>& wedge)
        {
            loop.Fill(INVALID_ID);

            for (u64 i = 0; i < vertexCount; ++i)
            {
                if (remap[i] == i)
                {
                    if (wedge[i] == i)
                    {
                        // No attribute seam, need to check if it's manifold
                        u32 v     = 0;
                        u64 edges = CountOpenEdges(adjacency, static_cast<u32>(i), &v);

                        // Note: we classify any vertices with no open edges as manifold
                        // This is technically incorrect - if 4 triangles share an edge, we'll classify vertices as manifold
                        // it's unclear if this is a problem in practice.
                        // Also note that we classify vertices as border if they have *one* open edge, not two, this is because we only have
                        // half-edges - so a border vertex would have one incoming and one outgoing edge
                        if (edges == 0)
                        {
                            result[i] = VertexKind::Manifold;
                        }
                        else if (edges == 1)
                        {
                            result[i] = VertexKind::Border;
                            loop[i]   = v;
                        }
                        else
                        {
                            result[i] = VertexKind::Locked;
                        }
                    }
                    else if (wedge[wedge[i]] == i)
                    {
                        // Attribute seam; need to distinguish between Seam and Locked
                        u32 a      = 0;
                        u64 aCount = CountOpenEdges(adjacency, static_cast<u32>(i), &a);
                        u32 b      = 0;
                        u64 bCount = CountOpenEdges(adjacency, wedge[i], &b);

                        // seam should have one open half-edge for each vertex, and the edges need to "connect" - point to the same vertex post-remap
                        if (aCount == 1 && bCount == 1)
                        {
                            u32 ao = FindWedgeEdge(adjacency, wedge, a, wedge[i]);
                            u32 bo = FindWedgeEdge(adjacency, wedge, b, static_cast<u32>(i));

                            if (ao != INVALID_ID && bo != INVALID_ID)
                            {
                                result[i] = VertexKind::Seam;

                                loop[i]        = a;
                                loop[wedge[i]] = b;
                            }
                            else
                            {
                                result[i] = VertexKind::Locked;
                            }
                        }
                        else
                        {
                            result[i] = VertexKind::Locked;
                        }
                    }
                    else
                    {
                        // More than one vertex maps to this one; we don't have classification available
                        result[i] = VertexKind::Locked;
                    }
                }
                else
                {
                    C3D_ASSERT(remap[i] < i);

                    result[i] = result[remap[i]];
                }
            }
        }

        void RescalePositions(DynamicArray<vec3>& result, const DynamicArray<Vertex>& vertexData, u64 vertexCount)
        {
            vec3 minv = vec3(FLT_MAX);
            vec3 maxv = vec3(-FLT_MAX);

            for (u64 i = 0; i < vertexCount; ++i)
            {
                const auto& vertex = vertexData[i];

                result[i] = vertex.pos;

                minv = glm::min(minv, vertex.pos);
                maxv = glm::max(maxv, vertex.pos);
            }

            f32 extent = 0.f;

            extent = (maxv.x - minv.x) < extent ? extent : (maxv.x - minv.x);
            extent = (maxv.y - minv.y) < extent ? extent : (maxv.y - minv.y);
            extent = (maxv.z - minv.z) < extent ? extent : (maxv.z - minv.z);

            f32 scale = extent == 0 ? 0.f : 1.f / extent;

            for (u64 i = 0; i < vertexCount; ++i)
            {
                result[i] = (result[i] - minv) * scale;
            }
        }

        void QuadricFromPlane(Quadric& Q, f32 a, f32 b, f32 c, f32 d)
        {
            Q.a00 = a * a;
            Q.a10 = b * a;
            Q.a11 = b * b;
            Q.a20 = c * a;
            Q.a21 = c * b;
            Q.a22 = c * c;
            Q.b0  = d * a;
            Q.b1  = d * b;
            Q.b2  = d * c;
            Q.c   = d * d;
        }

        void QuadricFromTriangle(Quadric& Q, const vec3& p0, const vec3& p1, const vec3& p2)
        {
            vec3 p10 = p1 - p0;
            vec3 p20 = p2 - p0;

            vec3 normal = glm::cross(p10, p20);
            f32 length  = glm::length(normal);
            if (length > 0)
            {
                normal /= length;
            }

            float distance = glm::dot(normal, p0);

            QuadricFromPlane(Q, normal.x, normal.y, normal.z, -distance);
            Q *= length;
        }

        void QuadricFromTriangleEdge(Quadric& Q, const vec3& p0, const vec3& p1, const vec3& p2, f32 weight)
        {
            vec3 p10   = p1 - p0;
            f32 length = glm::length(p10);
            if (length > 0)
            {
                p10 /= length;
            }

            vec3 p20 = p2 - p0;
            f32 p20p = glm::dot(p20, p10);

            vec3 normal = glm::cross(p20, p10);
            glm::normalize(normal);

            f32 distance = glm::dot(normal, p0);

            QuadricFromPlane(Q, normal.x, normal.y, normal.z, -distance);

            Q *= (length * length * weight);
        }

        void FillFaceQuadrics(DynamicArray<Quadric>& vertexQuadrics, const DynamicArray<u32>& indices, const DynamicArray<vec3>& vertexPositions,
                              const DynamicArray<u32>& remap)
        {
            u64 indexCount = indices.Size();
            for (u64 i = 0; i < indexCount; i += 3)
            {
                u32 i0 = indices[i + 0];
                u32 i1 = indices[i + 1];
                u32 i2 = indices[i + 2];

                Quadric Q;
                QuadricFromTriangle(Q, vertexPositions[i0], vertexPositions[i1], vertexPositions[i2]);

                vertexQuadrics[remap[i0]] += Q;
                vertexQuadrics[remap[i1]] += Q;
                vertexQuadrics[remap[i2]] += Q;
            }
        }

        void FillEdgeQuadrics(DynamicArray<Quadric>& vertexQuadrics, const DynamicArray<u32>& indices, const DynamicArray<vec3>& vertexPositions,
                              const DynamicArray<u32>& remap, const DynamicArray<VertexKind>& vertexKind, const DynamicArray<u32>& loop)
        {
            constexpr u32 next[3] = { 1, 2, 0 };

            u64 indexCount = indices.Size();
            for (u64 i = 0; i < indexCount; i += 3)
            {
                for (u32 e = 0; e < 3; ++e)
                {
                    u32 i0 = indices[i + e];
                    u32 i1 = indices[i + next[e]];

                    VertexKind k0 = vertexKind[i0];
                    VertexKind k1 = vertexKind[i1];

                    // Check that i0 and i1 are border/seam and are on the same edge loop
                    // loop[] tracks half edges so we only need to check i0->i1
                    if (k0 != k1 || (k0 != VertexKind::Border && k0 != VertexKind::Seam) || loop[i0] != i1)
                    {
                        continue;
                    }

                    u32 i2 = indices[i + next[next[e]]];

                    // We try hard to maintain border edge geometry; seam edges can move more freely
                    // due to topological restrictions on collapses, seam quadrics slightly improves collapse structure but aren't critical
                    constexpr f32 kEdgeWeightSeam   = 1.f;
                    constexpr f32 kEdgeWeightBorder = 10.f;

                    f32 edgeWeight = (k0 == VertexKind::Seam) ? kEdgeWeightSeam : kEdgeWeightBorder;

                    Quadric Q;
                    QuadricFromTriangleEdge(Q, vertexPositions[i0], vertexPositions[i1], vertexPositions[i2], edgeWeight);

                    vertexQuadrics[remap[i0]] += Q;
                    vertexQuadrics[remap[i1]] += Q;
                }
            }
        }

        u64 PickEdgeCollapses(DynamicArray<Collapse>& collapses, const DynamicArray<u32>& indices, u64 indexCount, const DynamicArray<u32>& remap,
                              const DynamicArray<VertexKind>& vertexKind, const DynamicArray<u32>& loop)
        {
            u64 collapseCount = 0;

            constexpr i32 next[3] = { 1, 2, 0 };

            for (u64 i = 0; i < indexCount; i += 3)
            {
                for (i32 e = 0; e < 3; ++e)
                {
                    u32 i0 = indices[i + e];
                    u32 i1 = indices[i + next[e]];

                    // This can happen either when input has a zero-length edge, or when we perform collapses for complex
                    // topology w/seams and collapse a manifold vertex that connects to both wedges onto one of them
                    // we leave edges like this alone since they may be important for preserving mesh integrity
                    if (remap[i0] == remap[i1])
                    {
                        continue;
                    }

                    VertexKind k0 = vertexKind[i0];
                    VertexKind k1 = vertexKind[i1];

                    // The edge has to be collapsible in at least one direction
                    if (!(K_CAN_COLLAPSE[k0][k1] | K_CAN_COLLAPSE[k1][k0]))
                    {
                        continue;
                    }

                    // Manifold and seam edges should occur twice (i0->i1 and i1->i0) - skip redundant edges
                    if (K_HAS_OPPOSITE[k0][k1] && remap[i1] > remap[i0])
                    {
                        continue;
                    }

                    // Two vertices are on a border or a seam, but there's no direct edge between them
                    // this indicates that they belong to two different edge loops and we should not collapse this edge
                    // loop[] tracks half edges so we only need to check i0->i1
                    if (k0 == k1 && (k0 == VertexKind::Border || k0 == VertexKind::Seam) && loop[i0] != i1) continue;

                    // Edge can be collapsed in either direction - we will pick the one with minimum error
                    // note: we evaluate error later during collapse ranking, here we just tag the edge as bidirectional
                    if (K_CAN_COLLAPSE[k0][k1] & K_CAN_COLLAPSE[k1][k0])
                    {
                        Collapse c = { i0, i1, { /* bidi= */ 1 } };

                        collapses[collapseCount++] = c;
                    }
                    else
                    {
                        // edge can only be collapsed in one direction
                        u32 e0 = K_CAN_COLLAPSE[k0][k1] ? i0 : i1;
                        u32 e1 = K_CAN_COLLAPSE[k0][k1] ? i1 : i0;

                        Collapse c = { e0, e1, { /* bidi= */ 0 } };

                        collapses[collapseCount++] = c;
                    }
                }
            }

            return collapseCount;
        }

        void RankEdgeCollapses(DynamicArray<Collapse>& collapses, u64 collapseCount, const DynamicArray<vec3>& vertexPositions,
                               const DynamicArray<Quadric>& vertexQuadrics, const DynamicArray<u32>& remap)
        {
            for (u64 i = 0; i < collapseCount; ++i)
            {
                Collapse& c = collapses[i];

                u32 i0 = c.v0;
                u32 i1 = c.v1;

                // Most edges are bidirectional which means we need to evaluate errors for two collapses
                // to keep this code branchless we just use the same edge for unidirectional edges
                u32 j0 = c.bidi ? i1 : i0;
                u32 j1 = c.bidi ? i0 : i1;

                f32 ei = vertexQuadrics[remap[i0]].Error(vertexPositions[i1]);
                f32 ej = vertexQuadrics[remap[j0]].Error(vertexPositions[j1]);

                // Pick edge direction with minimal error
                c.v0    = ei <= ej ? i0 : j0;
                c.v1    = ei <= ej ? i1 : j1;
                c.error = ei <= ej ? ei : ej;
            }
        }

        void SortEdgeCollapses(DynamicArray<u32>& sortOrder, const DynamicArray<Collapse>& collapses, u64 collapseCount)
        {
            constexpr i32 sortBits = 11;

            // Fill histogram for counting sort
            u32 histogram[1 << sortBits];
            std::memset(histogram, 0, sizeof(histogram));

            for (u64 i = 0; i < collapseCount; ++i)
            {
                // Skip sign bit since error is non-negative
                u32 key = (collapses[i].errorui << 1) >> (32 - sortBits);

                histogram[key]++;
            }

            // Compute offsets based on histogram data
            u64 histogramSum = 0;
            for (u64 i = 0; i < 1 << sortBits; ++i)
            {
                u64 count    = histogram[i];
                histogram[i] = static_cast<u32>(histogramSum);
                histogramSum += count;
            }

            C3D_ASSERT(histogramSum == collapseCount);

            // Compute sort order based on offsets
            for (u64 i = 0; i < collapseCount; ++i)
            {
                // Skip sign bit since error is non-negative
                u32 key = (collapses[i].errorui << 1) >> (32 - sortBits);

                sortOrder[histogram[key]++] = static_cast<u32>(i);
            }
        }

        u64 PerformEdgeCollapses(DynamicArray<u32>& collapseRemap, DynamicArray<u8>& collapseLocked, DynamicArray<Quadric>& vertexQuadrics,
                                 const DynamicArray<Collapse>& collapses, u64 collapseCount, const DynamicArray<u32>& collapseOrder,
                                 const DynamicArray<u32>& remap, const DynamicArray<u32>& wedge, const DynamicArray<VertexKind>& vertexKind,
                                 u64 triangleCollapseGoal, f32 errorLimit)
        {
            u64 edgeCollapses     = 0;
            u64 triangleCollapses = 0;

            for (u64 i = 0; i < collapseCount; ++i)
            {
                const Collapse& c = collapses[collapseOrder[i]];

                if (c.error > errorLimit)
                {
                    break;
                }

                if (triangleCollapses >= triangleCollapseGoal)
                {
                    break;
                }

                u32 r0 = remap[c.v0];
                u32 r1 = remap[c.v1];

                // We don't collapse vertices that had source or target vertex involved in a collapse
                // it's important to not move the vertices twice since it complicates the tracking/remapping logic
                // it's important to not move other vertices towards a moved vertex to preserve error since we don't re-rank collapses mid-pass
                if (collapseLocked[r0] | collapseLocked[r1])
                {
                    continue;
                }

                C3D_ASSERT(collapseRemap[r0] == r0);
                C3D_ASSERT(collapseRemap[r1] == r1);

                vertexQuadrics[r1] += vertexQuadrics[r0];

                if (vertexKind[c.v0] == VertexKind::Seam)
                {
                    // remap v0 to v1 and seam pair of v0 to seam pair of v1
                    u32 s0 = wedge[c.v0];
                    u32 s1 = wedge[c.v1];

                    C3D_ASSERT(s0 != c.v0 && s1 != c.v1);
                    C3D_ASSERT(wedge[s0] == c.v0 && wedge[s1] == c.v1);

                    collapseRemap[c.v0] = c.v1;
                    collapseRemap[s0]   = s1;
                }
                else
                {
                    C3D_ASSERT(wedge[c.v0] == c.v0);

                    collapseRemap[c.v0] = c.v1;
                }

                collapseLocked[r0] = 1;
                collapseLocked[r1] = 1;

                // border edges collapse 1 triangle, other edges collapse 2 or more
                triangleCollapses += (vertexKind[c.v0] == VertexKind::Border) ? 1 : 2;
                edgeCollapses++;
            }

            return edgeCollapses;
        }

        u64 RemapIndexBuffer(DynamicArray<u32>& indices, u64 indexCount, const DynamicArray<u32>& collapseRemap)
        {
            u64 write = 0;

            for (size_t i = 0; i < indexCount; i += 3)
            {
                u32 v0 = collapseRemap[indices[i + 0]];
                u32 v1 = collapseRemap[indices[i + 1]];
                u32 v2 = collapseRemap[indices[i + 2]];

                // We never move the vertex twice during a single pass
                C3D_ASSERT(collapseRemap[v0] == v0);
                C3D_ASSERT(collapseRemap[v1] == v1);
                C3D_ASSERT(collapseRemap[v2] == v2);

                if (v0 != v1 && v0 != v2 && v1 != v2)
                {
                    indices[write + 0] = v0;
                    indices[write + 1] = v1;
                    indices[write + 2] = v2;
                    write += 3;
                }
            }

            return write;
        }

        void RemapEdgeLoops(DynamicArray<u32>& loop, u64 vertexCount, const DynamicArray<u32>& collapseRemap)
        {
            for (u64 i = 0; i < vertexCount; ++i)
            {
                if (loop[i] != INVALID_ID)
                {
                    u32 l = loop[i];
                    u32 r = collapseRemap[l];

                    // i == r is a special case when the seam edge is collapsed in a direction opposite to where loop goes
                    loop[i] = (i == r) ? loop[l] : r;
                }
            }
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

    u32 MeshUtils::GenerateMeshlets(const DynamicArray<u32>& indices, u64 vertexCount, DynamicArray<MeshUtils::Meshlet>& meshlets, f32 coneWeight)
    {
        u64 indexCount = indices.Size();
        u64 faceCount  = indexCount / 3;

        C3D_ASSERT(indexCount % 3 == 0);

        Meshlet meshlet;

        // index of the vertex in the meshlet, 0xff if the vertex isn't used
        u8* used = Memory.Allocate<u8>(MemoryType::Array, vertexCount);
        std::memset(used, -1, vertexCount);

        size_t offset = 0;

        for (size_t i = 0; i < indexCount; i += 3)
        {
            u32 a = indices[i + 0], b = indices[i + 1], c = indices[i + 2];
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
    }

    MeshletBounds MeshUtils::GenerateMeshletBounds(const Meshlet& meshlet, const DynamicArray<Vertex>& vertices)
    {
        u32 indices[MESHLET_MAX_TRIANGLES * 3];

        u64 vertexCount = vertices.Size();
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

            const vec3& p0 = vertices[a].pos;
            const vec3& p1 = vertices[b].pos;
            const vec3& p2 = vertices[c].pos;

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
    }

    u32 MeshUtils::GenerateVertexRemap(const DynamicArray<Vertex>& vertices, u32 indexCount, DynamicArray<u32>& outRemap)
    {
        Hasher hasher = {};
        hasher.data   = reinterpret_cast<const u8*>(vertices.GetData());
        hasher.size   = sizeof(Vertex);
        hasher.stride = sizeof(Vertex);

        u32 vertexCount = vertices.Size();

        // Remap table will be at most vertexCount large and we fill it with SLOT_EMPTY
        outRemap.Resize(vertexCount);
        outRemap.Fill(SLOT_EMPTY);

        // Determine the size we need for our hash table
        u64 hashTableSize = HashBuckets(vertexCount);
        // Create our hashtable
        DynamicArray<u32> table(hashTableSize);
        // Initialize all entries as empty
        table.Fill(SLOT_EMPTY);

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

            u32* entry = HashLookup(table, hasher, i);
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

        C3D_ASSERT_MSG(nextVertex <= vertexCount, "The remapped number of vertices must be <= the original number of vertices");

        return nextVertex;
    }

    void MeshUtils::RemapVertices(MeshAsset& mesh, u32 indexCount, const DynamicArray<Vertex>& vertices, const DynamicArray<u32>& remap)
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

    void MeshUtils::RemapIndices(MeshAsset& mesh, u32 indexCount, const DynamicArray<u32>& remap)
    {
        for (u32 i = 0; i < indexCount; ++i)
        {
            C3D_ASSERT(remap[i] != SLOT_EMPTY);

            mesh.indices[i] = remap[i];
        }
    }

    void MeshUtils::OptimizeForVertexCache(MeshAsset& mesh) { OptimizeForVertexCache(mesh.indices, mesh.indices, mesh.vertices.Size()); }

    void MeshUtils::OptimizeForVertexCache(DynamicArray<u32>& destination, const DynamicArray<u32>& originalIndices, u64 vertexCount)
    {
        C3D_ASSERT(originalIndices.Size() % 3 == 0);

        // Guard for empty meshes
        if (vertexCount == 0 || originalIndices.Empty()) return;

        u64 indexCount = originalIndices.Size();
        u64 faceCount  = indexCount / 3;

        u32* indices = originalIndices.GetData();

        // Support in-place optimization
        if (indices == destination.GetData())
        {
            indices = Memory.Allocate<u32>(MemoryType::Array, originalIndices.Size());
            std::memcpy(indices, originalIndices.GetData(), sizeof(u32) * originalIndices.Size());
        }

        // Build adjacency information
        Adjacency adjacency = {};
        BuildTriangleAdjacency(adjacency, indices, indexCount, vertexCount);

        // Live triangle counts; note, we alias adjacency.counts as we remove triangles after emitting them so the counts always match
        u32* liveTriangles = adjacency.counts;

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
            destination[outputTriangle * 3 + 0] = a;
            destination[outputTriangle * 3 + 1] = b;
            destination[outputTriangle * 3 + 2] = c;
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

    void MeshUtils::OptimizeForVertexFetch(MeshAsset& mesh)
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

    u32 MeshUtils::Simplify(DynamicArray<u32>& destination, const DynamicArray<u32>& indices, const DynamicArray<Vertex>& vertices, u64 targetIndexCount,
                            f32 targetError)
    {
        u64 indexCount  = indices.Size();
        u64 vertexCount = vertices.Size();

        C3D_ASSERT(indexCount % 3 == 0);
        C3D_ASSERT(targetIndexCount <= indexCount);

        // Build adjacency information
        Adjacency adjacency = {};
        BuildEdgeAdjacency(adjacency, indices.GetData(), indexCount, vertexCount);

        // Build position remap that maps each vertex to the one with identical position
        DynamicArray<u32> remap(vertexCount);
        DynamicArray<u32> wedge(vertexCount);
        BuildPositionRemap(remap, wedge, vertices);

        // Classify vertices; vertex kind determines collapse rules, see kCanCollapse
        DynamicArray<VertexKind> vertexKind(vertexCount);
        DynamicArray<u32> loop(vertexCount);
        ClassifyVertices(vertexKind, loop, vertexCount, adjacency, remap, wedge);

        DynamicArray<vec3> vertexPositions(vertexCount);
        RescalePositions(vertexPositions, vertices, vertexCount);

        DynamicArray<Quadric> vertexQuadrics(vertexCount);
        std::memset(vertexQuadrics.GetData(), 0, vertexCount * sizeof(Quadric));

        FillFaceQuadrics(vertexQuadrics, indices, vertexPositions, remap);
        FillEdgeQuadrics(vertexQuadrics, indices, vertexPositions, remap, vertexKind, loop);

        if (destination.GetData() != indices.GetData())
        {
            C3D_ASSERT_MSG(destination.Size() >= indices.Size(), "Provided destination is too small");
            std::memcpy(destination.GetData(), indices.GetData(), sizeof(u32) * indices.Size());
        }

        DynamicArray<Collapse> edgeCollapses(indexCount);
        DynamicArray<u32> collapseOrder(indexCount);
        DynamicArray<u32> collapseRemap(vertexCount);
        DynamicArray<u8> collapseLocked(vertexCount);

        u64 resultCount = indexCount;

        while (resultCount > targetIndexCount)
        {
            u64 edgeCollapseCount = PickEdgeCollapses(edgeCollapses, destination, resultCount, remap, vertexKind, loop);

            // No edges can be collapsed any more due to topology restrictions
            if (edgeCollapseCount == 0)
            {
                break;
            }

            RankEdgeCollapses(edgeCollapses, edgeCollapseCount, vertexPositions, vertexQuadrics, remap);

            SortEdgeCollapses(collapseOrder, edgeCollapses, edgeCollapseCount);

            // Most collapses remove 2 triangles; use this to establish a bound on the pass in terms of error limit
            // note that edge_collapse_goal is an estimate; triangle_collapse_goal will be used to actually limit collapses
            u64 triangleCollapseGoal = (resultCount - targetIndexCount) / 3;
            u64 edgeCollapseGoal     = triangleCollapseGoal / 2;

            // We limit the error in each pass based on the error of optimal last collapse; since many collapses will be locked
            // as they will share vertices with other successfull collapses, we need to increase the acceptable error by this factor
            constexpr f32 kPassErrorBound = 1.5f;

            f32 errorGoal  = edgeCollapseGoal < edgeCollapseCount ? edgeCollapses[collapseOrder[edgeCollapseGoal]].error * kPassErrorBound : FLT_MAX;
            f32 errorLimit = errorGoal > targetError ? targetError : errorGoal;

            for (u64 i = 0; i < vertexCount; ++i)
            {
                collapseRemap[i] = static_cast<u32>(i);
            }
            collapseLocked.Fill(0);

            u64 collapses = PerformEdgeCollapses(collapseRemap, collapseLocked, vertexQuadrics, edgeCollapses, edgeCollapseCount, collapseOrder, remap, wedge,
                                                 vertexKind, triangleCollapseGoal, errorLimit);

            // No edges can be collapsed any more due to hitting the error limit or triangle collapse limit
            if (collapses == 0)
            {
                break;
            }

            RemapEdgeLoops(loop, vertexCount, collapseRemap);

            u64 newCount = RemapIndexBuffer(destination, resultCount, collapseRemap);
            C3D_ASSERT(newCount < resultCount);

            resultCount = newCount;
        }

        adjacency.Destroy();

        return resultCount;
    }

}  // namespace C3D