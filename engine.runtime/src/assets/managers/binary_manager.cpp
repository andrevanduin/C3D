
#include "binary_manager.h"

#include "logger/logger.h"
#include "platform/file_system.h"
#include "system/system_manager.h"

namespace C3D
{
    BinaryManager::BinaryManager() : IAssetManager(MemoryType::Array, AssetType::Binary, "shaders") {}

    bool BinaryManager::Read(const String& name, BinaryAsset& asset)
    {
        if (name.Empty())
        {
            ERROR_LOG("No valid name was provided.")
            return false;
        }

        // TODO: try different extensions
        auto fullPath = String::FromFormat("{}/{}/{}", m_assetPath, m_subFolder, name);

        File file;
        if (!file.Open(fullPath, FileModeRead | FileModeBinary))
        {
            ERROR_LOG("Unable to open file for binary reading: '{}'.", fullPath);
            return false;
        }

        asset.path = fullPath;

        u64 fileSize = 0;
        if (!file.Size(&fileSize))
        {
            ERROR_LOG("Unable to binary read size of file: '{}'.", fullPath);
            file.Close();
            return false;
        }

        asset.data = Memory.Allocate<char>(MemoryType::Array, fileSize);
        asset.name = name;

        if (!file.ReadAll(asset.data, &asset.size))
        {
            ERROR_LOG("Unable to read binary file: '{}'.", fullPath);
            file.Close();
            return false;
        }

        file.Close();
        return true;
    }

    void BinaryManager::Cleanup(BinaryAsset& asset)
    {
        if (asset.data)
        {
            Memory.Free(asset.data);
            asset.data = nullptr;
        }

        asset.name.Destroy();
        asset.path.Destroy();
    }
}  // namespace C3D
