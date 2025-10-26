
#pragma once
#include "assets/managers/asset_manager.h"
#include "cson/cson_reader.h"
#include "gltf/gltf_asset_types.h"
#include "renderer/mesh.h"
#include "renderer/types.h"

namespace C3D
{
    struct SceneAsset final : IAsset
    {
        SceneAsset() : IAsset(AssetType::Scene) {}

        Camera camera;

        DynamicArray<MeshAsset> meshes;
        DynamicArray<MeshDraw> draws;
    };

    class C3D_API SceneManager final : public IAssetManager
    {
    public:
        SceneManager();

        bool Read(const String& name, SceneAsset& asset);
        static void Cleanup(SceneAsset& asset);

    private:
        bool CreateSceneAsset(GLTFAsset& asset, SceneAsset& scene);

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
        bool ParseMaterialExtensions(const CSONObject& extensionsObj, DynamicArray<GLTFExtension>& materialExtensions) const;
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