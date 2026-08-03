#version 460

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_nonuniform_qualifier: require

#include "definitions.h"

#define RAYTRACE 1

#if RAYTRACE
#extension GL_EXT_ray_query: require

layout (constant_id = 2) const int POST = 0;

layout (binding = 7) uniform accelerationStructureEXT tlas;
#endif

layout (push_constant) uniform block
{
    Globals globals;
};

layout (binding = 1) readonly buffer Draws
{
    MeshDraw draws[];
};

layout (location = 0) out vec4 outputColor;

layout (location = 0) in flat uint drawId;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec4 tangent;
layout (location = 4) in vec3 wpos;

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

    float ndotl = max(dot(nrm, globals.sunDirection), 0.0);

#if RAYTRACE
    rayQueryEXT rq;
    rayQueryInitializeEXT(rq, tlas, gl_RayFlagsTerminateOnFirstHitEXT, 0xff, wpos, 1e-2f, globals.sunDirection, 100);
    rayQueryProceedEXT(rq);

    ndotl *= (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionNoneEXT) ? 1.0 : 0.05;
#endif

    outputColor = vec4(albedo.rgb * sqrt(ndotl + 0.05) + emissive, albedo.a);

    if (POST > 0 && albedo.a < 0.5)
    {
        discard;
    }
}