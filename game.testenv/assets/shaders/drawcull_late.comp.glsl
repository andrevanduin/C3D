#version 450

#extension GL_EXT_shader_explicit_arithmetic_types : require

#include "definitions.h"

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout (push_constant) uniform block
{
    DrawCullData cullData;
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

layout (binding = 4) buffer DrawVisibility
{
    uint drawVisibility[];
};

layout (binding = 5) uniform sampler2D depthPyramid;

/* 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere by Michael Mara and Morgan McGuire 2013. */
bool ProjectSphere(vec3 center, float radius, float zNear, float p00, float p11, out vec4 aabb)
{
    if (center.z < radius + zNear)
    {
        return false;
    }

    vec2 cx = -center.xz;
    vec2 vx = vec2(sqrt(dot(cx, cx) - radius * radius), radius) / length(cx);
    vec2 minx = mat2(vx.x, vx.y, -vx.y, vx.x) * cx;
    vec2 maxx = mat2(vx.x, -vx.y, vx.y, vx.x) * cx;

    vec2 cy = -center.yz;
    vec2 vy = vec2(sqrt(dot(cy, cy) - radius * radius), radius) / length(cy);
    vec2 miny = mat2(vy.x, -vy.y, vy.y, vy.x) * cy;
    vec2 maxy = mat2(vy.x, vy.y, -vy.y, vy.x) * cy;

    aabb = vec4(minx.x / minx.y * p00, miny.x / miny.y * p11, maxx.x / maxx.y * p00, maxy.x / maxy.y * p11) * vec4(0.5f, -0.5f, 0.5f, -0.5f) + vec4(0.5f);

    return true;
}

void main()
{
    uint di = gl_GlobalInvocationID.x;

    if (di >= cullData.drawCount)
    {
        return;
    }

    uint meshIndex = draws[di].meshIndex;
    Mesh mesh = meshes[meshIndex];

    vec3 center = mesh.center * draws[di].scale + draws[di].position;
    float radius = mesh.radius * draws[di].scale;

    bool visible = true;
    for (uint i = 0; i < 6; ++i)
    {
         visible = visible && dot(cullData.frustum[i], vec4(center, 1)) > -radius;
    }

    visible = cullData.cullingEnabled == 1 ? visible : true;

    if (visible && cullData.occlusionCullingEnabled == 1)
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

    if (visible && drawVisibility[di] == 0)
    {
        uint dci = atomicAdd(drawCommandCount, 1);

        float lodDistance = log2(max(1, (distance(center, vec3(0)) - radius)));
        uint lodIndex = cullData.lodEnabled == 1 ? clamp(int(lodDistance), 0, int(mesh.lodCount) - 1) : 0;

        MeshLod lod = meshes[meshIndex].lods[lodIndex];

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

    drawVisibility[di] = visible ? 1 : 0;
}