
#pragma once
#include "assets/types.h"
#include "containers/dynamic_array.h"
#include "defines.h"
#include "vertex.h"

namespace C3D
{
    struct Meshlet
    {
        u32 vertices[64];
        // NOTE: Divisible by 3 so good for up to 42 triangles
        u8 indices[126];
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