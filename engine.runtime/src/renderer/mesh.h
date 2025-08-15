
#pragma once
#include "containers/dynamic_array.h"
#include "defines.h"
#include "resources/types.h"
#include "vertex.h"

namespace C3D
{
    struct Mesh final : IResource
    {
        Mesh() : IResource(ResourceType::Mesh) {}

        DynamicArray<Vertex> vertices;
        DynamicArray<u32> indices;
    };
}  // namespace C3D