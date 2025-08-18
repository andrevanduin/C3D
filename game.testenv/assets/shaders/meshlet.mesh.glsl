// Mesh shader. See: https://github.com/KhronosGroup/GLSL/blob/main/extensions/ext/GLSL_EXT_mesh_shader.txt for more information

#version 450

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_mesh_shader : require

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout (triangles, max_vertices = 64, max_primitives = 42) out;

struct Vertex
{
    float16_t x, y, z;
    uint8_t nx, ny, nz, nw;
    float16_t u, v;
};

layout (binding = 0) readonly buffer Vertices 
{
    Vertex vertices[];
};

struct Meshlet
{
    uint vertices[64];
    // NOTE: Divisible by 3 so good for up to 42 triangles
    uint8_t indices[126];
    uint8_t triangleCount;
    uint8_t vertexCount;
};

layout (binding = 1) readonly buffer Meshlets
{
    Meshlet meshlets[];
};

layout (location = 0) out vec4 color[];

void main()
{
    uint mi = gl_WorkGroupID.x;

    for (uint i = 0; i < uint(meshlets[mi].vertexCount); ++i)
    {
        uint vi = meshlets[mi].vertices[i];
        
        Vertex v = vertices[vi];

        vec3 position = vec3(v.x, v.y, v.z);
        vec3 normal = vec3(v.nx, v.ny, v.nz) / 127.0 - 1.0;
        vec2 texCoord = vec2(v.u, v.v);

        gl_MeshVerticesEXT[i].gl_Position = vec4(position * vec3(1, 1, 0.5) + vec3(0, 0, 0.5), 1.0);
        color[i] = vec4(normal * 0.5 + vec3(0.5), 1.0);
    }

    SetMeshOutputsEXT(meshlets[mi].vertexCount, meshlets[mi].triangleCount);

    for (int i = 0; i < meshlets[mi].triangleCount; i++)
    {
        gl_PrimitiveTriangleIndicesEXT[i] = uvec3(meshlets[mi].indices[i * 3 + 0], meshlets[mi].indices[i * 3 + 1], meshlets[mi].indices[i * 3 + 2]);
    }
}