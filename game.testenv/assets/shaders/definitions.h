struct Vertex
{
    float x, y, z;
    uint8_t nx, ny, nz, nw;
    float16_t u, v;
};

struct Meshlet
{
    vec3 center;
    float radius;

    int8_t coneAxis[3];
    int8_t coneCutoff;

    uint dataOffset;
    uint8_t triangleCount;
    uint8_t vertexCount;
};

struct Globals
{
    mat4 projection;
};

struct MeshDraw
{
    vec3 position;
    float scale;
    vec4 orientation;

    uint vertexOffset;
    uint indexOffset;
    uint indexCount;
    uint meshletOffset;
    uint meshletCount;
};

struct MeshDrawCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;

    uint groupCountX;
    uint groupCountY;
    uint groupCountZ;
};