
#pragma once
#include "assets/types.h"
#include "containers/dynamic_array.h"
#include "defines.h"
#include "vertex.h"

namespace C3D
{
    constexpr auto MESHLET_MAX_VERTICES  = 64;
    constexpr auto MESHLET_MAX_TRIANGLES = 124;

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
        /** @brief The number of triangles in this Meshlet. */
        u8 triangleCount = 0;
        /** @brief The number of vertices in this Meshlet. */
        u8 vertexCount = 0;
    };

    /** @brief A collection of vertices and indices that together make up some renderable geometry. */
    struct Mesh final : IAsset
    {
        Mesh() : IAsset(AssetType::Mesh) {}

        DynamicArray<Vertex> vertices;
        DynamicArray<u32> indices;
        DynamicArray<Meshlet> meshlets;

        /** @brief This array hold the data need for the meshlets corresponding to this mesh.
         * The data is laid out as follows for each meshlet:
         * meshlet.vertexCount       * u32 for the vertex indices
         * meshlet.triangleCount / 4 * u32 for the triangle indices
         * Since our array is u32's and our triangle indices are all u8's we have to divide by 4
         */
        DynamicArray<u32> meshletData;
    };
}  // namespace C3D