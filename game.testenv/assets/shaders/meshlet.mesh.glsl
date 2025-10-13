// Mesh shader. See: https://github.com/KhronosGroup/GLSL/blob/main/extensions/ext/GLSL_EXT_mesh_shader.txt for more information

#version 450

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_mesh_shader : require
#extension GL_ARB_shader_draw_parameters : require

#include "definitions.h"
#include "math_utils.h"

#define DEBUG 0
#define CULL 1

layout (local_size_x = MESH_WGSIZE, local_size_y = 1, local_size_z = 1) in;
layout (triangles, max_vertices = MAX_VERTICES, max_primitives = 64) out;

layout (push_constant) uniform block
{
    RenderData renderData;
};

layout (binding = 0) readonly buffer DrawCommands
{
    MeshDrawCommand drawCommands[];
};

layout (binding = 1) readonly buffer Draws 
{
    MeshDraw draws[];
};

layout (binding = 2) readonly buffer Meshlets
{
    Meshlet meshlets[];
};

layout(binding = 3) readonly buffer MeshletData
{
	uint meshletData[];
};

// Same binding as before but now interpret it as u8 data
layout(binding = 3) readonly buffer MeshletData8
{
	uint8_t meshletData8[];
};

layout (binding = 4) readonly buffer Vertices 
{
    Vertex vertices[];
};

taskPayloadSharedEXT MeshTaskPayload payload;

layout (location = 0) out vec4 color[];

#if DEBUG
uint pcg_hash(uint a)
{
    uint state = a * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}
#endif

#if CULL
shared vec3 vertexClip[MAX_VERTICES];
#endif

void main()
{
    uint ti = gl_LocalInvocationIndex;
    uint mi = payload.meshletIndices[gl_WorkGroupID.x];
    
    MeshDraw meshDraw = draws[payload.drawId];

    uint vertexCount = meshlets[mi].vertexCount;
    uint triangleCount = meshlets[mi].triangleCount;

    SetMeshOutputsEXT(vertexCount, triangleCount);

    uint vertexOffset = meshlets[mi].dataOffset;
    uint indexOffset = vertexOffset + vertexCount;

#if DEBUG
    uint meshletHash = pcg_hash(mi); 
    vec3 meshletColor = vec3(meshletHash & 255, (meshletHash >> 8) & 255, (meshletHash >> 16) & 225) / 255;
#endif

    if (ti < vertexCount)
    {
        uint i = ti;
        uint vi = meshletData[vertexOffset + i] + meshDraw.vertexOffset;
        
        Vertex v = vertices[vi];

        vec3 position = vec3(v.x, v.y, v.z);
        vec3 normal = vec3(v.nx, v.ny, v.nz) / 127.0 - 1.0;
        vec2 texCoord = vec2(v.u, v.v);

        vec4 clip = renderData.projection * vec4(RotateVecByQuat(position, meshDraw.orientation) * meshDraw.scale + meshDraw.position, 1);
        gl_MeshVerticesEXT[i].gl_Position = clip;
        
        color[i] = vec4(normal * 0.5 + vec3(0.5), 1.0);

    #if CULL
        vertexClip[i] = vec3(clip.xy / clip.w, clip.w);
    #endif

    #if DEBUG
        color[i] = vec4(meshletColor, 1.0);
    #endif
    }

#if CULL
    barrier();
#endif

    vec2 screen = vec2(renderData.screenWidth, renderData.screenHeight);

    //for (uint i = ti; i < triangleCount; i += MESH_WGSIZE)
    if (ti < triangleCount)
    {
        uint i = ti;
        uint offset = indexOffset * 4 + i * 3;
        uint a = uint(meshletData8[offset + 0]), b = uint(meshletData8[offset + 1]), c = uint(meshletData8[offset + 2]);

        gl_PrimitiveTriangleIndicesEXT[i] = uvec3(a, b, c);

    #if CULL
        bool culled = false;

        vec2 pa = vertexClip[a].xy, pb = vertexClip[b].xy, pc = vertexClip[c].xy;

        // Backface culling + zero-area culling
        vec2 eb = pb - pa;
        vec2 ec = pc - pa;

        culled = culled || (eb.x * ec.y >= eb.y * ec.x);

        // Small primitive culling
        vec2 bmin = (min(pa, min(pb, pc)) * 0.5 + vec2(0.5)) * screen;
        vec2 bmax = (max(pa, max(pb, pc)) * 0.5 + vec2(0.5)) * screen;
        float sbprec = 1.0 / 256.0; // Note: This can be set to 1/2^subpixelPrecisionBits

        // Note: This is slightly imprecise (doesn't fully match hw bahavior and is both too lose and too strict)
        culled = culled || (round(bmin.x - sbprec) == round(bmax.x) || round(bmin.y) == round(bmax.y + sbprec));

        // The computations above are only valid if all vertices are in front of the perspective plane
        culled = culled && (vertexClip[a].z > 0 && vertexClip[b].z > 0 && vertexClip[c].z > 0);

        gl_MeshPrimitivesEXT[i].gl_CullPrimitiveEXT = culled;
    #endif
    }
}