
#pragma once
#include "assets/managers/asset_manager.h"
#include "containers/dynamic_array.h"
#include "cson/cson_reader.h"
#include "gltf/gltf_asset_types.h"
#include "renderer/material.h"
#include "renderer/mesh.h"

namespace C3D
{
    struct SceneCamera
    {
        vec3 position;
        quat orientation;
        f32 fovY;
    };

    struct SceneAsset final : IAsset
    {
        SceneAsset() : IAsset(AssetType::Scene) {}

        SceneCamera camera;
        vec3 sunDirection;

        DynamicArray<MeshAsset> meshes;
        DynamicArray<MeshDraw> draws;
        DynamicArray<String> textures;
        DynamicArray<Material> materials;
    };

    class C3D_API SceneManager final : public IAssetManager
    {
    public:
        SceneManager();

        bool Read(const String& name, SceneAsset& asset);
        static void Cleanup(SceneAsset& asset);

    private:
        bool CreateSceneAsset(const GLTFAsset& asset, SceneAsset& scene) const;

        bool ParseSceneMeshes(const GLTFAsset& asset, SceneAsset& scene) const;
        bool ParseSceneMeshIndices(const GLTFAsset& asset, const GLTFMeshPrimitive& primitive, MeshAsset& mesh) const;
        bool ParseSceneMeshVertices(const GLTFAsset& asset, const GLTFMeshPrimitive& primitive, MeshAsset& mesh) const;
        bool ParseVertexPosition(const GLTFAsset& asset, const GLTFMeshPrimitive& primitive, DynamicArray<f32>& scratchBuffer, MeshAsset& mesh) const;
        bool ParseVertexNormals(const GLTFAsset& asset, const GLTFMeshPrimitive& primitive, DynamicArray<f32>& scratchBuffer, MeshAsset& mesh) const;
        bool ParseVertexTangents(const GLTFAsset& asset, const GLTFMeshPrimitive& primitive, DynamicArray<f32>& scratchBuffer, MeshAsset& mesh) const;
        bool ParseVertexTexCoords(const GLTFAsset& asset, const GLTFMeshPrimitive& primitive, DynamicArray<f32>& scratchBuffer, MeshAsset& mesh) const;

        void RemapVertexAndIndexBuffer(MeshAsset& mesh) const;
        void OptimizeVertexFetchAndCache(MeshAsset& mesh) const;

        bool ParseSceneNodes(const GLTFAsset& asset, SceneAsset& scene) const;
        void ParseSceneMeshNode(const GLTFAsset& asset, const GLTFNode& node, SceneAsset& scene) const;
        bool ParseSceneCameraNode(const GLTFAsset& asset, const GLTFNode& node, SceneAsset& scene) const;

        bool ParseSceneTextures(const GLTFAsset& asset, SceneAsset& scene) const;

        bool ParseSceneMaterials(const GLTFAsset& asset, SceneAsset& scene) const;

        bool ImportGltfFile(const String& rootPath, SceneAsset& scene);

        bool ParseAsset(const CSONObject& gltf, GLTFAsset& asset) const;
        bool ParseExtensionsUsed(const CSONObject& gltf, GLTFAsset& asset) const;
        bool ParseDefaultScene(const CSONObject& gltf, GLTFAsset& asset) const;
        bool ParseCamera(const CSONObject& cameraObj, GLTFAsset& asset) const;
        bool ParseBuffer(const CSONObject& bufferObj, GLTFAsset& asset) const;
        bool ParseBufferView(const CSONObject& bufferViewObj, GLTFAsset& asset) const;
        bool ParseScene(const CSONObject& sceneObj, GLTFAsset& asset) const;
        bool ParseAccessor(const CSONObject& accessorObj, GLTFAsset& asset) const;
        bool ParseSampler(const CSONObject& samplerObj, GLTFAsset& asset) const;
        // Materials
        bool ParseMaterial(const CSONObject& materialObj, GLTFAsset& asset) const;
        bool ParsePBRSpecularGlossinessExtension(const CSONObject& extensionObj, GLTFExtension& extension) const;
        bool ParseTransmissionExtension(const CSONObject& extensionObj, GLTFExtension& extension) const;
        bool ParseMaterialExtensions(const CSONObject& extensionsObj, DynamicArray<GLTFExtension>& materialExtensions, GLTFMaterial& material) const;
        bool ParsePBR(const CSONObject& pbrObj, GLTFPBR& pbr) const;
        bool ParseNormalTexture(const CSONObject& normalTextureOjb, GLTFNormalTexture& normalTexture) const;
        bool ParseOcclusionTexture(const CSONObject& occlusionTextureOjb, GLTFOcclusionTexture& occlusionTexture) const;
        bool ParseTextureInfo(const CSONObject& textureInfoObj, GLTFTextureInfo& texInfo) const;
        // Meshes
        bool ParseMesh(const CSONObject& meshObj, GLTFAsset& asset) const;
        bool ParseMeshPrimitive(const CSONObject& primitiveObj, GLTFMeshPrimitive& primitive) const;
        // Nodes
        bool ParseNodes(const CSONObject& gltf, GLTFAsset& asset) const;
        bool ParseNode(const CSONObject& nodeObj, GLTFAsset& asset) const;
        bool ParseLightsPunctualNode(const CSONObject& extensionObj, GLTFExtension& extension) const;
        bool ParseNodeExtensions(const CSONObject& nodeExtensionsObj, DynamicArray<GLTFExtension>& nodeExtensions) const;
        // Textures
        bool ParseTexture(const CSONObject& textureObj, GLTFAsset& asset) const;
        bool ParseTextureDDS(const CSONObject& textureDDSObj, GLTFExtension& textureExtension) const;
        bool ParseTextureExtensions(const CSONObject& textureExtensionsObj, DynamicArray<GLTFExtension>& textureExtensions) const;

        bool ParseImage(const CSONObject& imageObj, GLTFAsset& asset) const;
        // Extensions
        bool ParseExtensions(const CSONObject& extensionsObj, GLTFAsset& asset) const;
        bool ParseLightsPunctual(const CSONObject& lightsObj, GLTFExtension& sceneExtension) const;

        CSONReader m_csonReader;
    };
}  // namespace C3D