// Mesh shader. See: https://github.com/KhronosGroup/GLSL/blob/main/extensions/ext/GLSL_EXT_mesh_shader.txt for more information

#version 450

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_mesh_shader : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_ARB_shader_draw_parameters : require

#include "definitions.h"
#include "math.h"

layout (constant_id = 0) const bool LATE = false;

layout (local_size_x = TASK_WGSIZE, local_size_y = 1, local_size_z = 1) in;

#define CULL 1

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

layout (binding = 5) buffer MeshletVisibility
{
    uint meshletVisibility[];
};

layout (binding = 6) uniform sampler2D depthPyramid;

taskPayloadSharedEXT MeshTaskPayload payload;

#if CULL
shared int sharedCount;
#endif

void main()
{
    uint drawId = drawCommands[gl_DrawIDARB].drawId;
    MeshDraw meshDraw = draws[drawId];
    uint lateDrawVisibility = drawCommands[gl_DrawIDARB].lateDrawVisibility;

    uint mgi = gl_GlobalInvocationID.x;
    uint mi = mgi + drawCommands[gl_DrawIDARB].taskOffset;

#if CULL
    sharedCount = 0;
    barrier();

    vec3 center = RotateVecByQuat(meshlets[mi].center, meshDraw.orientation) * meshDraw.scale + meshDraw.position;
    float radius = meshlets[mi].radius * meshDraw.scale;
    vec3 coneAxis = RotateVecByQuat(vec3(meshlets[mi].coneAxis[0] / 127.0, meshlets[mi].coneAxis[1] / 127.0, meshlets[mi].coneAxis[2] / 127.0), meshDraw.orientation);
    float coneCutoff = int(meshlets[mi].coneCutoff) / 127.0;

    uint mvi = meshDraw.meshletVisibilityOffset + mgi;

    bool valid = mgi < drawCommands[gl_DrawIDARB].taskCount;
    bool visible = valid;

    uint meshletVisibilityBit = meshletVisibility[mvi >> 5] & (1u << (mvi & 31));

    // TODO: this might not be the most efficient way to do this
    // occlusionEnabled=1 check is necessary because otherwise if we disable OC, cluster occlusion status becomes "sticky":
    // for draw calls are always dispatched with LATE=0, we never update their cull status because they are skipped
    if (!LATE && meshletVisibilityBit == 0 && renderData.clusterOcclusionCullingEnabled == 1)
    {
        visible = false;
    }

    bool skip = false;

    if (LATE && lateDrawVisibility == 1 && meshletVisibilityBit != 0)
    {
        skip = true;
    }

    // Backface cone culling
    visible = visible && !ConeCull(center, radius, coneAxis, coneCutoff, vec3(0, 0, 0));
    // The left/top/right/bottom plane culling utilizes frustum symmetry to cull against two planes at the same time
    visible = visible && center.z * renderData.frustum[1] - abs(center.x) * renderData.frustum[0] > -radius;
    visible = visible && center.z * renderData.frustum[3] - abs(center.y) * renderData.frustum[2] > -radius;
    // The near/far plane culling uses camera space Z directly
    visible = visible && center.z + radius > renderData.zNear && center.z - radius < renderData.zFar;

    if (LATE && visible && renderData.clusterOcclusionCullingEnabled == 1)
    {
        float p00 = renderData.projection[0][0], p11 = renderData.projection[1][1];

        vec4 aabb;
        if (ProjectSphere(center, radius, renderData.zNear, p00, p11, aabb))
        {
            float width = (aabb.z - aabb.x) * renderData.pyramidWidth;
            float height = (aabb.w - aabb.y) * renderData.pyramidHeight;

            float level = floor(log2(max(width, height)));

            // Sampler is set up to do min reduction, so this computes the minimum depth of a 2x2 texel quad
			float depth = textureLod(depthPyramid, (aabb.xy + aabb.zw) * 0.5, level).x;
			float depthSphere = renderData.zNear / (center.z - radius);

			visible = visible && depthSphere > depth;
        }
    }

    if (LATE && valid)
    {
        if (visible)
        {
            atomicOr(meshletVisibility[mvi >> 5], 1u << (mvi & 31));
        }
        else
        {
            atomicAnd(meshletVisibility[mvi >> 5], ~(1u << (mvi & 31)));
        }
    }

    if (visible && !skip)
    {
        uint index = atomicAdd(sharedCount, 1);
        payload.meshletIndices[index] = mi;
    }

    payload.drawId = drawId;

    barrier();
    EmitMeshTasksEXT(sharedCount, 1, 1);

#else
    payload.drawId = drawId;
    payload.meshletIndices[gl_LocalInvocationIndex] = mi;

    uint count = min(TASK_WGSIZE, drawCommands[gl_DrawIDARB].taskCount - gl_WorkGroupID.x * TASK_WGSIZE);
    EmitMeshTasksEXT(count, 1, 1);
#endif
}