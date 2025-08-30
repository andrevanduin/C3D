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

struct MeshDraw
{
    mat4 projection;
    vec3 position;
    float scale;
    vec4 orientation;
};