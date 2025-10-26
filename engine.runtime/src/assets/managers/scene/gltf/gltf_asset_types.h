
#pragma once
#include "assets/types.h"
#include "containers/dynamic_array.h"
#include "cson/cson_types.h"
#include "gltf_extension.h"
#include "string/string.h"

namespace C3D
{
    constexpr u32 GLTF_POINTS         = 0;
    constexpr u32 GLTF_LINES          = 1;
    constexpr u32 GLTF_LINE_LOOP      = 2;
    constexpr u32 GLTF_LINE_STRIP     = 3;
    constexpr u32 GLTF_TRIANGLES      = 4;
    constexpr u32 GLTF_TRIANGLE_STRIP = 5;
    constexpr u32 GLTF_TRIANGLE_FAN   = 6;

    constexpr u32 GLTF_BYTE                   = 5120;
    constexpr u32 GLTF_UNSIGNED_BYTE          = 5121;
    constexpr u32 GLTF_SHORT                  = 5122;
    constexpr u32 GLTF_UNSIGNED_SHORT         = 5123;
    constexpr u32 GLTF_UNSIGNED_INT           = 5125;
    constexpr u32 GLTF_FLOAT                  = 5126;
    constexpr u32 GLTF_NEAREST                = 9728;
    constexpr u32 GLTF_LINEAR                 = 9729;
    constexpr u32 GLTF_NEAREST_MIPMAP_NEAREST = 9784;
    constexpr u32 GLTF_LINEAR_MIMAP_NEAREST   = 9785;
    constexpr u32 GLTF_CLAMP_TO_EDGE          = 33071;
    constexpr u32 GLTF_MIRRORED_REPEAT        = 33648;
    constexpr u32 GLTF_REPEAT                 = 10497;

    enum class GLTFCameraType
    {
        Orthographic,
        Perspective,
    };

    struct GLTFCameraOrthographic
    {
        // Mandatory > 0.f
        f32 xmag;
        // Mandatory > 0.f
        f32 ymag;
        // Mandatory >= 0.f
        f32 zFar;
        // Mandatory > 0.f
        f32 zNear;
    };

    struct GLTFCameraPerspective
    {
        // Optional > 0.f
        f32 aspectRatio;
        // Mandatory > 0.f
        f32 yFov = 0.f;
        // Optional > 0.f
        f32 zFar = 0.f;
        // Mandatory > 0.f
        f32 zNear = 0.f;
    };

    struct GLTFCamera
    {
        GLTFCameraType type;
        String name;

        union {
            GLTFCameraOrthographic orthographic;
            GLTFCameraPerspective perspective;
        };
    };

    struct GLTFBuffer
    {
        bool IsLoaded() const { return data && byteLength != INVALID_ID_U64; }

        const u8* GetData(u64 offset) const;

        bool ReadFromDisk(const String& basePath);

        void Destroy();

        // Optional
        String name;
        // Optional
        String uri;
        // Mandatory >= 1
        u64 byteLength = INVALID_ID_U64;
        // Pointer to the byte data associated with this buffer.
        u8* data = nullptr;
    };

    struct GLTFBufferView
    {
        // Optional
        String name;
        // Mandatory >= 0
        u32 buffer = INVALID_ID;
        // Optional >= 0
        u64 byteOffset = INVALID_ID_U64;
        // Mandatory >= 1
        u64 byteLength = INVALID_ID_U64;
        // Optional [4, 252]
        u32 byteStride = INVALID_ID;
        // Optional 34962 (ARRAY_BUFFER) or 34963 (ELEMENT_ARRAY_BUFFER)
        u32 target = INVALID_ID;
    };

    struct GLTFScene
    {
        // Optional
        String name;
        // Optional 1 or more nodes
        DynamicArray<u32> nodes;
    };

    enum class GLTFAccessorType
    {
        Scalar,
        Vec2,
        Vec3,
        Vec4,
        Mat2,
        Mat3,
        Mat4
    };

    struct GLTFAccessor
    {
        // Mandatory
        GLTFAccessorType type;
        // Optional
        String name;
        // Mandatory can be: 5120 (BYTE) or 5121 (UNSIGNED_BYTE) or 5122 (SHORT) or 5123 (UNSIGNED_SHORT) or
        // 5125 (UNSIGNED_INT) or 5126 (FLOAT)
        u32 componentType = INVALID_ID;
        // Optional >= 0
        u32 bufferView = INVALID_ID;
        // Optional >= 0
        u64 byteOffset = 0;
        // Mandatory >= 1
        u32 count = INVALID_ID;
        // Optional length must be 1, 2, 3, 4, 9 or 16
        DynamicArray<f32> min;
        // Optional length must be 1, 2, 3, 4, 9 or 16
        DynamicArray<f32> max;

        // The size of the elements (based on GLTF type)
        u64 elementSize = INVALID_ID_U64;
        // The size of each component in each element. For Scalar type this will match elementSize. (based on GLTF componenType)
        u64 componentSize = INVALID_ID_U64;

        // TODO: accessor.sparse (https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#reference-accessor-sparse)
        // Optional
        bool normalized = false;
    };

    struct GLTFSampler
    {
        // Optional
        String name;
        // Optional can be 9728 (NEAREST) or 9729 (LINEAR)
        u32 magFilter = INVALID_ID;
        // Optional can be:
        // 9728 (NEAREST) or 9729 (LINEAR) or 9984 (NEAREST_MIPMAP_NEAREST) or 9985 (LINEAR_MIPMAP_NEAREST) or
        // 9986 (NEAREST_MIPMAP_LINEAR) or 9987 (LINEAR_MIPMAP_LINEAR)
        u32 minFilter = INVALID_ID;
        // Optional can be: 33071 (CLAMP_TO_EDGE) or 33648 (MIRRORED_REPEAT) or 10497 (REPEAT)
        u32 wrapS = GLTF_REPEAT;
        // Optional can be: 33071 (CLAMP_TO_EDGE) or 33648 (MIRRORED_REPEAT) or 10497 (REPEAT)
        u32 wrapT = GLTF_REPEAT;
    };

    struct GLTFTextureInfo
    {
        // Mandatory >= 0
        u32 index = INVALID_ID;
        // Optional >= 0
        u32 texCoord = 0;
    };

    struct GLTFPBR
    {
        f32 baseColorFactor[4] = { 1, 1, 1, 1 };

        f32 metallicFactor  = 1.f;
        f32 roughnessFactor = 1.f;

        GLTFTextureInfo baseColorTexture;
        GLTFTextureInfo metallicRoughnessTexture;
    };

    struct GLTFNormalTexture
    {
        f32 scale = 1.f;

        GLTFTextureInfo info;
    };

    struct GLTFOcclusionTexture
    {
        f32 strength = 1.f;

        GLTFTextureInfo info;
    };

    enum class GLTFMaterialAlphaMode
    {
        Opaque,
        Mask,
        Blend,
    };

    struct GLTFMaterial
    {
        String name;
        GLTFPBR pbr;
        GLTFMaterialAlphaMode alphaMode;

        GLTFNormalTexture normalTexture;
        GLTFOcclusionTexture occlusionTexture;
        GLTFTextureInfo emissiveTexture;

        DynamicArray<GLTFExtension> extensions;

        f32 emissiveFactor[3] = { 0, 0, 0 };
        f32 alphaCutoff       = 0.5f;

        bool doubleSided = false;
    };

    struct GLTFMeshPrimitive
    {
        // Optional >= 0
        u32 indices = INVALID_ID;
        // Optional
        u32 material = INVALID_ID;
        // Optional can be: 0 (POINTS) or 1 (LINES) or 2 (LINE_LOOP) or 3 (LINE_STRIP) or 4 (TRIANGLES) or 5 (TRIANGLE_STRIP) or 6 (TRIANGLE_FAN)
        u32 mode = GLTF_TRIANGLES;
        // Mandatory
        CSONObject attributes;
        // Optional
        DynamicArray<CSONObject> targets;
    };

    struct GLTFMesh
    {
        // Optional
        String name;
        // Optional length >= 1
        DynamicArray<f32> weights;
        // Mandatory length >= 1
        DynamicArray<GLTFMeshPrimitive> primitives;
    };

    struct GLTFNode
    {
        // Optional
        String name;
        // Optional >= 0
        u32 camera = INVALID_ID;
        // Optional >= 0
        u32 skin = INVALID_ID;
        // Optional >= 0
        u32 mesh = INVALID_ID;
        // A pointer to this node's parent
        GLTFNode* parent = nullptr;
        // An array of pointers to children nodes
        DynamicArray<GLTFNode*> children;
        // An array of children indices
        DynamicArray<u32> childrenIndices;
        // Optional length >= 1
        DynamicArray<f32> weights;
        // Optional
        DynamicArray<GLTFExtension> extensions;
        // Optional
        f32 translation[3] = { 0.f, 0.f, 0.f };
        // Optional
        f32 rotation[4] = { 0.f, 0.f, 0.f, 1.f };
        // Optional
        f32 scale[3] = { 1.f, 1.f, 1.f };
        // Optional
        f32 matrix[16] = { 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f };
        // Boolean indicating if a matrix was present in the GLTF file
        bool hasMatrix = false;

        /** @brief Transform the node's local translation, rotation and scale to a f32[16] (mat4). */
        void TransformLocal(f32* outMatrix) const;

        /** @brief Transform the node's world translation, rotation and scale to a f32[16] (mat4). */
        void TransformWorld(f32* outMatrix) const;
    };

    struct GLTFTexture
    {
        // Optional
        String name;
        // Optional >= 0
        u32 sampler = INVALID_ID;
        // Optional  >= 0
        u32 source = INVALID_ID;
        // Optional
        DynamicArray<GLTFExtension> extensions;
    };

    struct GLTFImage
    {
        // Optional
        String name;
        // Optional
        String mimeType;
        // Optional
        String uri;
        // Optional >= 0
        u32 bufferView = INVALID_ID;
    };

    struct GLTFAsset
    {
        String generator;
        String version;
        String root;

        u32 defaultScene = 0;

        /** @brief An array of names of GLTF extensions used by this scene. */
        DynamicArray<String> extensionsUsed;
        DynamicArray<GLTFCamera> cameras;
        DynamicArray<GLTFBuffer> buffers;
        DynamicArray<GLTFBufferView> bufferViews;
        DynamicArray<GLTFScene> scenes;
        DynamicArray<GLTFAccessor> accessors;
        DynamicArray<GLTFSampler> samplers;
        DynamicArray<GLTFMaterial> materials;
        DynamicArray<GLTFMesh> meshes;
        DynamicArray<GLTFNode> nodes;
        DynamicArray<GLTFTexture> textures;
        DynamicArray<GLTFImage> images;
        DynamicArray<GLTFExtension> extensions;

        /** @brief Finds the accessor that belongs to the attribute with the given name.
         *
         * @param A const reference to the primitive
         * @param name The name of the attribute
         * @return A pointer to the accessor or nullptr when it can't be found
         */
        const GLTFAccessor* FindAccessor(const GLTFMeshPrimitive& primitive, const String& name) const;

        /** @brief Loads all buffers associated with this GLTF file. */
        bool LoadAllBuffers();

        /** @brief Unpacks the provided data and stores it in the destination.
         *
         * @param destination A pointer to the destination for the data
         * @param destinationElementSize The size of each element in the destination
         * @param accessor The accessor used for getting the indices
         * @return True if successful; false otherwise
         */
        bool UnpackIndexData(void* destination, u64 destinationElementSize, const GLTFAccessor& accessor) const;

        /**
         * @brief Unpacks the provided data and stores it as floats in the destination.
         *
         * @param destination A pointer to the destination for the data
         * @param accessor The accessor used for getting the floats
         * @return True if successful; false otherwise
         */
        bool UnpackFloats(f32* destination, const GLTFAccessor* accessor) const;
    };
}  // namespace C3D