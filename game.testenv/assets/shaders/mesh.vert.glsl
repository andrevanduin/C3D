#version 450

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_ARB_shader_draw_parameters : require

#include "definitions.h"
#include "math.h"

layout (push_constant) uniform block
{
    Globals globals;
};

layout (binding = 0) readonly buffer DrawCommands
{
    MeshDrawCommand drawCommands[];
};

layout (binding = 1) readonly buffer Draws
{
    MeshDraw draws[];
};

layout (binding = 2) readonly buffer Vertices 
{
    Vertex vertices[];
};

layout (location = 0) out flat uint outDrawId;
layout (location = 1) out vec2 outUv;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec4 outTangent;

void main()
{
    uint drawId = drawCommands[gl_DrawIDARB].drawId;
    MeshDraw meshDraw = draws[drawId];

    Vertex v = vertices[gl_VertexIndex];
    vec3 position = vec3(v.x, v.y, v.z);
    vec3 normal = vec3(v.nx, v.ny, v.nz) / 127.0 - 1.0;
    vec4 tangent = vec4(v.tx, v.ty, v.tz, v.tw) / 127.0 - 1.0;
    vec2 texCoord = vec2(v.tu, v.tv);

    normal = RotateVecByQuat(normal, meshDraw.orientation);
    tangent.xyz = RotateVecByQuat(tangent.xyz, meshDraw.orientation);
    
    gl_Position = globals.projection * (globals.cullData.view * vec4(RotateVecByQuat(position, meshDraw.orientation) * meshDraw.scale + meshDraw.position, 1));
    
    outDrawId = drawId;
    outUv = texCoord;
    outNormal = normal;
    outTangent = tangent;
}