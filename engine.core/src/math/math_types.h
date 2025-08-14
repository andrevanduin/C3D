
#pragma once
#include <fmt/format.h>

#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/transform.hpp>

#include "defines.h"

typedef glm::vec2 vec2;
typedef glm::ivec2 ivec2;
typedef glm::uvec2 uvec2;
typedef glm::u16vec2 u16vec2;

typedef glm::vec3 vec3;
typedef glm::ivec3 ivec3;
typedef glm::uvec3 uvec3;

typedef glm::vec4 vec4;
typedef glm::ivec4 ivec4;
typedef glm::uvec4 uvec4;

typedef glm::mat4 mat4;

typedef glm::quat quat;

namespace C3D
{
    struct Extents2D
    {
        vec2 min = { 0, 0 };
        vec2 max = { 0, 0 };
    };

    struct Extents3D
    {
        vec3 min = { 0, 0, 0 };
        vec3 max = { 0, 0, 0 };
    };

    struct Bounds
    {
        Bounds() {}
        Bounds(f32 x, f32 y, f32 width, f32 height) : x(x), y(y), width(width), height(height) {}

        bool Contains(f32 xPos, f32 yPos) const { return xPos >= x && xPos <= x + width && yPos >= y && yPos <= y + height; }

        bool Contains(const vec2& point) const { return point.x >= x && point.x <= x + width && point.y >= y && point.y <= y + height; }

        f32 x      = 0.0f;
        f32 y      = 0.0f;
        f32 width  = 0.0f;
        f32 height = 0.0f;
    };

    struct Sphere
    {
        vec3 center;
        f32 radius;
    };

    struct AABB
    {
        vec3 center;
        vec3 extents;
    };

    struct Rect2D
    {
        /** @brief The X coordinate of the rectangle. */
        f32 x;
        /** @brief The Y coordinate of the rectangle. */
        f32 y;
        /** @brief The width of the rectangle. */
        f32 width;
        /** @brief The height of the rectangle. */
        f32 height;

        /**
         * @brief Checks if the provided point is inside of this rectangle
         *
         * @param point The point we want to check against
         * @return true if point is inside, false otherwise
         */
        bool PointIsInside(const vec2& point) const
        {
            return point.x >= x && point.x <= x + width && point.y >= y && point.y <= y + height;
        }
    };

    struct Circle2D
    {
        /** @brief The center point of the circle. */
        vec2 center;
        /** @brief The radius of the circle. */
        f32 radius;

        /**
         * @brief Checks if the provided point is inside of this circle
         *
         * @param point The point we want to check against
         * @return true if point is inside, false otherwise
         */
        bool PointIsInside(const vec2& point)
        {
            f32 rSquared      = radius * radius;
            vec2 distance     = point - center;
            f32 lengthSquared = distance.x * distance.x + distance.y * distance.y;
            return lengthSquared <= rSquared;
        }
    };
}  // namespace C3D

template <>
struct fmt::formatter<vec2>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(vec2 const& vector, FormatContext& ctx)
    {
        return fmt::format_to(ctx.out(), "{}, {}", vector.x, vector.y);
    }
};

template <>
struct fmt::formatter<vec3>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(vec3 const& vector, FormatContext& ctx)
    {
        return fmt::format_to(ctx.out(), "{}, {}, {}", vector.x, vector.y, vector.z);
    }
};

template <>
struct fmt::formatter<vec4>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(vec4 const& vector, FormatContext& ctx)
    {
        return fmt::format_to(ctx.out(), "{}, {}, {}, {}", vector.x, vector.y, vector.z, vector.w);
    }
};