#pragma once
#include "defines.h"
#include "math/c3d_math.h"

namespace C3D
{
    constexpr auto HSV_60  = 60.0f / 360.0f;
    constexpr auto HSV_120 = 120.0f / 360.0f;
    constexpr auto HSV_180 = 180.0f / 360.0f;
    constexpr auto HSV_240 = 240.0f / 360.0f;
    constexpr auto HSV_300 = 300.0f / 360.0f;

    constexpr auto RED     = vec4(1.0f, 0.0f, 0.0f, 1.0f);
    constexpr auto GREEN   = vec4(0.0f, 1.0f, 0.0f, 1.0f);
    constexpr auto BLUE    = vec4(0.0f, 0.0f, 1.0f, 1.0f);
    constexpr auto BLACK   = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    constexpr auto WHITE   = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    constexpr auto GRAY    = vec4(0.5f, 0.5f, 0.5f, 1.0f);
    constexpr auto MAGENTA = vec4(1.0f, 0.0f, 1.0f, 1.0f);
    constexpr auto YELLOW  = vec4(1.0f, 1.0f, 0.0f, 1.0f);

    struct C3D_API RGB
    {
        f32 r = 0.0f;
        f32 g = 0.0f;
        f32 b = 0.0f;

        RGB() = default;
        RGB(u32 rgb) : r(rgb >> 16 & 0x0FF), g(rgb >> 8 & 0x0FF), b(rgb & 0x0FF) {}
        RGB(f32 r, f32 g, f32 b) : r(r), g(g), b(b) {}
        RGB(const vec3& vec) : r(static_cast<u32>(vec.x) * 255), g(static_cast<u32>(vec.y) * 255), b(static_cast<u32>(vec.z) * 255) {}

        u32 ToU32() const;
        vec3 ToVec3() const;
    };

    struct C3D_API RGBA
    {
        f32 r = 0.0f;
        f32 g = 0.0f;
        f32 b = 0.0f;
        f32 a = 0.0f;

        RGBA() = default;
        RGBA(f32 r, f32 g, f32 b, f32 a) : r(r), g(g), b(b), a(a) {}
        RGBA(const RGB& rgb, f32 a) : r(rgb.r), g(rgb.g), b(rgb.b), a(a) {}
    };

    struct C3D_API HSV
    {
        HSV(u32 h, u32 s, u32 v) : h(h / 360.0f), s(s / 100.0f), v(v / 100.0f) {}
        HSV(f32 h, f32 s, f32 v) : h(h), s(s), v(v) {}

        f32 h = 0.0f;
        f32 s = 0.0f;
        f32 v = 0.0f;

        RGBA ToRGBA() const;
    };

    C3D_API u32 RgbToU32(u32 r, u32 g, u32 b);

}  // namespace C3D