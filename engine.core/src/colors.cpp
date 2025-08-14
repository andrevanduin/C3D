
#include "colors.h"

#include "random/random.h"

namespace C3D
{
    u32 RGB::ToU32() const
    {
        auto red   = (static_cast<u32>(r * 255) & 0x0FF) << 16;
        auto green = (static_cast<u32>(g * 255) & 0x0FF) << 8;
        auto blue  = (static_cast<u32>(b * 255) & 0x0FF);
        return red | green | blue;
    }

    vec3 RGB::ToVec3() const { return { static_cast<f32>(r) / 255.0f, static_cast<f32>(g) / 255.0f, static_cast<f32>(b) / 255.0f }; }

    RGBA HSV::ToRGBA() const
    {
        RGBA rgba(0, 0, 0, 1);

        f32 c = s * v;
        f32 x = c * (1 - Abs(Mod(h * 6.0f, 2.0f) - 1));
        f32 m = v - c;

        if (h >= 0.0f && h < HSV_60)
        {
            rgba.r = c;
            rgba.g = x;
            rgba.b = 0;
        }
        else if (h >= HSV_60 && h < HSV_120)
        {
            rgba.r = x;
            rgba.g = c;
            rgba.b = 0;
        }
        else if (h >= HSV_120 && h < HSV_180)
        {
            rgba.r = 0;
            rgba.g = c;
            rgba.b = x;
        }
        else if (h >= HSV_180 && h < HSV_240)
        {
            rgba.r = 0;
            rgba.g = x;
            rgba.b = c;
        }
        else if (h >= HSV_240 && h < HSV_300)
        {
            rgba.r = x;
            rgba.g = 0;
            rgba.b = c;
        }
        else
        {
            rgba.r = c;
            rgba.g = 0;
            rgba.b = x;
        }

        return rgba;
    }

    u32 RgbToU32(u32 r, u32 g, u32 b) { return (r & 0x0FF) << 16 | (g & 0x0FF) << 8 | (b & 0x0FF); }
}  // namespace C3D