
#include "texture_manager.h"

#include "assets/deserializers/dds_deserializer.h"
#include "logger/logger.h"

namespace C3D
{
    TextureManager::TextureManager() : IAssetManager(MemoryType::Texture, AssetType::Texture, "textures") {}

    bool TextureManager::Read(const String& path, TextureAsset& asset)
    {
        if (path.EndsWith(".dds"))
        {
            // Parse a DDS file
            DDSDeserializer ddsDeserializer;

            if (!ddsDeserializer.Deserialize(path, asset))
            {
                ERROR_LOG("Failed to deserialize: '{}'.", path);
                return false;
            }

            INFO_LOG("Successfully parsed all: {} bytes of DDS file: '{}'.", asset.size, path);
        }
        else
        {
            ERROR_LOG("Unsupported extension for reading file: '{}'.", path);
            return false;
        }

        return true;
    }

    void TextureManager::Cleanup(TextureAsset& asset) {}
}  // namespace C3D