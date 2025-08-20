// Mesh shader. See: https://github.com/KhronosGroup/GLSL/blob/main/extensions/ext/GLSL_EXT_mesh_shader.txt for more information

#version 450

#define DEBUG 1

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_mesh_shader : require

layout (local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
layout (triangles, max_vertices = 64, max_primitives = 126) out;

#include "definitions.h"

layout (binding = 0) readonly buffer Vertices 
{
    Vertex vertices[];
};

layout (binding = 1) readonly buffer Meshlets
{
    Meshlet meshlets[];
};

layout (location = 0) out vec4 color[];

#if DEBUG
uint pcg_hash(uint a)
{
    uint state = a * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}
#endif

void main()
{
    uint mi = gl_WorkGroupID.x;
    uint ti = gl_LocalInvocationID.x;

    uint vertexCount = meshlets[mi].vertexCount;
    uint triangleCount = meshlets[mi].triangleCount;

#if DEBUG
    uint meshletHash = pcg_hash(mi); 
    vec3 meshletColor = vec3(meshletHash & 255, (meshletHash >> 8) & 255, (meshletHash >> 16) & 225) / 255;
#endif

    for (uint i = ti; i < vertexCount; i += 32) 
    {
        uint vi = meshlets[mi].vertices[i];
        
        Vertex v = vertices[vi];

        vec3 position = vec3(v.x, v.y, v.z);
        vec3 normal = vec3(v.nx, v.ny, v.nz) / 127.0 - 1.0;
        vec2 texCoord = vec2(v.u, v.v);

        gl_MeshVerticesEXT[i].gl_Position = vec4(position * vec3(1, 1, 0.5) + vec3(0, 0, 0.5), 1.0);
        
        color[i] = vec4(normal * 0.5 + vec3(0.5), 1.0);

#if DEBUG
        color[i] = vec4(meshletColor, 1.0);
#endif
    }

    for (uint i = ti; i < triangleCount; i += 32)
    {
        gl_PrimitiveTriangleIndicesEXT[i] = uvec3(meshlets[mi].indices[i * 3 + 0], meshlets[mi].indices[i * 3 + 1], meshlets[mi].indices[i * 3 + 2]);
    }

    if (ti == 0)
    {
        SetMeshOutputsEXT(meshlets[mi].vertexCount, meshlets[mi].triangleCount);
    }
}