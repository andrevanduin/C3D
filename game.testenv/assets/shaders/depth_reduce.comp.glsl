
#version 450

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout (binding = 0, r32f) uniform writeonly image2D out_image;

void main()
{
    uvec2 pos = gl_GlobalInvocationID.xy;

    imageStore(out_image, ivec2(pos), vec4(0));
}