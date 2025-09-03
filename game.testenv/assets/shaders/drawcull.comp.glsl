#version 450

#extension GL_EXT_shader_explicit_arithmetic_types : require

#include "definitions.h"

layout (local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout (push_constant) uniform block
{
    vec4 frustum[6];
};

layout (binding = 0) readonly buffer Draws
{
    MeshDraw draws[];
};

layout (binding = 1) readonly buffer Meshes
{
    Mesh meshes[];
};

layout (binding = 2) writeonly buffer DrawCommands
{
    MeshDrawCommand drawCommands[];
};

layout (binding = 3) buffer DrawCommandCount
{
    uint drawCommandCount;
};

void main()
{
    uint ti = gl_LocalInvocationID.x;
    uint gi = gl_WorkGroupID.x;
    uint di = gi * 32 + ti;

    Mesh mesh = meshes[draws[di].meshIndex];

    vec3 center = mesh.center * draws[di].scale + draws[di].position;
    float radius = mesh.radius * draws[di].scale;

    bool visible = true;
    for (uint i = 0; i < 6; ++i)
    {
        visible = visible && dot(frustum[i], vec4(center, 1)) > -radius;
    }

    if (visible)
    {
        uint dci = atomicAdd(drawCommandCount, 1);

        drawCommands[dci].drawId = di;

        drawCommands[dci].indexCount = mesh.indexCount;
        drawCommands[dci].instanceCount = 1;
        drawCommands[dci].firstIndex = mesh.indexOffset;
        drawCommands[dci].vertexOffset = mesh.vertexOffset;
        drawCommands[dci].firstInstance = 0;

        drawCommands[dci].taskOffset = mesh.meshletOffset / 32;
        drawCommands[dci].groupCountX = (mesh.meshletCount + 31) / 32;
        drawCommands[dci].groupCountY = 1;
        drawCommands[dci].groupCountZ = 1;
    }
}