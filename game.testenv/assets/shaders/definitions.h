struct Vertex
{
    float x, y, z;
    uint8_t nx, ny, nz, nw;
    float16_t u, v;
};

struct Meshlet
{
    vec4 cone;
    uint dataOffset;
    uint8_t triangleCount;
    uint8_t vertexCount;
};

struct MeshDraw
{
    vec2 offset;
    vec2 scale;
};