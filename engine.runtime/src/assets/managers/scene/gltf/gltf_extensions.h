
#pragma once
#include "containers/dynamic_array.h"
#include "defines.h"
#include "gltf_asset_types.h"
#include "gltf_extension.h"
#include "string/string.h"

namespace C3D
{
    constexpr auto KHR_MATERIALS_TRANSMISSION = "KHR_materials_transmission";
    struct GLTFTransmissionExtension
    {
        f32 transmissionFactor = 0.0f;
        GLTFTextureInfo transmissionTexture;
    };

    constexpr auto KHR_MATERIALS_PBR_SPECULAR_GLOSSINESS = "KHR_materials_pbrSpecularGlossiness";
    struct GLTFPBRSpecularGlossinessExtension
    {
        GLTFTextureInfo diffuseTexture;
        GLTFTextureInfo specularGlossinessTexture;

        f32 glossinessFactor  = 1.0f;
        f32 specularFactor[3] = { 1.0f, 1.0f, 1.0f };
        f32 diffuseFactor[4]  = { 1.0f, 1.0f, 1.0f, 1.0f };
    };

    constexpr auto KHR_LIGHTS_PUNCTUAL = "KHR_lights_punctual";
    struct GLTFNodeLightsPunctualExtension
    {
        u32 light;
    };

    enum class GLTFLightPunctualType
    {
        Directional,
        Point,
        Spot,
    };

    struct GLTFLightPunctual
    {
        String name;
        GLTFLightPunctualType type;

        f32 intensity = 1.0f;
        f32 range     = 0.f;
        f32 color[3]  = { 1.f, 1.f, 1.f };
    };

    struct GLTFLightsPunctualExtension
    {
        DynamicArray<GLTFLightPunctual> lights;
    };

    constexpr auto MSFT_TEXTURE_DDS = "MSFT_texture_dds";
    struct GLTFTextureDDS
    {
        u32 source;
    };

    void CleanupGLTFExtension(GLTFExtension& extension);
}  // namespace C3D