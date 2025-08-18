
#pragma once
#include "defines.h"
#include "math/math_types.h"

namespace C3D
{
    struct Vertex
    {
        /** @brief The position of the vertex. These are halfs (2-byte floats) to save some space. */
        u16 vx, vy, vz;
        /** @brief The normal of the vertex. */
        u8 nx, ny, nz, nw;
        /** @brief The texture coordinates (u, v) of the vertex. */
        u16 tx, ty;
    };
}  // namespace C3D