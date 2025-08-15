
#pragma once
#include "defines.h"
#include "math/math_types.h"

namespace C3D
{
    struct Vertex
    {
        /** @brief The position of the vertex. */
        vec3 position;
        /** @brief The normal of the vertex. */
        vec3 normal;
        /** @brief The texture coordinates (u, v) of the vertex. */
        vec2 texture;
    };
}  // namespace C3D