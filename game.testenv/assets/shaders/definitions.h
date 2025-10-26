#define TASK_WGSIZE 64
#define MESH_WGSIZE 64
#define MESHLET_MAX_VERTICES 64
#define MESHLET_MAX_TRIANGLES 96

// Should we do meshlet frustum, occlusion and backface culling in task shader?
#define TASK_CULL 1

// Should we do triangle frustum and backface culling in mesh shader?
#define MESH_CULL 0

/** @brief Maximum number of total task shader workgroups; 4M workgroups ~= 256M meshlets ~= 16B triangles if TASK_WGSIZE=64 and MESH_MAX_TRIANGLES=64 */
#define TASK_WGLIMIT (1 << 22)

/** @brief Maximum number of total visible clusters; 16M meshlets ~= 64MB buffer with cluster indices */
#define CLUSTER_LIMIT (1 << 24)

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

struct CullData
{
    mat4 view;

    float p00, p11, zNear, zFar;        // Symmertric projection parameters
    float frustum[4];                   // Data for left/right/top/bottom planes
    float lodTarget;                    // lod target error at z=1
    float pyramidWidth, pyramidHeight;  // Depth pyramid size in texels

    uint drawCount;

    int cullingEnabled;
    int occlusionCullingEnabled;
    int clusterOcclusionCullingEnabled;
    int lodEnabled;
};

struct Globals
{
    mat4 projection;
    CullData cullData;
    float screenWidth, screenHeight;
};

struct MeshLod
{
    uint indexOffset;
    uint indexCount;

    uint meshletOffset;
    uint meshletCount;
    float error;
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
    uint meshletVisibilityOffset;
};

struct MeshDrawCommand
{
    uint drawId;

    // VkDrawIndexedIndirectCommand
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;
};

struct MeshTaskCommand
{
    uint drawId;
    uint taskOffset;
    uint taskCount;
    uint lateDrawVisibility;
    uint meshletVisibilityOffset;
};

struct MeshTaskPayload
{
    uint clusterIndices[TASK_WGSIZE];
};