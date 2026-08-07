
#pragma once
#include "defines.h"
#include "math/math_types.h"

namespace C3D
{
    struct alignas(16) Material
    {
        /** @brief Texture index for the albedo texture. */
        u32 albedoIndex = 0;
        /** @brief Texture index for the normal texture. */
        u32 normalIndex = 0;
        /** @brief Texture index for the specular texture. */
        u32 specularIndex = 0;
        /** @brief Texture index for the emissive texture. */
        u32 emissiveIndex = 0;

        /** @brief The diffuse factor used by this material. */
        vec4 diffuseFactor = vec4(1.0);
        /** @brief The diffuse factor used by this material. */
        vec4 specularFactor;
        /** @brief The diffuse factor used by this material. */
        vec3 emissiveFactor;
    };
}  // namespace C3D