
#include "scene_manager.h"

#include <meshoptimizer/src/meshoptimizer.h>

#include "gltf/gltf_asset_types.h"
#include "gltf/gltf_extensions.h"
#include "math/c3d_math.h"
#include "time/scoped_timer.h"

#define PARSE_ARRAY_OF_OBJECTS_PROP(name, parseFunc, asset)           \
    {                                                                 \
        CSONArray _array;                                             \
        if (!gltf.GetPropertyValueByName(name, _array))               \
        {                                                             \
            return true;                                              \
        }                                                             \
        for (const auto& prop : _array.properties)                    \
        {                                                             \
            if (!prop.HoldsObject())                                  \
            {                                                         \
                ERROR_LOG("'{}' should only contain objects.", name); \
                return false;                                         \
            }                                                         \
            CSONObject _obj = prop.GetObject();                       \
            if (!parseFunc(_obj, asset))                              \
            {                                                         \
                ERROR_LOG("Invalid GLTF file.");                      \
                return false;                                         \
            }                                                         \
        }                                                             \
    }

#define PARSE_UNBOUNDED_ARRAY_OF_NUMBERS(obj, name, dest)                       \
    {                                                                           \
        CSONArray _array;                                                       \
        if (obj.GetPropertyValueByName(name, _array))                           \
        {                                                                       \
            for (u32 i = 0; i < _array.properties.Size(); ++i)                  \
            {                                                                   \
                const auto& _prop = _array.properties[i];                       \
                if (_prop.HoldsFloat())                                         \
                {                                                               \
                    dest.PushBack(_prop.GetF32());                              \
                }                                                               \
                else if (_prop.HoldsInteger())                                  \
                {                                                               \
                    dest.PushBack(_prop.GetI32());                              \
                }                                                               \
                else                                                            \
                {                                                               \
                    ERROR_LOG("'{}' property should only hold numbers.", name); \
                    return false;                                               \
                }                                                               \
            }                                                                   \
        }                                                                       \
    }

#define PARSE_UNBOUNDED_ARRAY_OF_INTEGERS(obj, name, dest)                      \
    {                                                                           \
        CSONArray _array;                                                       \
        if (obj.GetPropertyValueByName(name, _array))                           \
        {                                                                       \
            for (u32 i = 0; i < _array.properties.Size(); ++i)                  \
            {                                                                   \
                const auto& _prop = _array.properties[i];                       \
                if (!_prop.HoldsInteger())                                      \
                {                                                               \
                    ERROR_LOG("'{}' propertyshould only hold integers.", name); \
                    return false;                                               \
                }                                                               \
                dest.PushBack(_prop.GetI32());                                  \
            }                                                                   \
        }                                                                       \
    }

#define PARSE_ARRAY_OF_NUMBERS(obj, name, count, dest)                          \
    {                                                                           \
        CSONArray _array;                                                       \
        if (obj.GetPropertyValueByName(name, _array))                           \
        {                                                                       \
            if (_array.properties.Size() != count)                              \
            {                                                                   \
                ERROR_LOG("'{}' property should have {} values.", name, count); \
                return false;                                                   \
            }                                                                   \
            for (u32 i = 0; i < count; ++i)                                     \
            {                                                                   \
                const auto& _prop = _array.properties[i];                       \
                if (_prop.HoldsFloat())                                         \
                {                                                               \
                    dest[i] = _prop.GetF32();                                   \
                }                                                               \
                else if (_prop.HoldsInteger())                                  \
                {                                                               \
                    dest[i] = _prop.GetI32();                                   \
                }                                                               \
                else                                                            \
                {                                                               \
                    ERROR_LOG("'{}' property should only hold numbers.", name); \
                    return false;                                               \
                }                                                               \
            }                                                                   \
        }                                                                       \
    }

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

        // The base path is the path were the GLTF file resides
        String rootPath = String::FromFormat("{}/{}/{}", m_assetPath, m_subFolder, name);

        // The name should be a folder inside of the scenes folder in there we expect a .gltf file with the same name
        String fullPath = String::FromFormat("{}/{}.{}", rootPath, name, "gltf");

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

        auto result = ImportGltfFile(rootPath, asset);
        if (!result)
        {
            ERROR_LOG("Failed to parse gltf file.");
        }

        return result;
    }

    void SceneManager::Cleanup(SceneAsset& asset)
    {
        // Cleanup our mesh data
        for (auto& mesh : asset.meshes)
        {
            mesh.vertices.Destroy();
            mesh.indices.Destroy();
        }
        asset.meshes.Destroy();
        asset.draws.Destroy();
        asset.name.Destroy();
    }

    static void DecomposeTransform(f32 translation[3], f32 rotation[4], f32 scale[3], const f32* transform)
    {
        f32 m[4][4] = {};
        std::memcpy(m, transform, 16 * sizeof(f32));

        // Extract translation from last row
        translation[0] = m[3][0];
        translation[1] = m[3][1];
        translation[2] = m[3][2];

        // Compute determinant to determine handedness
        f32 det = m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                  m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

        f32 sign = (det < 0.f) ? -1.f : 1.f;

        // Recover scale from axis lengths
        scale[0] = Sqrt(m[0][0] * m[0][0] + m[0][1] * m[0][1] + m[0][2] * m[0][2]) * sign;
        scale[1] = Sqrt(m[1][0] * m[1][0] + m[1][1] * m[1][1] + m[1][2] * m[1][2]) * sign;
        scale[2] = Sqrt(m[2][0] * m[2][0] + m[2][1] * m[2][1] + m[2][2] * m[2][2]) * sign;

        // Normalize axes to get a pure rotation matrix
        f32 rsx = (scale[0] == 0.f) ? 0.f : 1.f / scale[0];
        f32 rsy = (scale[1] == 0.f) ? 0.f : 1.f / scale[1];
        f32 rsz = (scale[2] == 0.f) ? 0.f : 1.f / scale[2];

        f32 r00 = m[0][0] * rsx, r10 = m[1][0] * rsy, r20 = m[2][0] * rsz;
        f32 r01 = m[0][1] * rsx, r11 = m[1][1] * rsy, r21 = m[2][1] * rsz;
        f32 r02 = m[0][2] * rsx, r12 = m[1][2] * rsy, r22 = m[2][2] * rsz;

        // "Branchless" version of Mike Day's matrix to quaternion conversion
        i32 qc  = r22 < 0 ? (r00 > r11 ? 0 : 1) : (r00 < -r11 ? 2 : 3);
        f32 qs1 = qc & 2 ? -1.f : 1.f;
        f32 qs2 = qc & 1 ? -1.f : 1.f;
        f32 qs3 = (qc - 1) & 2 ? -1.f : 1.f;

        f32 qt = 1.f - qs3 * r00 - qs2 * r11 - qs1 * r22;
        f32 qs = 0.5f / Sqrt(qt);

        rotation[qc ^ 0] = qs * qt;
        rotation[qc ^ 1] = qs * (r01 + qs1 * r10);
        rotation[qc ^ 2] = qs * (r20 + qs2 * r02);
        rotation[qc ^ 3] = qs * (r12 + qs3 * r21);
    }

    bool SceneManager::CreateSceneAsset(GLTFAsset& asset, SceneAsset& scene)
    {
        ScopedTimer timer(String::FromFormat("Using GLTFAsset to create SceneAsset."));

        {
            ScopedTimer timer(String::FromFormat("Loading all GLTF buffers."));
            asset.LoadAllBuffers();
        }

        DynamicArray<f32> scratchBuffer;

        scene.meshes.Reserve(asset.meshes.Size());
        for (const auto& mesh : asset.meshes)
        {
            C3D_ASSERT_MSG(mesh.primitives.Size() == 1, "We only support meshes with a single primitive.");

            const auto& primitive = mesh.primitives[0];
            C3D_ASSERT_MSG(primitive.mode == GLTF_TRIANGLES, "We only support triangle meshes.");

            C3D_ASSERT_MSG(primitive.indices != INVALID_ID, "We only support meshes with index data.");

            MeshAsset sceneMesh = {};
            sceneMesh.name      = mesh.name;

            const u8* data  = nullptr;
            u64 size        = 0;
            u64 elementSize = 0;

            {
                // Determinate how many indices we have
                const auto& indexAccessor = asset.accessors[primitive.indices];
                // Resize indices array for enough space
                sceneMesh.indices.Resize(indexAccessor.count);
                // Unpack Indices
                asset.UnpackIndexData(sceneMesh.indices.GetData(), sizeof(u32), indexAccessor);
            }

            {
                // Get the positions
                auto posAccesor = asset.FindAccessor(primitive, "POSITION");
                C3D_ASSERT(posAccesor);
                C3D_ASSERT(posAccesor->type == GLTFAccessorType::Vec3);
                C3D_ASSERT(posAccesor->componentType == GLTF_FLOAT);

                // Ensure our scratch buffer to have enough space
                if (scratchBuffer.Size() < posAccesor->count * 4)
                {
                    scratchBuffer.Resize(posAccesor->count * 4);
                }

                // Resize our vertices array to have enough space
                sceneMesh.vertices.Resize(posAccesor->count);
                // Unpack positions into our scratchBuffer
                asset.UnpackFloats(scratchBuffer.GetData(), posAccesor);
                // Store the positions into our vertices
                for (u32 i = 0; i < posAccesor->count; ++i)
                {
                    sceneMesh.vertices[i].pos.x = scratchBuffer[i * 3 + 0];
                    sceneMesh.vertices[i].pos.y = scratchBuffer[i * 3 + 1];
                    sceneMesh.vertices[i].pos.z = scratchBuffer[i * 3 + 2];
                }

                // Get the normals
                auto normalAccesor = asset.FindAccessor(primitive, "NORMAL");
                C3D_ASSERT(normalAccesor);
                C3D_ASSERT(normalAccesor->type == GLTFAccessorType::Vec3);
                C3D_ASSERT(normalAccesor->componentType == GLTF_FLOAT);

                // Unpack normals into our scratchBuffer
                asset.UnpackFloats(scratchBuffer.GetData(), normalAccesor);
                // Store the normals into our vertices
                for (u32 i = 0; i < posAccesor->count; ++i)
                {
                    sceneMesh.vertices[i].nx = static_cast<u8>(scratchBuffer[i * 3 + 0] * 127.f + 127.5f);
                    sceneMesh.vertices[i].ny = static_cast<u8>(scratchBuffer[i * 3 + 1] * 127.f + 127.5f);
                    sceneMesh.vertices[i].nz = static_cast<u8>(scratchBuffer[i * 3 + 2] * 127.f + 127.5f);
                }

                // Get the texture coordinates
                auto texAccessor = asset.FindAccessor(primitive, "TEXCOORD_0");
                C3D_ASSERT(texAccessor);
                C3D_ASSERT(texAccessor->type == GLTFAccessorType::Vec2);
                C3D_ASSERT(texAccessor->componentType == GLTF_FLOAT);

                // Unpack texCoords into our scratchBuffer
                asset.UnpackFloats(scratchBuffer.GetData(), texAccessor);
                // Store the texCoords into our vertices
                for (u32 i = 0; i < posAccesor->count; ++i)
                {
                    sceneMesh.vertices[i].tx = QuantizeHalf(scratchBuffer[i * 3 + 0]);
                    sceneMesh.vertices[i].ty = QuantizeHalf(scratchBuffer[i * 3 + 1]);
                }

                {
                    ScopedTimer timer(String::FromFormat("Remapping vertex and index buffers of: '{}'.", sceneMesh.name));
                    
                    DynamicArray<u32> remap(sceneMesh.indices.Size());
                    u64 uniqueVertices = meshopt_generateVertexRemap(remap.GetData(), sceneMesh.indices.GetData(), sceneMesh.indices.Size(), sceneMesh.vertices.GetData(), sceneMesh.vertices.Size(), sizeof(Vertex));

                    meshopt_remapVertexBuffer(sceneMesh.vertices.GetData(), sceneMesh.vertices.GetData(), sceneMesh.vertices.Size(), sizeof(Vertex), remap.GetData());
                    meshopt_remapIndexBuffer(sceneMesh.indices.GetData(), sceneMesh.indices.GetData(), sceneMesh.indices.Size(), remap.GetData());

                    INFO_LOG("Went from {} vertices to {} vertices.", sceneMesh.vertices.Size(), uniqueVertices);

                    sceneMesh.vertices.Resize(uniqueVertices);
                }

                {
                    ScopedTimer timer(String::FromFormat("Optimization for Vertex Cache and Fetch of: '{}'.", sceneMesh.name));

                    u32 indexCount  = sceneMesh.indices.Size();
                    u32 vertexCount = sceneMesh.vertices.Size();

                    meshopt_optimizeVertexCache(sceneMesh.indices.GetData(), sceneMesh.indices.GetData(), indexCount, vertexCount);
                    meshopt_optimizeVertexFetch(sceneMesh.vertices.GetData(), sceneMesh.indices.GetData(), indexCount, sceneMesh.vertices.GetData(),
                                                vertexCount, sizeof(Vertex));
                }

                scene.meshes.PushBack(sceneMesh);
            }
        }

        {
            // Parse the nodes
            for (const auto& node : asset.nodes)
            {
                if (node.mesh != INVALID_ID)
                {
                    f32 matrix[16];
                    node.TransformWorld(matrix);

                    f32 translation[3];
                    f32 rotation[4];
                    f32 scale[3];
                    DecomposeTransform(translation, rotation, scale, matrix);

                    MeshDraw draw = {};

                    draw.position    = vec3(translation[0], translation[1], translation[2]);
                    draw.scale       = Max(scale[0], Max(scale[1], scale[2]));
                    draw.orientation = quat(rotation[3], rotation[0], rotation[1], rotation[2]);
                    draw.meshIndex   = node.mesh;

                    scene.draws.PushBack(draw);
                }

                if (node.camera != INVALID_ID)
                {
                    f32 matrix[16];
                    node.TransformWorld(matrix);

                    f32 translation[3];
                    f32 rotation[4];
                    f32 scale[3];
                    DecomposeTransform(translation, rotation, scale, matrix);

                    const auto& nodeCam = asset.cameras[node.camera];
                    C3D_ASSERT(nodeCam.type == GLTFCameraType::Perspective);

                    scene.camera.position    = vec3(translation[0], translation[1], translation[2]);
                    scene.camera.orientation = quat(rotation[3], rotation[0], rotation[1], rotation[2]);
                    scene.camera.fovY        = nodeCam.perspective.yFov;
                }
            }

            INFO_LOG("Loaded {} meshes and {} draws.", scene.meshes.Size(), scene.draws.Size());
        }

        return true;
    }

    bool SceneManager::ImportGltfFile(const String& rootPath, SceneAsset& scene)
    {
        INFO_LOG("Importing gltf file: '{}'", scene.path);

        GLTFAsset asset;
        CSONObject gltf;

        {
            ScopedTimer timer(String::FromFormat("Reading GLTF file and parsing JSON."));

            // Read the GLTF file
            if (!m_csonReader.ReadFromFile(scene.path, gltf))
            {
                ERROR_LOG("Failed to parse GLTF file: {}.", scene.path);
                return false;
            }

            // Store off the root path (folder that contains the GLTF file)
            asset.root = rootPath;
        }

        {
            ScopedTimer timer(String::FromFormat("Parsing JSON into GLTFAsset."));

            // Parse the asset property (mandatory)
            if (!ParseAsset(gltf, asset))
            {
                ERROR_LOG("Invalid GLTF file: {}.", scene.path);
                return false;
            }

            // Parse the extensionsUsed property (optional)
            if (!ParseExtensionsUsed(gltf, asset))
            {
                ERROR_LOG("Invalid GLTF file: {}.", scene.path);
                return false;
            }

            // Parse the scene property (optional)
            if (!ParseDefaultScene(gltf, asset))
            {
                ERROR_LOG("Invalid GLTF file: {}.", scene.path);
                return false;
            }

            // Parse the cameras property (optional)
            PARSE_ARRAY_OF_OBJECTS_PROP("cameras", ParseCamera, asset);

            // Parse the buffers property (optional)
            PARSE_ARRAY_OF_OBJECTS_PROP("buffers", ParseBuffer, asset);

            // Parse the bufferViews property (optional)
            PARSE_ARRAY_OF_OBJECTS_PROP("bufferViews", ParseBufferView, asset);

            // Parse the scenes property (optional)
            PARSE_ARRAY_OF_OBJECTS_PROP("scenes", ParseScene, asset);

            // Parse the accessors property (optional)
            PARSE_ARRAY_OF_OBJECTS_PROP("accessors", ParseAccessor, asset);

            // Parse the samplers property (optional)
            PARSE_ARRAY_OF_OBJECTS_PROP("samplers", ParseSampler, asset);

            // Parse the materials property (optional)
            PARSE_ARRAY_OF_OBJECTS_PROP("materials", ParseMaterial, asset);

            // Parse the meshes property (optional)
            PARSE_ARRAY_OF_OBJECTS_PROP("meshes", ParseMesh, asset);

            // Parse the nodes property (optional)
            ParseNodes(gltf, asset);

            // Parse the textures property (optional)
            PARSE_ARRAY_OF_OBJECTS_PROP("textures", ParseTexture, asset);

            // Parse the images property (optional)
            PARSE_ARRAY_OF_OBJECTS_PROP("images", ParseImage, asset);

            // Parse extensions (optional)
            CSONObject extensionsObj;
            if (gltf.GetPropertyValueByName("extensions", extensionsObj))
            {
                if (!ParseExtensions(extensionsObj, asset))
                {
                    return false;
                }
            }
        }

        bool createSceneResult = CreateSceneAsset(asset, scene);

        {
            ScopedTimer timer(String::FromFormat("Cleanup GLTFAsset."));

            // Destroy our generator
            asset.generator.Destroy();

            // Destroy our version
            asset.version.Destroy();

            // Destroy our extensionsUsed
            asset.extensionsUsed.Destroy();

            // Destroy our cameras
            asset.cameras.Destroy();

            // Destroy our buffers
            for (auto& buffer : asset.buffers)
            {
                buffer.Destroy();
            }
            asset.buffers.Destroy();

            // Destroy our bufferViews
            asset.bufferViews.Destroy();

            // Destroy our scenes
            asset.scenes.Destroy();

            // Destroy our accessors
            asset.accessors.Destroy();

            // Destroy our samplers
            asset.samplers.Destroy();

            // Destroy our materials
            for (auto& material : asset.materials)
            {
                for (auto& extension : material.extensions)
                {
                    CleanupGLTFExtension(extension);
                }
            }
            asset.materials.Destroy();

            // Destroy our meshes
            asset.meshes.Destroy();

            // Destroy our nodes
            for (auto& node : asset.nodes)
            {
                for (auto& extension : node.extensions)
                {
                    CleanupGLTFExtension(extension);
                }
            }
            asset.nodes.Destroy();

            // Destroy our textures
            for (auto& texture : asset.textures)
            {
                for (auto& extension : texture.extensions)
                {
                    CleanupGLTFExtension(extension);
                }
            }
            asset.textures.Destroy();

            // Destroy our images
            asset.images.Destroy();

            // Destroy our extensions
            for (auto& extension : asset.extensions)
            {
                CleanupGLTFExtension(extension);
            }
            asset.extensions.Destroy();
        }

        return createSceneResult;
    }

    bool SceneManager::ParseAsset(const CSONObject& gltf, GLTFAsset& asset) const
    {
        CSONObject assetObj;
        if (!gltf.GetPropertyValueByName("asset", assetObj))
        {
            ERROR_LOG("GLTF file does not contain: 'asset' property.");
            return false;
        }

        // Get generator (optional)
        assetObj.GetPropertyValueByName("generator", asset.generator);

        // Get the version (mandatory)
        if (!assetObj.GetPropertyValueByName("version", asset.version))
        {
            ERROR_LOG("GLTF file 'asset' does not contain: 'version'.");
            return false;
        }

        // TODO: extensions
        if (assetObj.HasProperty("extensions"))
        {
            ERROR_LOG("Unsupported 'extensions' property in 'asset'.");
            return false;
        }

        return true;
    }

    bool SceneManager::ParseExtensionsUsed(const CSONObject& gltf, GLTFAsset& asset) const
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

    bool SceneManager::ParseDefaultScene(const CSONObject& gltf, GLTFAsset& asset) const
    {
        gltf.GetPropertyValueByName("scene", asset.defaultScene);
        return true;
    }

    bool SceneManager::ParseCamera(const CSONObject& cameraObj, GLTFAsset& asset) const
    {
        GLTFCamera camera = {};

        // Mandatory type
        String cameraType;
        if (!cameraObj.GetPropertyValueByName("type", cameraType))
        {
            ERROR_LOG("Each camera inside 'cameras' property needs a 'type' property of type string.");
            return false;
        }

        if (cameraType == "perspective")
        {
            camera.type = GLTFCameraType::Perspective;
            if (cameraObj.HasProperty("orthographic"))
            {
                ERROR_LOG("Camera has type 'perspective' but it contains 'orthographic' property.");
                return false;
            }
        }
        else if (cameraType == "orthographic")
        {
            camera.type = GLTFCameraType::Orthographic;
            if (cameraObj.HasProperty("perspective"))
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
        cameraObj.GetPropertyValueByName("name", camera.name);

        if (camera.type == GLTFCameraType::Perspective)
        {
            CSONObject perspectiveObj;
            if (cameraObj.GetPropertyValueByName("perspective", perspectiveObj))  // optional
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
            if (cameraObj.GetPropertyValueByName("orthographic", orthographicObj))  // optional
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

        // TODO: extensions
        if (cameraObj.HasProperty("extensions"))
        {
            ERROR_LOG("Unsupported 'extensions' property in 'camera'.");
            return false;
        }

        // Finally we add this camera to our scene asset
        asset.cameras.PushBack(camera);

        return true;
    }

    bool SceneManager::ParseBuffer(const CSONObject& bufferObj, GLTFAsset& asset) const
    {
        GLTFBuffer buffer;

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

        // TODO: extensions
        if (bufferObj.HasProperty("extensions"))
        {
            ERROR_LOG("Unsupported 'extensions' property in 'buffer'.");
            return false;
        }

        // Finally we add this buffer to our scene asset
        asset.buffers.PushBack(buffer);
        return true;
    }

    bool SceneManager::ParseBufferView(const CSONObject& bufferViewObj, GLTFAsset& asset) const
    {
        GLTFBufferView bufferView;

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

        // TODO: extensions
        if (bufferViewObj.HasProperty("extensions"))
        {
            ERROR_LOG("Unsupported 'extensions' property in 'bufferView'.");
            return false;
        }

        // Finally we add this bufferView to our asset
        asset.bufferViews.PushBack(bufferView);
        return true;
    }

    bool SceneManager::ParseScene(const CSONObject& sceneObj, GLTFAsset& asset) const
    {
        GLTFScene scene;

        // Optional
        sceneObj.GetPropertyValueByName("name", scene.name);

        // Optional
        CSONArray nodes;
        if (sceneObj.GetPropertyValueByName("nodes", nodes))
        {
            for (const auto n : nodes.properties)
            {
                if (n.HoldsInteger())
                {
                    scene.nodes.EmplaceBack(n.GetU32());
                }
            }
        }

        // TODO: extensions
        if (sceneObj.HasProperty("extensions"))
        {
            ERROR_LOG("Unsupported 'extensions' property in 'scene'.");
            return false;
        }

        asset.scenes.PushBack(scene);

        return true;
    }

    static bool ParseAccessorType(const CSONObject& accessorObj, GLTFAccessor& accessor)
    {
        String type;
        if (!accessorObj.GetPropertyValueByName("type", type))
        {
            ERROR_LOG("Each 'accessor' in the 'accessors' property must contain a 'type' property.");
            return false;
        }

        if (type.StartsWith("V"))
        {
            if (type == "VEC2")
            {
                accessor.type        = GLTFAccessorType::Vec2;
                accessor.elementSize = accessor.componentSize * 2;
                return true;
            }
            else if (type == "VEC3")
            {
                accessor.type        = GLTFAccessorType::Vec3;
                accessor.elementSize = accessor.componentSize * 3;
                return true;
            }
            else if (type == "VEC4")
            {
                accessor.type        = GLTFAccessorType::Vec4;
                accessor.elementSize = accessor.componentSize * 4;
                return true;
            }
        }

        if (type.StartsWith("M"))
        {
            if (type == "MAT2")
            {
                accessor.type        = GLTFAccessorType::Mat2;
                accessor.elementSize = accessor.componentSize * 4;
                return true;
            }
            else if (type == "MAT3")
            {
                accessor.type        = GLTFAccessorType::Mat3;
                accessor.elementSize = accessor.componentSize * 9;
                return true;
            }
            else if (type == "MAT4")
            {
                accessor.type        = GLTFAccessorType::Mat4;
                accessor.elementSize = accessor.componentSize * 16;
                return true;
            }
        }

        if (type == "SCALAR")
        {
            accessor.type        = GLTFAccessorType::Scalar;
            accessor.elementSize = accessor.componentSize;
            return true;
        }

        ERROR_LOG("Accessor contains invalid type: '{}'.", type);
        return false;
    }

    static bool ParseAccessorComponentType(const CSONObject& accessorObj, GLTFAccessor& accessor)
    {
        if (!accessorObj.GetPropertyValueByName("componentType", accessor.componentType))
        {
            ERROR_LOG("Each 'accessor' in the 'accessors' property must contain a 'componentType' property.");
            return false;
        }

        switch (accessor.componentType)
        {
            case GLTF_BYTE:
            case GLTF_UNSIGNED_BYTE:
                accessor.componentSize = 1;
                break;
            case GLTF_SHORT:
            case GLTF_UNSIGNED_SHORT:
                accessor.componentSize = 2;
                break;
            case GLTF_UNSIGNED_INT:
            case GLTF_FLOAT:
                accessor.componentSize = 4;
                break;
            default:
                ERROR_LOG("'accessor' has an unknown 'componentType': {}.", accessor.componentType);
                return false;
        }

        return true;
    }

    bool SceneManager::ParseAccessor(const CSONObject& accessorObj, GLTFAsset& asset) const
    {
        GLTFAccessor accessor;

        // Mandatory componentType
        if (!ParseAccessorComponentType(accessorObj, accessor))
        {
            return false;
        }

        // Mandatory type
        if (!ParseAccessorType(accessorObj, accessor))
        {
            return false;
        }

        // Mandatory count
        if (!accessorObj.GetPropertyValueByName("count", accessor.count))
        {
            ERROR_LOG("Each 'accessor' in the 'accessors' property must contain a 'count' property.");
            return false;
        }

        // Optional bufferView
        accessorObj.GetPropertyValueByName("bufferView", accessor.bufferView);

        // Optional byteOffset
        accessorObj.GetPropertyValueByName("byteOffset", accessor.byteOffset);

        // Optional normalized
        accessorObj.GetPropertyValueByName("normalized", accessor.normalized);

        // Optional min
        CSONArray minArray;
        if (accessorObj.GetPropertyValueByName("min", minArray))
        {
            for (const auto& prop : minArray.properties)
            {
                if (!prop.HoldsNumber())
                {
                    return false;
                }
                accessor.min.EmplaceBack(prop.GetF32());
            }
        }

        // Optional max
        CSONArray maxArray;
        if (accessorObj.GetPropertyValueByName("max", maxArray))
        {
            for (const auto& prop : maxArray.properties)
            {
                if (!prop.HoldsNumber())
                {
                    return false;
                }
                accessor.max.EmplaceBack(prop.GetF32());
            }
        }

        // Optional name
        accessorObj.GetPropertyValueByName("name", accessor.name);

        // TODO: extensions
        if (accessorObj.HasProperty("extensions"))
        {
            ERROR_LOG("Unsupported 'extensions' property in 'accessor'.");
            return false;
        }

        // Finally add the accessor to our asset
        asset.accessors.PushBack(accessor);

        return true;
    }

    bool SceneManager::ParseSampler(const CSONObject& samplerObj, GLTFAsset& asset) const
    {
        GLTFSampler sampler;

        // Optional name
        samplerObj.GetPropertyValueByName("name", sampler.name);

        // Optional magFilter
        samplerObj.GetPropertyValueByName("magFilter", sampler.magFilter);

        // Optional minFilter
        samplerObj.GetPropertyValueByName("minFilter", sampler.minFilter);

        // Optional wrapS
        samplerObj.GetPropertyValueByName("wrapS", sampler.wrapS);

        // Optional wrapT
        samplerObj.GetPropertyValueByName("wrapT", sampler.wrapT);

        // TODO: extensions
        if (samplerObj.HasProperty("extensions"))
        {
            ERROR_LOG("Unsupported 'extensions' property in 'sampler'.");
            return false;
        }

        // Finally add the sampler to our asset
        asset.samplers.PushBack(sampler);

        return true;
    }

    bool SceneManager::ParseMaterial(const CSONObject& materialObj, GLTFAsset& asset) const
    {
        GLTFMaterial material;

        // Optional name
        materialObj.GetPropertyValueByName("name", material.name);

        // Optional material extensions
        CSONObject extensionsObj;
        if (!materialObj.GetPropertyValueByName("extensions", extensionsObj))
        {
            if (!ParseMaterialExtensions(extensionsObj, material.extensions))
            {
                return false;
            }
        }

        // Optional pbrMetallicRoughness
        CSONObject pbrMetallicRoughnessObj;
        if (materialObj.GetPropertyValueByName("pbrMetallicRoughness", pbrMetallicRoughnessObj))
        {
            if (!ParsePBR(pbrMetallicRoughnessObj, material.pbr))
            {
                return false;
            }
        }

        // Optional normalTexture
        CSONObject normalTextureObj;
        if (materialObj.GetPropertyValueByName("normalTexture", normalTextureObj))
        {
            if (!ParseNormalTexture(normalTextureObj, material.normalTexture))
            {
                return false;
            }
        }

        // Optional occlusionTexture
        CSONObject occlusionTextureObj;
        if (materialObj.GetPropertyValueByName("occlusionTexture", occlusionTextureObj))
        {
            if (!ParseOcclusionTexture(occlusionTextureObj, material.occlusionTexture))
            {
                return false;
            }
        }

        // Optional emissiveTexture
        CSONObject emissiveTextureObj;
        if (materialObj.GetPropertyValueByName("emissiveTexture", emissiveTextureObj))
        {
            if (!ParseTextureInfo(emissiveTextureObj, material.emissiveTexture))
            {
                return false;
            }
        }

        // Optional emissiveFactor
        PARSE_ARRAY_OF_NUMBERS(materialObj, "emissiveFactor", 3, material.emissiveFactor);

        // Optional alphaMode
        String alphaMode;
        if (materialObj.GetPropertyValueByName("alphaMode", alphaMode))
        {
            if (alphaMode == "OPAQUE")
            {
                material.alphaMode = GLTFMaterialAlphaMode::Opaque;
            }
            else if (alphaMode == "MASK")
            {
                material.alphaMode = GLTFMaterialAlphaMode::Mask;
            }
            else if (alphaMode == "BLEND")
            {
                material.alphaMode = GLTFMaterialAlphaMode::Blend;
            }
            else
            {
                ERROR_LOG("A material has unknown 'alphaMode' property: '{}'.", alphaMode);
                return false;
            }
        }

        // Optional alphaCutoff
        materialObj.GetPropertyValueByName("alphaCutoff", material.alphaCutoff);

        // Optional doubleSided
        materialObj.GetPropertyValueByName("doubleSided", material.doubleSided);

        // Finally add the material to our asset
        asset.materials.PushBack(material);

        return true;
    }

    bool SceneManager::ParsePBRSpecularGlossinessExtension(const CSONObject& extensionObj, GLTFExtension& extension) const
    {
        // Allocate the correct extension type
        auto& ext = extension.Allocate<GLTFPBRSpecularGlossinessExtension>();

        // Optional diffuseFactor
        PARSE_ARRAY_OF_NUMBERS(extensionObj, "diffuseFactor", 4, ext.diffuseFactor);

        // Optional diffuseTexture
        CSONObject diffuseTextureObj;
        if (extensionObj.GetPropertyValueByName("diffuseTexture", diffuseTextureObj))
        {
            if (!ParseTextureInfo(diffuseTextureObj, ext.diffuseTexture))
            {
                return false;
            }
        }

        // Optional specularFactor
        PARSE_ARRAY_OF_NUMBERS(extensionObj, "specularFactor", 3, ext.specularFactor);

        // Optional glossinessFactor
        extensionObj.GetPropertyValueByName("glossinessFactor", ext.glossinessFactor);

        // Optional specularGlossinessTexture
        CSONObject specularGlossinessTextureObj;
        if (extensionObj.GetPropertyValueByName("diffuseTexture", specularGlossinessTextureObj))
        {
            if (!ParseTextureInfo(specularGlossinessTextureObj, ext.specularGlossinessTexture))
            {
                return false;
            }
        }

        return true;
    }

    bool SceneManager::ParseTransmissionExtension(const CSONObject& extensionObj, GLTFExtension& extension) const
    {
        // Allocate the correct extension type
        auto& ext = extension.Allocate<GLTFTransmissionExtension>();

        // Optional transmissionFactor
        extensionObj.GetPropertyValueByName("transmissionFactor", ext.transmissionFactor);

        // Optional transmissionTexture
        CSONObject transmissionTextureObj;
        if (extensionObj.GetPropertyValueByName("diffuseTexture", transmissionTextureObj))
        {
            if (!ParseTextureInfo(transmissionTextureObj, ext.transmissionTexture))
            {
                return false;
            }
        }

        return true;
    }

    bool SceneManager::ParseMaterialExtensions(const CSONObject& extensionsObj, DynamicArray<GLTFExtension>& materialExtensions) const
    {
        for (const auto& extensionProp : extensionsObj.properties)
        {
            if (!extensionProp.HoldsObject())
            {
                ERROR_LOG("Material 'extensions' property should contain an object with key value pairs.");
                return false;
            }

            auto& extension = materialExtensions.EmplaceBack();

            const auto& extensionObj = extensionProp.GetObject();
            if (extensionProp.name == KHR_MATERIALS_PBR_SPECULAR_GLOSSINESS)
            {
                extension.type = GLTFExtensionType::PBRSpecularGlossiness;
                if (!ParsePBRSpecularGlossinessExtension(extensionObj, extension))
                {
                    return false;
                }
            }
            else if (extensionProp.name == KHR_MATERIALS_TRANSMISSION)
            {
                extension.type = GLTFExtensionType::Transmission;
                if (!ParseTransmissionExtension(extensionObj, extension))
                {
                    return false;
                }
            }
            else
            {
                ERROR_LOG("Unsupported material extension: '{}'.", extensionProp.name);
                return false;
            }
        }

        return true;
    }

    bool SceneManager::ParsePBR(const CSONObject& pbrObj, GLTFPBR& pbr) const
    {
        // Optional baseColorFactor
        PARSE_ARRAY_OF_NUMBERS(pbrObj, "baseColorFactor", 4, pbr.baseColorFactor);

        // Optional baseColorTexture
        CSONObject baseColorTextureObj;
        if (pbrObj.GetPropertyValueByName("baseColorTexture", baseColorTextureObj))
        {
            if (!ParseTextureInfo(baseColorTextureObj, pbr.baseColorTexture))
            {
                return false;
            }
        }

        // Optional metallicFactor
        pbrObj.GetPropertyValueByName("metallicFactor", pbr.metallicFactor);

        // Optional roughnessFactor
        pbrObj.GetPropertyValueByName("roughnessFactor", pbr.roughnessFactor);

        // Optional metallicRoughnessTexture
        CSONObject metallicRoughnessTextureObj;
        if (pbrObj.GetPropertyValueByName("metallicRoughnessTexture", metallicRoughnessTextureObj))
        {
            if (!ParseTextureInfo(metallicRoughnessTextureObj, pbr.metallicRoughnessTexture))
            {
                return false;
            }
        }

        return true;
    }

    bool SceneManager::ParseNormalTexture(const CSONObject& normalTextureOjb, GLTFNormalTexture& normalTexture) const
    {
        // TextureInfo
        if (!ParseTextureInfo(normalTextureOjb, normalTexture.info))
        {
            return false;
        }

        // Optional scale
        normalTextureOjb.GetPropertyValueByName("scale", normalTexture.scale);

        return true;
    }

    bool SceneManager::ParseOcclusionTexture(const CSONObject& occlusionTextureOjb, GLTFOcclusionTexture& occlusionTexture) const
    {
        // TextureInfo
        if (!ParseTextureInfo(occlusionTextureOjb, occlusionTexture.info))
        {
            return false;
        }

        // Optional strength
        occlusionTextureOjb.GetPropertyValueByName("strength", occlusionTexture.strength);

        return true;
    }

    bool SceneManager::ParseTextureInfo(const CSONObject& textureInfoObj, GLTFTextureInfo& texInfo) const
    {
        // Mandatory index
        if (!textureInfoObj.GetPropertyValueByName("index", texInfo.index))
        {
            ERROR_LOG("A TextureInfo property must contain an index.");
            return false;
        }

        // Optional texCoord
        textureInfoObj.GetPropertyValueByName("texCoord", texInfo.texCoord);

        return true;
    }

    bool SceneManager::ParseMesh(const CSONObject& meshObj, GLTFAsset& asset) const
    {
        GLTFMesh mesh;

        // Optional primitives
        CSONArray primitiveArray;
        if (!meshObj.GetPropertyValueByName("primitives", primitiveArray))
        {
            ERROR_LOG("Each 'mesh' inside the 'meshes' property must contain a 'primitives' property.");
            return false;
        }

        for (const auto& primitiveProp : primitiveArray.properties)
        {
            if (!primitiveProp.HoldsObject())
            {
                ERROR_LOG("Each 'primitive' inside the 'primitives' property must be an object.");
                return false;
            }

            GLTFMeshPrimitive primitive;

            const auto& primitiveObj = primitiveProp.GetObject();
            if (!ParseMeshPrimitive(primitiveObj, primitive))
            {
                return false;
            }

            mesh.primitives.PushBack(primitive);
        }

        // Optional weights
        PARSE_UNBOUNDED_ARRAY_OF_NUMBERS(meshObj, "weights", mesh.weights);

        // Optional name
        meshObj.GetPropertyValueByName("name", mesh.name);

        // TODO: extensions
        if (meshObj.HasProperty("extensions"))
        {
            ERROR_LOG("Unsupported 'extensions' property in 'mesh'.");
            return false;
        }

        // Finally add the mesh to our asset
        asset.meshes.PushBack(mesh);

        return true;
    }

    bool SceneManager::ParseMeshPrimitive(const CSONObject& primitiveObj, GLTFMeshPrimitive& primitive) const
    {
        // Mandatory attributes
        if (!primitiveObj.GetPropertyValueByName("attributes", primitive.attributes))
        {
            ERROR_LOG("Each 'primitive' inside the 'primitives' property must contain a 'attributes' property.");
            return false;
        }

        // Optional indices
        primitiveObj.GetPropertyValueByName("indices", primitive.indices);

        // Optional material
        primitiveObj.GetPropertyValueByName("material", primitive.material);

        // Optional mode
        primitiveObj.GetPropertyValueByName("mode", primitive.mode);

        // TODO: targets
        if (primitiveObj.HasProperty("targets"))
        {
            ERROR_LOG("Unsupported 'target' property in 'primitive'.");
            return false;
        }

        // TODO: extensions
        if (primitiveObj.HasProperty("extensions"))
        {
            ERROR_LOG("Unsupported 'extensions' property in 'primitive'.");
            return false;
        }

        return true;
    }

    bool SceneManager::ParseNodes(const CSONObject& gltf, GLTFAsset& asset) const
    {
        PARSE_ARRAY_OF_OBJECTS_PROP("nodes", ParseNode, asset);

        // We need to proces the nodes to add parent and children pointers
        for (auto& node : asset.nodes)
        {
            for (auto& childIndex : node.childrenIndices)
            {
                auto& child  = asset.nodes[childIndex];
                child.parent = &node;
                node.children.PushBack(&child);
            }
        }

        return true;
    }

    bool SceneManager::ParseNode(const CSONObject& nodeObj, GLTFAsset& asset) const
    {
        GLTFNode node;

        // Optional camera
        nodeObj.GetPropertyValueByName("camera", node.camera);

        // Optional children
        PARSE_UNBOUNDED_ARRAY_OF_INTEGERS(nodeObj, "children", node.childrenIndices);

        // Optional skin
        nodeObj.GetPropertyValueByName("skin", node.skin);

        // Optional matrix
        if (nodeObj.HasProperty("matrix"))
        {
            PARSE_ARRAY_OF_NUMBERS(nodeObj, "matrix", 16, node.matrix);
            node.hasMatrix = true;
        }

        // Optional mesh
        nodeObj.GetPropertyValueByName("mesh", node.mesh);

        // Optional rotation
        PARSE_ARRAY_OF_NUMBERS(nodeObj, "rotation", 4, node.rotation);

        // Optional scale
        PARSE_ARRAY_OF_NUMBERS(nodeObj, "scale", 3, node.scale);

        // Optional translation
        PARSE_ARRAY_OF_NUMBERS(nodeObj, "translation", 3, node.translation);

        // Optional weights
        PARSE_UNBOUNDED_ARRAY_OF_NUMBERS(nodeObj, "weights", node.weights);

        // Optional name
        nodeObj.GetPropertyValueByName("name", node.name);

        // Optional extensions
        CSONObject extensionsObj;
        if (nodeObj.GetPropertyValueByName("extensions", extensionsObj))
        {
            if (!ParseNodeExtensions(extensionsObj, node.extensions))
            {
                return false;
            }
        }

        // Finally add the node to our asset
        asset.nodes.PushBack(node);

        return true;
    }

    bool SceneManager::ParseLightsPunctualNode(const CSONObject& extensionObj, GLTFExtension& extension) const
    {
        // Allocate the correct extension type
        auto& ext = extension.Allocate<GLTFNodeLightsPunctualExtension>();

        // Mandatory light
        if (!extensionObj.GetPropertyValueByName("light", ext.light))
        {
            ERROR_LOG("'{}' must contain 'light' property.", KHR_LIGHTS_PUNCTUAL);
            return false;
        }

        return true;
    }

    bool SceneManager::ParseNodeExtensions(const CSONObject& nodeExtensionsObj, DynamicArray<GLTFExtension>& nodeExtensions) const
    {
        GLTFExtension extension;

        for (const auto& extensionProp : nodeExtensionsObj.properties)
        {
            if (!extensionProp.HoldsObject())
            {
                ERROR_LOG("'extensions' property may only hold objects.");
                return false;
            }

            if (extensionProp.name == KHR_LIGHTS_PUNCTUAL)
            {
                extension.type = GLTFExtensionType::NodeLightsPunctual;

                if (!ParseLightsPunctualNode(extensionProp.GetObject(), extension))
                {
                    return false;
                }
            }
            else
            {
                ERROR_LOG("Unsupported extension: '{}' in 'node'.", extensionProp.name);
                return false;
            }
        }

        // Finally add the extenion to our extension array
        nodeExtensions.PushBack(extension);

        return true;
    }

    bool SceneManager::ParseTexture(const CSONObject& textureObj, GLTFAsset& asset) const
    {
        GLTFTexture texture;

        // Optional sampler
        textureObj.GetPropertyValueByName("sampler", texture.sampler);

        // Optional source
        textureObj.GetPropertyValueByName("source", texture.source);

        // Optional name
        textureObj.GetPropertyValueByName("name", texture.name);

        // Optional extensions
        CSONObject extensionsObj;
        if (textureObj.GetPropertyValueByName("extensions", extensionsObj))
        {
            if (!ParseTextureExtensions(extensionsObj, texture.extensions))
            {
                return false;
            }
        }

        // Finally add the texture to our asset
        asset.textures.PushBack(texture);

        return true;
    }

    bool SceneManager::ParseTextureDDS(const CSONObject& textureDDSObj, GLTFExtension& textureExtension) const
    {
        auto& ext = textureExtension.Allocate<GLTFTextureDDS>();

        // Mandatory source
        if (!textureDDSObj.GetPropertyValueByName("source", ext.source))
        {
            ERROR_LOG("{} must contain a 'source' property.", MSFT_TEXTURE_DDS);
            return false;
        }

        return true;
    }

    bool SceneManager::ParseTextureExtensions(const CSONObject& textureExtensionsObj, DynamicArray<GLTFExtension>& textureExtensions) const
    {
        GLTFExtension extension;

        for (const auto& extensionProp : textureExtensionsObj.properties)
        {
            if (!extensionProp.HoldsObject())
            {
                ERROR_LOG("'extensions' property may only hold objects.");
                return false;
            }

            if (extensionProp.name == MSFT_TEXTURE_DDS)
            {
                extension.type = GLTFExtensionType::TextureDDS;

                if (!ParseTextureDDS(extensionProp.GetObject(), extension))
                {
                    return false;
                }
            }
            else
            {
                ERROR_LOG("Unsupported extension: '{}' in 'texture'.", extensionProp.name);
                return false;
            }
        }

        // Finally add the extenion to our extension array
        textureExtensions.PushBack(extension);

        return true;
    }

    bool SceneManager::ParseImage(const CSONObject& imageObj, GLTFAsset& asset) const
    {
        GLTFImage image;

        // Optional uri
        imageObj.GetPropertyValueByName("uri", image.uri);

        // Optional mimeType
        imageObj.GetPropertyValueByName("mimeType", image.mimeType);

        // Optional bufferView
        imageObj.GetPropertyValueByName("bufferView", image.bufferView);

        // Optional name
        imageObj.GetPropertyValueByName("name", image.name);

        // TODO: extensions
        if (imageObj.HasProperty("extensions"))
        {
            ERROR_LOG("Unsupported 'extensions' property in 'image'.");
            return false;
        }

        // Finally add the image to our asset
        asset.images.PushBack(image);

        return true;
    }

    bool SceneManager::ParseExtensions(const CSONObject& extensionsObj, GLTFAsset& asset) const
    {
        GLTFExtension extension;

        for (const auto& extensionProp : extensionsObj.properties)
        {
            if (!extensionProp.HoldsObject())
            {
                ERROR_LOG("Each 'extension' in the 'extensions' property must be an object.");
                return false;
            }

            const auto& extensionObj = extensionProp.GetObject();
            if (extensionProp.name == KHR_LIGHTS_PUNCTUAL)
            {
                extension.type = GLTFExtensionType::LightsPunctual;
                if (!ParseLightsPunctual(extensionObj, extension))
                {
                    return false;
                }
            }
            else
            {
                ERROR_LOG("Unsupported extension type: '{}'.", extensionProp.name);
                return false;
            }
        }

        // Finally add the extension to our asset
        asset.extensions.PushBack(extension);

        return true;
    }

    bool SceneManager::ParseLightsPunctual(const CSONObject& lightsObj, GLTFExtension& sceneExtension) const
    {
        auto& ext = sceneExtension.Allocate<GLTFLightsPunctualExtension>();

        GLTFLightPunctual light;

        // Mandatory array lights
        CSONArray lightsArray;
        if (!lightsObj.GetPropertyValueByName("lights", lightsArray))
        {
            ERROR_LOG("'{}' must contain 'lights' property.", KHR_LIGHTS_PUNCTUAL);
            return false;
        }

        for (const auto& lightProp : lightsArray.properties)
        {
            if (!lightProp.HoldsObject())
            {
                ERROR_LOG("The 'lights' property must be an array of objects.");
                return false;
            }

            const auto& lightObj = lightProp.GetObject();

            // Optional name
            lightObj.GetPropertyValueByName("name", light.name);

            // Optional color
            PARSE_ARRAY_OF_NUMBERS(lightObj, "color", 3, light.color);

            // Optional intensity
            lightObj.GetPropertyValueByName("intensity", light.intensity);

            // Mandatory type
            String typeStr;
            if (!lightObj.GetPropertyValueByName("type", typeStr))
            {
                ERROR_LOG("Every 'light' inside 'lights' property must contain a 'type' property.");
                return false;
            }

            if (typeStr == "directional")
            {
                light.type = GLTFLightPunctualType::Directional;
            }
            else if (typeStr == "point")
            {
                light.type = GLTFLightPunctualType::Point;
            }
            else
            {
                ERROR_LOG("Unsupported 'light' type: '{}'.", typeStr);
                return false;
            }

            lightObj.GetPropertyValueByName("range", light.range);
        }

        // Finally add the light to our lights array
        ext.lights.PushBack(light);

        return true;
    }

}  // namespace C3D