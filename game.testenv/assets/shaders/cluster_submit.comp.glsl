#version 450

#extension GL_EXT_shader_explicit_arithmetic_types : require

#include "definitions.h"

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0) buffer ClusterCount
{
	uint clusterCount;
	uint groupCountX;
	uint groupCountY;
	uint groupCountZ;
};

layout(binding = 1) writeonly buffer ClusterIndices
{
	uint clusterIndices[];
};

void main()
{
    uint tid = gl_LocalInvocationID.x;
    uint count = min(clusterCount, CLUSTER_LIMIT);

    // Represent cluster count as X * 256 * 1; X has a max of 65535 (per EXT_mesh_shader limits), so this allows us to reach ~16M clusters
    if (tid == 0)
    {
        groupCountX = min((count + 255) / 256, 65535);
        groupCountY = 256;
        groupCountZ = 1;
    }

    // The above may result in reading command data that was never written; as such, pad the excess entries with dummy commands (up to 255)
    uint boundary = (count + 255) & ~255;

    if (count + tid < boundary)
    {
        clusterIndices[count + tid] = ~0;
    }
}