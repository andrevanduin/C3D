
#pragma once
#include "defines.h"
#include "math_types.h"

namespace C3D
{
    struct Disc3D
    {
        vec3 center;
        vec3 normal;
        f32 outerRadius;
        f32 innerRadius;
    };
}  // namespace C3D