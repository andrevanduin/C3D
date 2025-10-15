
#include "scene_manager.h"

#include "time/scoped_timer.h"

namespace C3D
{
    SceneManager::SceneManager() : IAssetManager(MemoryType::Scene, AssetType::Scene, "scenes") {}

    bool SceneManager::Read(const String& name, SceneAsset& asset)
    {
        if (name.Empty())
        {
            ERROR_LOG("No valid name was provided.");
            return false;
        }

        // The name should be a folder inside of the scenes folder in there we expect a .gltf file with the same name
        String fullPath = String::FromFormat("{}/{}/{}/{}.{}", m_assetPath, m_subFolder, name, name, "gltf");

        // Check if the requested file exists with the current extension
        if (!File::Exists(fullPath))
        {
            ERROR_LOG("Unable to find a scene file called: '{}'.", name);
            return false;
        }

        // Copy the path to the file
        asset.path = fullPath;
        // Copy the name of the asset
        asset.name = name;

        auto result = ImportGltfFile(asset);
        if (!result)
        {
            ERROR_LOG("Failed to parse gltf file.");
        }

        return result;
    }

    void SceneManager::Cleanup(SceneAsset& asset) {}

    bool SceneManager::ImportGltfFile(SceneAsset& asset)
    {
        INFO_LOG("Importing gltf file: '{}'", asset.path);

        CSONObject gltf;

        {
            ScopedTimer timer(String::FromFormat("Parsing: '{}'.", asset.path));

            // Read the GLTF file
            if (!m_csonReader.ReadFromFile(asset.path, gltf))
            {
                ERROR_LOG("Failed to parse GLTF file: {}.", asset.path);
                return false;
            }

            // Parse the asset property (mandatory)
            if (!ParseAsset(gltf, asset))
            {
                ERROR_LOG("Invalid GLTF file: {}.", asset.path);
                return false;
            }

            // Parse the extensionsUsed property (optional)
            if (!ParseExtensionsUsed(gltf, asset))
            {
                ERROR_LOG("Invalid GLTF file: {}.", asset.path);
                return false;
            }

            // Parse the scene property (optional)
            if (!ParseScene(gltf, asset))
            {
                ERROR_LOG("Invalid GLTF file: {}.", asset.path);
                return false;
            }

            // Parse the cameras property (optional)
            if (!ParseCameras(gltf, asset))
            {
                ERROR_LOG("Invalid GLTF file: {}.", asset.path);
                return false;
            }

            // Parse the buffers property (optional)
            if (!ParseBuffers(gltf, asset))
            {
                ERROR_LOG("Invalid GLTF file: {}.", asset.path);
                return false;
            }

            // Parse the bufferViews property (optional)
            if (!ParseBufferViews(gltf, asset))
            {
                ERROR_LOG("Invalid GLTF file: {}.", asset.path);
                return false;
            }

            // Parse the scenes property (optional)
            if (!ParseScenes(gltf, asset))
            {
                ERROR_LOG("Invalid GLTF file: {}.", asset.path);
                return false;
            }
        }

        return false;
    }

    bool SceneManager::ParseAsset(const CSONObject& gltf, SceneAsset& asset) const
    {
        CSONObject assetObject;
        if (!gltf.GetPropertyValueByName("asset", assetObject))
        {
            ERROR_LOG("GLTF file does not contain: 'asset' property.");
            return false;
        }

        // Get generator (optional)
        assetObject.GetPropertyValueByName("generator", asset.generator);

        // Get the version (mandatory)
        if (!assetObject.GetPropertyValueByName("version", asset.version))
        {
            ERROR_LOG("GLTF file 'asset' does not contain: 'version'.");
            return false;
        }

        return true;
    }

    bool SceneManager::ParseExtensionsUsed(const CSONObject& gltf, SceneAsset& asset) const
    {
        CSONArray extensionsUsedArray;
        if (!gltf.GetPropertyValueByName("extensionsUsed", extensionsUsedArray))
        {
            // Not mandatory
            return true;
        }

        for (const auto& prop : extensionsUsedArray.properties)
        {
            if (!prop.HoldsString())
            {
                ERROR_LOG("'extensionsUsed' should only contain strings.");
                return false;
            }
            asset.extensionsUsed.EmplaceBack(prop.GetString());
        }

        return true;
    }

    bool SceneManager::ParseScene(const CSONObject& gltf, SceneAsset& asset) const
    {
        gltf.GetPropertyValueByName("scene", asset.defaultScene);
        return true;
    }

    bool SceneManager::ParseCameras(const CSONObject& gltf, SceneAsset& asset) const
    {
        CSONArray camerasArray;
        if (!gltf.GetPropertyValueByName("cameras", camerasArray))
        {
            // Not mandatory
            return true;
        }

        for (const auto& prop : camerasArray.properties)
        {
            if (!prop.HoldsObject())
            {
                ERROR_LOG("The 'cameras' property should only contain objects.");
                return false;
            }

            CSONObject cameraObject = prop.GetObject();
            if (!ParseCamera(cameraObject, asset))
            {
                return false;
            }
        }

        return true;
    }

    bool SceneManager::ParseCamera(const CSONObject& cameraObject, SceneAsset& asset) const
    {
        SceneCamera camera = {};

        // Mandatory type
        String cameraType;
        if (!cameraObject.GetPropertyValueByName("type", cameraType))
        {
            ERROR_LOG("Each camera inside 'cameras' property needs a 'type' property of type string.");
            return false;
        }

        if (cameraType == "perspective")
        {
            camera.type = SceneCameraType::Perspective;
            if (cameraObject.HasProperty("orthographic"))
            {
                ERROR_LOG("Camera has type 'perspective' but it contains 'orthographic' property.");
                return false;
            }
        }
        else if (cameraType == "orthographic")
        {
            camera.type = SceneCameraType::Orthographic;
            if (cameraObject.HasProperty("perspective"))
            {
                ERROR_LOG("Camera has type 'orthographic' but it contains 'perspective' property.");
                return false;
            }
        }
        else
        {
            ERROR_LOG("'{}' is not a valid camera type.", cameraType);
            return false;
        }

        // optional name
        cameraObject.GetPropertyValueByName("name", camera.name);

        if (camera.type == SceneCameraType::Perspective)
        {
            CSONObject perspectiveObj;
            if (cameraObject.GetPropertyValueByName("perspective", perspectiveObj))  // optional
            {
                if (!perspectiveObj.GetPropertyValueByName("yfov", camera.perspective.yFov))  // mandatory
                {
                    ERROR_LOG("The 'perspective' property must hold a 'yfov' property of type float.");
                    return false;
                }
                if (!perspectiveObj.GetPropertyValueByName("znear", camera.perspective.zNear))  // mandatory
                {
                    ERROR_LOG("The 'perspective' property must hold a 'znear' property of type float.");
                    return false;
                }

                perspectiveObj.GetPropertyValueByName("zfar", camera.perspective.zFar);                // optional
                perspectiveObj.GetPropertyValueByName("aspectRatio", camera.perspective.aspectRatio);  // optional
            }
        }
        else
        {
            CSONObject orthographicObj;
            if (cameraObject.GetPropertyValueByName("orthographic", orthographicObj))  // optional
            {
                if (!orthographicObj.GetPropertyValueByName("xmag", camera.orthographic.xmag))  // mandatory
                {
                    ERROR_LOG("The 'orthographic' property must hold a 'xmag' property of type float.");
                    return false;
                }
                if (!orthographicObj.GetPropertyValueByName("ymag", camera.orthographic.ymag))  // mandatory
                {
                    ERROR_LOG("The 'orthographic' property must hold a 'ymag' property of type float.");
                    return false;
                }
                if (!orthographicObj.GetPropertyValueByName("zfar", camera.orthographic.zFar))  // mandatory
                {
                    ERROR_LOG("The 'orthographic' property must hold a 'zfar' property of type float.");
                    return false;
                }
                if (!orthographicObj.GetPropertyValueByName("znear", camera.orthographic.zNear))  // mandatory
                {
                    ERROR_LOG("The 'orthographic' property must hold a 'znear' property of type float.");
                    return false;
                }
            }
        }

        // Finally we add this camera to our scene asset
        asset.cameras.PushBack(camera);

        return true;
    }

    bool SceneManager::ParseBuffers(const CSONObject& gltf, SceneAsset& asset) const
    {
        CSONArray buffersArray;
        if (!gltf.GetPropertyValueByName("buffers", buffersArray))
        {
            // Not mandatory
            return true;
        }

        for (const auto& bufferProp : buffersArray.properties)
        {
            if (!bufferProp.HoldsObject())
            {
                ERROR_LOG("The 'buffers' property must contain an array of buffer objects.");
                return false;
            }

            SceneBuffer buffer;

            CSONObject bufferObj = bufferProp.GetObject();

            // Optional name
            bufferObj.GetPropertyValueByName("name", buffer.name);
            // Optional uri
            bufferObj.GetPropertyValueByName("uri", buffer.uri);
            // Mandatory byteLength
            if (!bufferObj.GetPropertyValueByName("byteLength", buffer.byteLength))
            {
                ERROR_LOG("Each buffer in the 'buffers' property must have a 'byteLength' property.");
                return false;
            }

            // Finally we add this buffer to our scene asset
            asset.buffers.PushBack(buffer);
        }

        return true;
    }

    bool SceneManager::ParseBufferViews(const CSONObject& gltf, SceneAsset& asset) const
    {
        CSONArray bufferViewsArray;
        if (!gltf.GetPropertyValueByName("bufferViews", bufferViewsArray))
        {
            // Not mandatory
            return true;
        }

        for (const auto& bufferViewProp : bufferViewsArray.properties)
        {
            if (!bufferViewProp.HoldsObject())
            {
                ERROR_LOG("The 'bufferViews' property must contain an array of bufferView objects.");
                return false;
            }

            SceneBufferView bufferView;

            CSONObject bufferViewObj = bufferViewProp.GetObject();

            // Optional name
            bufferViewObj.GetPropertyValueByName("name", bufferView.name);

            // Mandatory buffer
            if (!bufferViewObj.GetPropertyValueByName("buffer", bufferView.buffer))
            {
                ERROR_LOG("Each bufferView in the 'bufferViews' property must contain a 'buffer' property.");
                return false;
            }

            // Optional byteOffset
            bufferViewObj.GetPropertyValueByName("byteOffset", bufferView.byteOffset);

            // Mandatory byteLength
            if (!bufferViewObj.GetPropertyValueByName("byteLength", bufferView.byteLength))
            {
                ERROR_LOG("Each bufferView in the 'bufferViews' property must contain a 'byteLength' property.");
                return false;
            }

            // Optional byteStride
            bufferViewObj.GetPropertyValueByName("byteStride", bufferView.byteStride);

            // Optional target
            bufferViewObj.GetPropertyValueByName("target", bufferView.target);

            // Finally we add this bufferView to our asset
            asset.bufferViews.PushBack(bufferView);
        }

        return true;
    }

    bool SceneManager::ParseScenes(const CSONObject& gltf, SceneAsset& asset) const
    {
        CSONArray scenesArray;
        if (!gltf.GetPropertyValueByName("scenes", scenesArray))
        {
            // Not mandatory
            return true;
        }

        for (const auto& sceneProp : scenesArray.properties)
        {
            if (!sceneProp.HoldsObject())
            {
                ERROR_LOG("The 'scenes' property must contain an array of scene objects.");
                return false;
            }

            Scene scene;

            CSONObject sceneNodeObj = sceneProp.GetObject();

            // Optional
            sceneNodeObj.GetPropertyValueByName("name", scene.name);

            // Optional
            CSONArray nodes;
            if (sceneNodeObj.GetPropertyValueByName("nodes", nodes))
            {
                for (const auto n : nodes.properties)
                {
                    if (n.HoldsInteger())
                    {
                        scene.nodes.EmplaceBack(n.GetU32());
                    }
                }
            }

            asset.scenes.PushBack(scene);
        }

        return true;
    }

}  // namespace C3D