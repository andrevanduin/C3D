// Mesh shader. See: https://github.com/KhronosGroup/GLSL/blob/main/extensions/ext/GLSL_EXT_mesh_shader.txt for more information

#version 450

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_EXT_mesh_shader : require

layout (local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

#include "definitions.h"
#include "math_utils.h"

#define CULL 1

layout (push_constant) uniform block
{
    MeshDraw meshDraw;
};

layout (binding = 1) readonly buffer Meshlets
{
    Meshlet meshlets[];
};

struct Task
{
    uint meshletIndices[32];
};
taskPayloadSharedEXT Task OUT;

void main()
{
    uint ti = gl_LocalInvocationID.x;
    uint mgi = gl_WorkGroupID.x;
    uint mi = mgi * 32 + ti;

#if CULL
    vec3 center = RotateVecByQuat(meshlets[mi].center, meshDraw.orientation) * meshDraw.scale + meshDraw.position;
    float radius = meshlets[mi].radius * meshDraw.scale;
    vec3 coneAxis = RotateVecByQuat(meshlets[mi].coneAxis, meshDraw.orientation);
    float coneCutoff = meshlets[mi].coneCutoff;

    bool accept = !ConeCull(center, radius, coneAxis, coneCutoff, vec3(0, 0, 0));

    uvec4 ballot = subgroupBallot(accept);

    uint index = subgroupBallotExclusiveBitCount(ballot);

    if (accept)
    {
        OUT.meshletIndices[index] = mi;
    }

    uint count = subgroupBallotBitCount(ballot);

    if (ti == 0)
    {
        EmitMeshTasksEXT(count, 1, 1);
    }

#else
    OUT.meshletIndices[ti] = mi;

    if (ti == 0)
    {
        EmitMeshTasksEXT(32, 1, 1);
    }
#endif
}