
#pragma once
#include "defines.h"

namespace C3D
{
    struct Vertex
    {
        /** @brief The position of the vertex. */
        uint16_t vx, vy, vz, vw;
        /** @brief The normal of the vertex. */
        u8 nx, ny, nz, nw;
        /** @brief The tangents of the vertex. */
        u8 tx, ty, tz, tw;
        /** @brief The texture coordinates (u, v) (Halfs to save space). */
        u16 tu, tv;
    };
}  // namespace C3D