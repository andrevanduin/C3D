// Mesh shader. See: https://github.com/KhronosGroup/GLSL/blob/main/extensions/ext/GLSL_EXT_mesh_shader.txt for more information

#version 450

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_mesh_shader : require
#extension GL_ARB_shader_draw_parameters : require

#include "definitions.h"
#include "math_utils.h"

#define DEBUG 0

layout (local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
layout (triangles, max_vertices = 64, max_primitives = 124) out;

layout (push_constant) uniform block
{
    Globals globals;
};

layout (binding = 0) readonly buffer Draws 
{
    MeshDraw draws[];
};

layout (binding = 1) readonly buffer Meshlets
{
    Meshlet meshlets[];
};

layout(binding = 2) readonly buffer MeshletData
{
	uint meshletData[];
};

// Same binding as before but now interpret it as u8 data
layout(binding = 2) readonly buffer MeshletData8
{
	uint8_t meshletData8[];
};

layout (binding = 3) readonly buffer Vertices 
{
    Vertex vertices[];
};


struct Task
{
    uint meshletIndices[32];
};
taskPayloadSharedEXT Task IN;

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
    uint ti = gl_LocalInvocationID.x;
    uint mi = IN.meshletIndices[gl_WorkGroupID.x];
    
    MeshDraw meshDraw = draws[gl_DrawIDARB];

    uint vertexCount = meshlets[mi].vertexCount;
    uint triangleCount = meshlets[mi].triangleCount;

    uint vertexOffset = meshlets[mi].dataOffset;
    // Multiply by 4 to compensate for the fact that this index is based on u32's and we will index based on u8's
    uint indexOffset = (vertexOffset + vertexCount) * 4;

#if DEBUG
    uint meshletHash = pcg_hash(mi); 
    vec3 meshletColor = vec3(meshletHash & 255, (meshletHash >> 8) & 255, (meshletHash >> 16) & 225) / 255;
#endif

    for (uint i = ti; i < vertexCount; i += 32) 
    {
        uint vi = meshletData[vertexOffset + i];
        
        Vertex v = vertices[vi];

        vec3 position = vec3(v.x, v.y, v.z);
        vec3 normal = vec3(v.nx, v.ny, v.nz) / 127.0 - 1.0;
        vec2 texCoord = vec2(v.u, v.v);

        gl_MeshVerticesEXT[i].gl_Position = globals.projection * vec4(RotateVecByQuat(position, meshDraw.orientation) * meshDraw.scale + meshDraw.position, 1);
        
        color[i] = vec4(normal * 0.5 + vec3(0.5), 1.0);

#if DEBUG
        color[i] = vec4(meshletColor, 1.0);
#endif
    }

    for (uint i = ti; i < triangleCount; i += 32)
    {
        uint offset = indexOffset + i * 3;
        gl_PrimitiveTriangleIndicesEXT[i] = uvec3(meshletData8[offset + 0], meshletData8[offset + 1], meshletData8[offset + 2]);
    }

    if (ti == 0)
    {
        SetMeshOutputsEXT(vertexCount, triangleCount);
    }
}