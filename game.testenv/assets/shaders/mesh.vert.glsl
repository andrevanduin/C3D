#version 450

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_ARB_shader_draw_parameters : require

#include "definitions.h"
#include "math_utils.h"

layout (push_constant) uniform block
{
    Globals globals;
};

layout (binding = 0) readonly buffer Draws
{
    MeshDraw draws[];
};

layout (binding = 1) readonly buffer Vertices 
{
    Vertex vertices[];
};

layout (location = 0) out vec4 color;

void main()
{
    MeshDraw meshDraw = draws[gl_DrawIDARB];

    Vertex v = vertices[gl_VertexIndex];
    vec3 position = vec3(v.x, v.y, v.z);
    vec3 normal = vec3(v.nx, v.ny, v.nz) / 127.0 - 1.0;
    vec2 texCoord = vec2(v.u, v.v);

    gl_Position = globals.projection * vec4(RotateVecByQuat(position, meshDraw.orientation) * meshDraw.scale + meshDraw.position, 1);

    color = vec4(normal * 0.5 + vec3(0.5), 1.0);
}