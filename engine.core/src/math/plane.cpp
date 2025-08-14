
#include "plane.h"

#include <glm/glm.hpp>

#include "c3d_math.h"

namespace C3D
{
    Plane3D::Plane3D(const vec3& p1, const vec3& norm) : normal(normalize(norm)) { distance = glm::dot(normal, p1); }

    Plane3D::Plane3D(const vec4& normalizedSide) : normal(normalizedSide), distance(normalizedSide.w) {}

    f32 Plane3D::SignedDistance(const vec3& position) const { return glm::dot(normal, position) - distance; }

    bool Plane3D::IntersectsWithSphere(const Sphere& sphere) const { return SignedDistance(sphere.center) > -sphere.radius; }

    bool Plane3D::IntersectsWithAABB(const AABB& aabb) const
    {
        const f32 r = aabb.extents.x * Abs(normal.x) + aabb.extents.y * Abs(normal.y) + aabb.extents.z * Abs(normal.z);
        return -r <= SignedDistance(aabb.center);
    }
}  // namespace C3D