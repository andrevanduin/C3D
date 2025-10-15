
#pragma once
#include "assets/managers/asset_manager.h"
#include "cson/cson_reader.h"
#include "scene_asset_types.h"

namespace C3D
{
    class C3D_API SceneManager final : public IAssetManager
    {
    public:
        SceneManager();

        bool Read(const String& name, SceneAsset& asset);
        static void Cleanup(SceneAsset& asset);

    private:
        bool ImportGltfFile(SceneAsset& asset);

        bool ParseAsset(const CSONObject& gltf, SceneAsset& asset) const;
        bool ParseExtensionsUsed(const CSONObject& gltf, SceneAsset& asset) const;
        bool ParseScene(const CSONObject& gltf, SceneAsset& asset) const;
        bool ParseCameras(const CSONObject& gltf, SceneAsset& asset) const;
        bool ParseCamera(const CSONObject& cameraObject, SceneAsset& asset) const;
        bool ParseBuffers(const CSONObject& gltf, SceneAsset& asset) const;
        bool ParseBufferViews(const CSONObject& gltf, SceneAsset& asset) const;
        bool ParseScenes(const CSONObject& gltf, SceneAsset& asset) const;
        bool ParseAccessors(const CSONObject& gltf, SceneAsset& asset) const;
        bool ParseSamplers(const CSONObject& gltf, SceneAsset& asset) const;
        bool ParseMaterials(const CSONObject& gltf, SceneAsset& asset) const;
        bool ParseMeshes(const CSONObject& gltf, SceneAsset& asset) const;
        bool ParseNodes(const CSONObject& gltf, SceneAsset& asset) const;
        bool ParseTextures(const CSONObject& gltf, SceneAsset& asset) const;
        bool ParseImages(const CSONObject& gltf, SceneAsset& asset) const;
        bool ParseExtensions(const CSONObject& gltf, SceneAsset& asset) const;

        CSONReader m_csonReader;
    };
}  // namespace C3D