
#pragma once
#include "defines.h"
#include "math/math_types.h"

namespace C3D
{
    struct Vertex
    {
        /** @brief The position of the vertex. */
        vec3 pos;
        /** @brief The normal of the vertex. */
        u8 nx, ny, nz, nw;
        /** @brief The texture coordinates (u, v) (Halfs to save space). */
        u16 tx, ty;
    };

    constexpr auto jan = sizeof(Vertex);
}  // namespace C3D