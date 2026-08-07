#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_nonuniform_qualifier: require

#include "definitions.h"
#include "math.h"

#define DEBUG 0

layout (constant_id = 2) const int POST = 0;

layout (push_constant) uniform block
{
    Globals globals;
};

layout (binding = 1) readonly buffer Draws
{
    MeshDraw draws[];
};

layout (location = 0) out vec4 gBuffer[2];

layout (location = 0) in flat uint drawId;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec4 tangent;
layout (location = 4) in vec3 wpos;

layout (binding = 7) uniform sampler textureSampler;

layout (binding = 8) readonly buffer Materials
{
    Material materials[];
};

layout (binding = 0, set = 1) uniform texture2D textures[];

#define SAMP(id) sampler2D(textures[nonuniformEXT(id)], textureSampler)

uint hash(uint a)
{
   a = (a+0x7ed55d16) + (a<<12);
   a = (a^0xc761c23c) ^ (a>>19);
   a = (a+0x165667b1) + (a<<5);
   a = (a+0xd3a2646c) ^ (a<<9);
   a = (a+0xfd7046c5) + (a<<3);
   a = (a^0xb55a4f09) ^ (a>>16);
   return a;
}

void main()
{
    MeshDraw meshDraw = draws[drawId];
    Material material = materials[meshDraw.materialIndex];

    vec4 albedo = material.diffuseFactor;
    if (material.albedoIndex > 0)
    {
        albedo *= FromSRGB(texture(SAMP(material.albedoIndex), uv));
    }

    vec3 normalMap = vec3(0, 0, 1);
    if (material.normalIndex > 0)
    {
        normalMap = texture(SAMP(material.normalIndex), uv).rgb * 2 - 1;
    }

    vec4 specGloss = material.specularFactor;
    if (material.specularIndex > 0)
    {
        specGloss *= FromSRGB(texture(SAMP(material.specularIndex), uv));
    }

    vec3 emissive = material.emissiveFactor;
    if (material.emissiveIndex > 0)
    {
        emissive *= FromSRGB(texture(SAMP(material.emissiveIndex), uv).rgb);
    }

    vec3 biTangent = cross(normal, tangent.xyz) * tangent.w;
    
    vec3 nrm = normalize(normalMap.r * tangent.xyz + normalMap.g * biTangent + normalMap.b * normal);

    float emissiveF = dot(emissive, vec3(0.3, 0.6, 0.1)) / (dot(albedo.rgb, vec3(0.3, 0.6, 0.1)) + 1e-3);

    // TODO: reconstruct metalness from specular texture
    gBuffer[0] = vec4(ToSRGB(albedo).rgb, log2(1 + emissiveF) / 5);
	gBuffer[1] = vec4(EncodeOct(nrm) * 0.5 + 0.5, specGloss.a, 0.0);

    if (POST > 0 && albedo.a < 0.5)
    {
        discard;
    }

#if DEBUG
    uint mhash = hash(drawId);
    gBuffer[0] = vec4(float(mhash & 255), float((mhash >> 8) & 255), float((mhash >> 16) & 255), 0) / 255.0;
#endif
}