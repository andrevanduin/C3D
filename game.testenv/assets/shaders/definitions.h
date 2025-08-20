
struct Vertex
{
    float16_t x, y, z, w;
    uint8_t nx, ny, nz, nw;
    float16_t u, v;
};

struct Meshlet
{
    uint vertices[64];
    // NOTE: Up to 126 triangles (3 indices per triangle)
    uint8_t indices[126 * 3];
    uint8_t triangleCount;
    uint8_t vertexCount;
};
