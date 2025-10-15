
#pragma once
#include "assets/types.h"
#include "containers/dynamic_array.h"
#include "defines.h"
#include "vertex.h"

namespace C3D
{
    constexpr auto MESHLET_MAX_VERTICES  = 64;
    constexpr auto MESHLET_MAX_TRIANGLES = 96;
    constexpr auto MESHLET_CONE_WEIGHT   = 0.5f;
    constexpr auto TASK_WGLIMIT          = (1 << 22);
    constexpr auto CLUSTER_LIMIT         = (1 << 24);

    /** @brief A collection of vertices and indices that together make up a loaded mesh asset. */
    struct MeshAsset final : IAsset
    {
        MeshAsset() : IAsset(AssetType::Mesh) {}

        /** @brief An array containing the vertices in this mesh. */
        DynamicArray<Vertex> vertices;
        /** @brief An array containing the indices in this mesh. */
        DynamicArray<u32> indices;
    };

    struct MeshletBounds
    {
        /* Bounding sphere, useful for frustum and occlusion culling */
        vec3 center;
        f32 radius;

        /* Normal cone, useful for backface culling */
        vec3 coneAxis;
        i8 coneAxisS8[3];

        f32 coneCutoff; /* = cos(angle/2) */
        i8 coneCutoffS8;
    };

    struct alignas(16) Meshlet
    {
        /** @brief The center of the meshlet. Used for culling. */
        vec3 center;
        /** @brief The radius of the meshlet. Used for culling. */
        f32 radius;
        /** @brief The Cone axis, used for backface culling in the task shader. */
        i8 coneAxis[3];
        /** @brief The cone cutoff. Used in the backface culling test in the task shader. */
        i8 coneCutoff;
        /** @brief An index into the MeshletData array where the data for this meshlet starts.
         * The first elements will be the vertex indices. Then at dataOffset + vertexCount the triangle indices are stored */
        u32 dataOffset = 0;
        /** @brief The number of vertices in this Meshlet. */
        u8 vertexCount = 0;
        /** @brief The number of triangles in this Meshlet. */
        u8 triangleCount = 0;
    };

    struct MeshLod
    {
        /** @brief The offset into the index array for this lod. */
        u32 indexOffset = 0;
        /** @brief The number of indices in this lod. */
        u32 indexCount = 0;
        /** @brief The offset into the meshlet array for this load. */
        u32 meshletOffset = 0;
        /** @brief The number of meshlets in this lod. */
        u32 meshletCount = 0;
        /** @brief The error introduced by this lod. */
        f32 error = 0.f;
    };

    struct alignas(16) Mesh
    {
        /** @brief Bounding sphere center for culling in the compute shader. */
        vec3 center;
        /** @brief Bounding sphere radius for culling in the compute shader. */
        f32 radius;

        /** @brief The offset into the vertex array of the geometry struct for this mesh. */
        u32 vertexOffset = 0;
        /** @brief The number of vertices in this mesh. */
        u32 vertexCount = 0;

        /** @brief The number of lods available for this mesh. */
        u32 lodCount = 0;
        /** @brief An array of lods for this mesh. */
        MeshLod lods[8];
    };

    struct alignas(16) MeshDraw
    {
        vec3 position;
        f32 scale;
        quat orientation;

        u32 meshIndex;
        u32 vertexOffset;
        u32 meshletVisibilityOffset;
    };

    /** @brief A collection of vertices and indices for all meshes that we can render. */
    struct Geometry
    {
        /** @brief This array holds all renderable vertices (of all meshes) */
        DynamicArray<Vertex> vertices;
        /** @brief This array holds all renderable indices (of all meshes) */
        DynamicArray<u32> indices;
        /** @brief This array holds all meshlets (of all meshes) */
        DynamicArray<Meshlet> meshlets;
        /** @brief This array holds the data needed for all the meshlets.
         * The data is laid out as follows for each meshlet:
         * meshlet.vertexCount       x u32 for the vertex indices
         * meshlet.triangleCount / 4 x u32 for the triangle indices
         * Note: since our array contains u32's and our triangle indices are all u8's we have to divide the number of triangles by 4
         */
        DynamicArray<u32> meshletData;
        /** @brief This array holds all the meshes that are part of the renderable geometry. */
        DynamicArray<Mesh> meshes;
    };
}  // namespace C3D