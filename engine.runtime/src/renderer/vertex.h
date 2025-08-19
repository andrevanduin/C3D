
#pragma once
#include "defines.h"
#include "math/math_types.h"

namespace C3D
{
    struct Vertex
    {
        /** @brief The position of the vertex. Halfs (2-byte floats) to save some space. */
        u16 vx, vy, vz, vw;
        /** @brief The normal of the vertex. */
        u8 nx, ny, nz, nw;
        /** @brief The texture coordinates (u, v) (again halfs to save space). */
        u16 tx, ty;
    };

    constexpr auto jan = sizeof(Vertex);
}  // namespace C3D