// Mesh shader. See: https://github.com/KhronosGroup/GLSL/blob/main/extensions/ext/GLSL_EXT_mesh_shader.txt for more information

#version 450

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_mesh_shader : require

#include "definitions.h"
#include "math.h"

layout (constant_id = 0) const bool LATE = false;

layout (local_size_x = TASK_WGSIZE, local_size_y = 1, local_size_z = 1) in;

#define CULL TASK_CULL

layout (push_constant) uniform block
{
    Globals globals;
};

layout (binding = 0) readonly buffer TaskCommands
{
    MeshTaskCommand taskCommands[];
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
    // We convert 2D index to 1D index using a fixed * 64 factor, see task_submit.comp.glsl
    uint commandId = gl_WorkGroupID.x * 64 + gl_WorkGroupID.y;
    MeshTaskCommand command = taskCommands[commandId];
    uint drawId = command.drawId;
    MeshDraw meshDraw = draws[drawId];

    uint lateDrawVisibility = command.lateDrawVisibility;
    uint taskCount = command.taskCount;

    uint mgi = gl_LocalInvocationID.x;
    uint mi = mgi + command.taskOffset;
    uint mvi = mgi + command.meshletVisibilityOffset;

#if CULL
    sharedCount = 0;
    barrier();  // For sharedCount

    CullData cullData = globals.cullData;

    vec3 center = RotateVecByQuat(meshlets[mi].center, meshDraw.orientation) * meshDraw.scale + meshDraw.position;
    center = (cullData.view * vec4(center, 1)).xyz;

    float radius = meshlets[mi].radius * meshDraw.scale;
    
    vec3 coneAxis = RotateVecByQuat(vec3(meshlets[mi].coneAxis[0] / 127.0, meshlets[mi].coneAxis[1] / 127.0, meshlets[mi].coneAxis[2] / 127.0), meshDraw.orientation);
    coneAxis = mat3(cullData.view) * coneAxis;
    
    float coneCutoff = int(meshlets[mi].coneCutoff) / 127.0;

    bool valid = mgi < taskCount;
    bool visible = valid;
    bool skip = false;

    if (cullData.clusterOcclusionCullingEnabled == 1)
    {
        uint meshletVisibilityBit = meshletVisibility[mvi >> 5] & (1u << (mvi & 31));

        // In early pass, we have to *only* render clusters that were visible last frame, to build a reasonable depth pyramid out of visible triangles
        if (!LATE && meshletVisibilityBit == 0)
        {
            visible = false;
        }

        // In the late pass, we have to process objects visible last frame again (after rendering them in early pass)
        // in early pass, per above test, we render previously visible clusters
        // in late pass, we must invert the above test to *not* render previously visible clusters of previously visible objects because they were rendered in early pass.
        if (LATE && lateDrawVisibility == 1 && meshletVisibilityBit != 0)
        {
            skip = true;
        }
    }

    // Backface cone culling
    visible = visible && !ConeCull(center, radius, coneAxis, coneCutoff, vec3(0, 0, 0));
    // The left/top/right/bottom plane culling utilizes frustum symmetry to cull against two planes at the same time
    visible = visible && center.z * cullData.frustum[1] - abs(center.x) * cullData.frustum[0] > -radius;
    visible = visible && center.z * cullData.frustum[3] - abs(center.y) * cullData.frustum[2] > -radius;
    // The near/far plane culling uses camera space Z directly
    visible = visible && center.z + radius > cullData.zNear && center.z - radius < cullData.zFar;

    if (LATE && cullData.clusterOcclusionCullingEnabled == 1 && visible)
    {
        vec4 aabb;
        if (ProjectSphere(center, radius, cullData.zNear, cullData.p00, cullData.p11, aabb))
        {
            float width = (aabb.z - aabb.x) * cullData.pyramidWidth;
            float height = (aabb.w - aabb.y) * cullData.pyramidHeight;

            float level = floor(log2(max(width, height)));

            // Sampler is set up to do min reduction, so this computes the minimum depth of a 2x2 texel quad
			float depth = textureLod(depthPyramid, (aabb.xy + aabb.zw) * 0.5, level).x;
			float depthSphere = cullData.zNear / (center.z - radius);

			visible = visible && depthSphere > depth;
        }
    }

    if (LATE && cullData.clusterOcclusionCullingEnabled == 1 && valid)
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
        payload.clusterIndices[index] = commandId | (mgi << 24);
    }

    barrier(); // For sharedCount
    EmitMeshTasksEXT(sharedCount, 1, 1);

#else
    payload.clusterIndices[gl_LocalInvocationID.x] = commandId | (mgi << 24);

    EmitMeshTasksEXT(taskCount, 1, 1);
#endif
}