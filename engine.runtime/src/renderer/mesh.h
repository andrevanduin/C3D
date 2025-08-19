
#pragma once
#include "assets/types.h"
#include "containers/dynamic_array.h"
#include "defines.h"
#include "vertex.h"

namespace C3D
{
    constexpr auto MESHLET_MAX_VERTICES  = 64;
    constexpr auto MESHLET_MAX_TRIANGLES = 126;

    struct Meshlet
    {
        u32 vertices[MESHLET_MAX_VERTICES];
        u8 indices[MESHLET_MAX_TRIANGLES * 3];
        u8 triangleCount = 0;
        u8 vertexCount   = 0;
    };

    /** @brief A collection of vertices and indices that together make up some renderable geometry. */
    struct Mesh final : IAsset
    {
        Mesh() : IAsset(AssetType::Mesh) {}

        DynamicArray<Vertex> vertices;
        DynamicArray<u32> indices;
        DynamicArray<Meshlet> meshlets;
    };
}  // namespace C3D