
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
        vec3 coneApex;
        vec3 coneAxis;
        f32 coneCutoff; /* = cos(angle/2) */
    };

    struct alignas(16) Meshlet
    {
        /** @brief A cone used to determine if this meshlet can be backface culled on the GPU. */
        vec4 cone;
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

    struct alignas(16) MeshDraw
    {
        vec2 offset;
        vec2 scale;
    };
}  // namespace C3D