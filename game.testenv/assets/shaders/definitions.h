#define TASK_WGSIZE 64
#define MESH_WGSIZE 64
#define MAX_VERTICES 64

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
    uint8_t vertexCount;
    uint8_t triangleCount;
};

struct Globals
{
    mat4 projection;
};

struct DrawCullData
{
    float p00, p11, zNear, zFar;        // Symmertric projection parameters
    float frustum[4];                   // Data for left/right/top/bottom planes
    float lodBase, lodStep;             // Lod distance i = base * pow(step, i)
    float pyramidWidth, pyramidHeight;  // Depth pyramid size in texels

    uint drawCount;

    int cullingEnabled;
    int occlusionCullingEnabled;
    int lodEnabled;
};

struct MeshLod
{
    uint indexOffset;
    uint indexCount;

    uint meshletOffset;
    uint meshletCount;
};

struct Mesh
{
    vec3 center;
    float radius;

    uint vertexOffset;
    uint vertexCount;

    uint lodCount;
    MeshLod lods[8];
};

struct MeshDraw
{
    vec3 position;
    float scale;
    vec4 orientation;

    uint meshIndex;
    uint vertexOffset;  // == meshes[meshIndex].vertexOffset, improves data locality in the mesh shader
};

struct MeshDrawCommand
{
    uint drawId;

    // Traditional rasterizer
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;

    // Mesh shading
    uint taskOffset;
    uint taskCount;
    uint groupCountX;
    uint groupCountY;
    uint groupCountZ;
};

struct MeshTaskPayload
{
    uint drawId;
    uint meshletIndices[TASK_WGSIZE];
};