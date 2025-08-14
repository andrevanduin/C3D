
#pragma once
#include "plane.h"

namespace C3D
{
    class Viewport;

    enum FrustumPlane
    {
        FrustumPlaneTop,
        FrustumPlaneBottom,
        FrustumPlaneRight,
        FrustumPlaneLeft,
        FrustumPlaneFar,
        FrustumPlaneNear,
        FrustumPlaneMax
    };

    struct C3D_API Frustum
    {
        Frustum() = default;
        Frustum(const vec3& position, const vec3& forward, const vec3& right, const vec3& up, f32 nearClip, f32 farClip, f32 fov,
                f32 aspectRatio);
        Frustum(const mat4& viewProjection);

        [[nodiscard]] bool IntersectsWithSphere(const Sphere& sphere) const;

        [[nodiscard]] bool IntersectsWithAABB(const AABB& aabb) const;

        Plane3D sides[FrustumPlaneMax] = {};
    };

    C3D_API void FrustumCornerPointsInWorldSpace(const mat4& projectionView, vec4* corners);
}  // namespace C3D