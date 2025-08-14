
#pragma once
#include "defines.h"
#include "math/math_types.h"

namespace C3D
{
    class C3D_API Plane3D
    {
    public:
        Plane3D() = default;
        Plane3D(const vec3& p1, const vec3& norm);
        Plane3D(const vec4& normalizedSide);

        [[nodiscard]] f32 SignedDistance(const vec3& position) const;

        [[nodiscard]] bool IntersectsWithSphere(const Sphere& sphere) const;

        [[nodiscard]] bool IntersectsWithAABB(const AABB& aabb) const;

        vec3 normal  = vec3(0.0f);
        f32 distance = 0.0f;
    };
}  // namespace C3D