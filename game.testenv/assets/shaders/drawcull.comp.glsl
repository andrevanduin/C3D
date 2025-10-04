#version 450

#extension GL_EXT_shader_explicit_arithmetic_types : require

#include "definitions.h"

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout (push_constant) uniform block
{
    vec4 frustum[6];
    uint drawCount;
    int cullingEnabled;
    int lodEnabled;
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

    if (di >= drawCount)
    {
        return;
    }

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

        float lodDistance = log2(max(1, (distance(center, vec3(0)) - radius)));
        uint lodIndex = lodEnabled == 1 ? clamp(int(lodDistance), 0, int(mesh.lodCount) - 1) : 0;

        MeshLod lod = mesh.lods[lodIndex];

        drawCommands[dci].drawId = di;
        drawCommands[dci].indexCount = lod.indexCount;
        drawCommands[dci].instanceCount = 1;
        drawCommands[dci].firstIndex = lod.indexOffset;
        drawCommands[dci].vertexOffset = mesh.vertexOffset;
        drawCommands[dci].firstInstance = 0;

        drawCommands[dci].taskOffset = lod.meshletOffset;
        drawCommands[dci].taskCount = lod.meshletCount;

        drawCommands[dci].groupCountX = (lod.meshletCount + TASK_WGSIZE - 1) / TASK_WGSIZE;
        drawCommands[dci].groupCountY = 1;
        drawCommands[dci].groupCountZ = 1;
    }
}