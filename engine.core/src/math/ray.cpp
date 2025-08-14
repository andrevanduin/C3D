
#include "ray.h"

#include "c3d_math.h"

namespace
{
    enum Quadrants : u8
    {
        RIGHT      = 0,
        LEFT       = 1,
        MIDDLE     = 2,
        Dimensions = 3,
    };
}

namespace C3D
{
    Ray::Ray(const vec3& position, const vec3& dir) : origin(position), direction(dir) {}

    bool Ray::TestAgainstAABB(const Extents3D& extents, vec3& outPoint) const
    {
        /**
         * @brief Based on Graphics Gems Fast Ray-Box Intersection implementation.
         * See: https://github.com/erich666/GraphicsGems/blob/master/gems/RayBox.c
         */
        bool inside = true;
        i8 quadrant[3];
        vec3 maxT           = {};
        vec3 candidatePlane = {};
        outPoint            = {};

        for (u32 i = 0; i < Dimensions; i++)
        {
            if (origin[i] < extents.min[i])
            {
                quadrant[i]       = LEFT;
                candidatePlane[i] = extents.min[i];
                inside            = false;
            }
            else if (origin[i] > extents.max[i])
            {
                quadrant[i]       = RIGHT;
                candidatePlane[i] = extents.max[i];
                inside            = false;
            }
            else
            {
                quadrant[i] = MIDDLE;
            }
        }

        // Ray origin is inside of our bounding box
        if (inside)
        {
            outPoint = origin;
            return true;
        }

        // Calculate the distances to candidate planes
        for (u32 i = 0; i < Dimensions; i++)
        {
            if (quadrant[i] != MIDDLE && direction[i] != 0.0f)
            {
                maxT[i] = (candidatePlane[i] - origin[i]) / direction[i];
            }
            else
            {
                maxT[i] = -1.0f;
            }
        }

        // Get the largest of the maxTs for final choice of intersection
        i32 whichPlane = 0;
        for (u32 i = 1; i < Dimensions; i++)
        {
            if (maxT[whichPlane] < maxT[i])
            {
                whichPlane = i;
            }
        }

        // Check if the final candidate is actually inside the box
        if (maxT[whichPlane] < 0.0f)
        {
            return false;
        }

        for (u32 i = 0; i < Dimensions; i++)
        {
            if (whichPlane != i)
            {
                outPoint[i] = origin[i] + maxT[whichPlane] * direction[i];
                if (outPoint[i] < extents.min[i] || outPoint[i] > extents.max[i])
                {
                    return false;
                }
            }
            else
            {
                outPoint[i] = candidatePlane[i];
            }
        }

        // This ray hits the box
        return true;
    }

    bool Ray::TestAgainstExtents(const Extents3D& extents, const mat4& model, f32& outDistance) const
    {
        mat4 inverse = glm::inverse(model);

        // Transform the ray to AABB space
        Ray transformedRay;
        transformedRay.origin    = inverse * vec4(origin, 1.0f);
        transformedRay.direction = inverse * vec4(direction, 0.0f);

        vec3 outPoint;
        bool result = transformedRay.TestAgainstAABB(extents, outPoint);

        // If there was a hit, transform the point to the oriented space, then calculate the hit distance
        // based on that transformed position versus the original (untransformed) ray origin
        if (result)
        {
            outPoint    = model * vec4(outPoint, 1.0f);
            outDistance = glm::distance(outPoint, origin);
        }

        return result;
    }

    bool Ray::TestAgainstPlane3D(const Plane3D& plane, vec3& outPoint, f32& outDistance) const
    {
        f32 normalDir   = glm::dot(direction, plane.normal);
        f32 pointNormal = glm::dot(origin, plane.normal);

        // If the ray and plane normal point in the same direction, there can't be a hit
        if (normalDir >= 0.0f)
        {
            return false;
        }

        // Calculate the distance
        f32 t = (plane.distance - pointNormal) / normalDir;

        // Distance must be positive or 0, otherwise the ray hits behind the plane
        // which is technically not a hit at all
        if (t >= 0.0f)
        {
            outDistance = t;
            outPoint    = origin + (direction * t);
            return true;
        }

        return false;
    }

    bool Ray::TestAgainstDisc3D(const Disc3D& disc, vec3& outPoint, f32& outDistance) const
    {
        Plane3D plane = { disc.center, disc.normal };

        if (TestAgainstPlane3D(plane, outPoint, outDistance))
        {
            // Square the radii and compare against squared distance
            f32 outerRadiusSq = disc.outerRadius * disc.outerRadius;
            f32 innerRadiusSq = disc.innerRadius * disc.innerRadius;

            vec3 dist  = disc.center - outPoint;
            f32 distSq = glm::dot(dist, dist);
            if (distSq > outerRadiusSq)
            {
                return false;
            }
            if (innerRadiusSq > 0 && distSq < innerRadiusSq)
            {
                return false;
            }

            return true;
        }

        return false;
    }

    Ray Ray::FromScreen(const vec2& screenPos, const Rect2D& viewportRect, const vec3& origin, const mat4& view, const mat4& projection)
    {
        Ray ray;
        ray.origin = origin;

        // We start with the screen position that is provided
        // First we get normalized device coordinates (-1:1 range that our gpu uses)
        vec3 rayNdc;
        rayNdc.x = (2.0f * (screenPos.x - viewportRect.x)) / viewportRect.width - 1.0f;
        rayNdc.y = 1.0f - (2.0f * (screenPos.y - viewportRect.y)) / viewportRect.height;
        rayNdc.z = 1.0f;

        // Then we get the clip space out of those NDC
        vec4 rayClip = vec4(rayNdc.x, rayNdc.y, -1.0f, 1.0f);

        // Next we get the eye/camera space
        vec4 rayEye = glm::inverse(projection) * rayClip;

        // Unproject xy, change wz to "forward"
        rayEye.z = -1.0f;
        rayEye.w = 0.0f;

        // Convert to world coordinates
        ray.direction = vec3(rayEye * view);
        ray.direction = glm::normalize(ray.direction);

        return ray;
    }
}  // namespace C3D