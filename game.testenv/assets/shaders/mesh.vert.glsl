#version 450

#extension GL_EXT_shader_explicit_arithmetic_types : require

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

layout (location = 0) out vec4 color;

void main()
{
    Vertex v = vertices[gl_VertexIndex];
    vec3 position = vec3(v.x, v.y, v.z);
    vec3 normal = vec3(v.nx, v.ny, v.nz) / 127.0 - 1.0;
    vec2 texCoord = vec2(v.u, v.v);

    gl_Position = vec4(position * vec3(1, 1, 0.5) + vec3(0, 0, 0.5), 1.0);

    color = vec4(normal * 0.5 + vec3(0.5), 1.0);
}