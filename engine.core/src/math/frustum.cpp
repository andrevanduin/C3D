
#include "frustum.h"

namespace C3D
{
    Frustum::Frustum(const vec3& position, const vec3& forward, const vec3& right, const vec3& up, f32 nearClip, f32 farClip, f32 fov,
                     f32 aspectRatio)
    {
        const f32 halfV       = farClip * tanf(fov * 0.5f);
        const f32 halfH       = halfV * aspectRatio;
        const vec3 forwardFar = forward * farClip;
        const vec3 rightHalfH = right * halfH;
        const vec3 upHalfV    = up * halfV;

        sides[FrustumPlaneTop]    = Plane3D((forward * nearClip) + position, forward);
        sides[FrustumPlaneBottom] = Plane3D(position + forwardFar, -1.0f * forward);
        sides[FrustumPlaneRight]  = Plane3D(position, glm::cross(up, forwardFar + rightHalfH));
        sides[FrustumPlaneLeft]   = Plane3D(position, glm::cross(forwardFar - rightHalfH, up));
        sides[FrustumPlaneFar]    = Plane3D(position, glm::cross(right, forwardFar - upHalfV));
        sides[FrustumPlaneNear]   = Plane3D(position, glm::cross(forwardFar + upHalfV, right));
    }

    Frustum::Frustum(const mat4& viewProjection)
    {
        mat4 inv = glm::inverse(viewProjection);

        vec4 mat0 = { inv[0][0], inv[0][1], inv[0][2], inv[0][3] };
        vec4 mat1 = { inv[1][0], inv[1][1], inv[1][2], inv[1][3] };
        vec4 mat2 = { inv[2][0], inv[2][1], inv[2][2], inv[2][3] };
        vec4 mat3 = { inv[3][0], inv[3][1], inv[3][2], inv[3][3] };

        sides[FrustumPlaneLeft]   = Plane3D(glm::normalize(mat3 + mat0));
        sides[FrustumPlaneRight]  = Plane3D(glm::normalize(mat3 - mat0));
        sides[FrustumPlaneTop]    = Plane3D(glm::normalize(mat3 - mat1));
        sides[FrustumPlaneBottom] = Plane3D(glm::normalize(mat3 + mat1));
        sides[FrustumPlaneNear]   = Plane3D(glm::normalize(mat3 + mat2));
        sides[FrustumPlaneFar]    = Plane3D(glm::normalize(mat3 - mat2));
    }

    bool Frustum::IntersectsWithSphere(const Sphere& sphere) const
    {
        for (auto& side : sides)
        {
            if (!side.IntersectsWithSphere(sphere))
            {
                return false;
            }
        }
        return true;
    }

    bool Frustum::IntersectsWithAABB(const AABB& aabb) const
    {
        for (const auto& side : sides)
        {
            if (!side.IntersectsWithAABB(aabb))
            {
                return false;
            }
        }
        return true;
    }

    void FrustumCornerPointsInWorldSpace(const mat4& projectionView, vec4* corners)
    {
        mat4 inverseViewProjection = glm::inverse(projectionView);

        corners[0] = vec4(-1.0f, -1.0f, 0.0f, 1.0f);
        corners[1] = vec4(1.0f, -1.0f, 0.0f, 1.0f);
        corners[2] = vec4(1.0f, 1.0f, 0.0f, 1.0f);
        corners[3] = vec4(-1.0f, 1.0f, 0.0f, 1.0f);

        corners[4] = vec4(-1.0f, -1.0f, 1.0f, 1.0f);
        corners[5] = vec4(1.0f, -1.0f, 1.0f, 1.0f);
        corners[6] = vec4(1.0f, 1.0f, 1.0f, 1.0f);
        corners[7] = vec4(-1.0f, 1.0f, 1.0f, 1.0f);

        for (u32 i = 0; i < 8; ++i)
        {
            vec4 point = corners[i] * inverseViewProjection;
            corners[i] = point / point.w;
        }
    }
}  // namespace C3D
