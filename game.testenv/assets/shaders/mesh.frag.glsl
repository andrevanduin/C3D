#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_nonuniform_qualifier: require

#include "definitions.h"

layout (constant_id = 2) const int POST = 0;

layout (binding = 1) readonly buffer Draws
{
    MeshDraw draws[];
};

layout (location = 0) out vec4 outputColor;

layout (location = 0) in flat uint drawId;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec4 tangent;

layout (binding = 0, set = 1) uniform sampler2D textures[];

void main()
{
    MeshDraw meshDraw = draws[drawId];

    vec4 albedo = vec4(0.5f, 0.5f, 0.5f, 1);
    if (meshDraw.albedoTexture > 0)
    {
        albedo = texture(textures[nonuniformEXT(meshDraw.albedoTexture)], uv);
    }

    vec3 normalMap = vec3(0, 0, 1);
    if (meshDraw.normalTexture > 0)
    {
        normalMap = texture(textures[nonuniformEXT(meshDraw.normalTexture)], uv).rgb * 2 - 1;
    }

    vec3 emissive = vec3(0.0f);
    if (meshDraw.emissiveTexture > 0)
    {
        emissive = texture(textures[nonuniformEXT(meshDraw.emissiveTexture)], uv).rgb;
    }

    vec3 biTangent = cross(normal, tangent.xyz) * tangent.w;
    
    vec3 nrm = normalize(normalMap.r * tangent.xyz + normalMap.g * biTangent + normalMap.b * normal);

    float ndot1 = max(dot(nrm, normalize(vec3(-1, 1, -1))), 0.0);

    outputColor = vec4(albedo.rgb * sqrt(ndot1 + 0.05) + emissive, albedo.a);

    if (POST > 0 && albedo.a < 0.5)
    {
        discard;
    }
}