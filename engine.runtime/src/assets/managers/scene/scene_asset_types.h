
#include "assets/types.h"

namespace C3D
{
    enum class SceneCameraType
    {
        Orthographic,
        Perspective,
    };

    struct SceneCameraOrthographic
    {
        f32 xmag  = 0.f;
        f32 ymag  = 0.f;
        f32 zNear = 0.f;
        f32 zFar  = 0.f;
    };

    struct SceneCameraPerspective
    {
        f32 aspectRatio = 0.f;
        f32 yFov        = 0.f;
        f32 zNear       = 0.f;
        f32 zFar        = 0.f;
    };

    struct SceneCamera
    {
        SceneCameraType type;
        String name;

        union {
            SceneCameraOrthographic orthographic;
            SceneCameraPerspective perspective;
        };
    };

    struct SceneBuffer
    {
        String name;
        String uri;
        u64 byteLength = 0;
    };

    struct SceneBufferView
    {
        String name;

        u32 buffer     = 0;
        u64 byteOffset = 0;
        u64 byteLength = 0;
        u32 byteStride = 0;
        u32 target     = 0;
    };

    struct Scene
    {
        String name;
        DynamicArray<u32> nodes;
    };

    struct SceneAsset final : IAsset
    {
        SceneAsset() : IAsset(AssetType::Scene) {}

        String generator;
        String version;

        u32 defaultScene = 0;

        DynamicArray<String> extensionsUsed;
        DynamicArray<SceneCamera> cameras;
        DynamicArray<SceneBuffer> buffers;
        DynamicArray<SceneBufferView> bufferViews;
        DynamicArray<Scene> scenes;
    };
}  // namespace C3D