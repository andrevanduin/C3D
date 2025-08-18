
#include "asset_manager.h"

#include "config/config_system.h"
#include "system/system_manager.h"

namespace C3D
{
    IAssetManager::IAssetManager(MemoryType memoryType, AssetType type, const char* subFolder)
        : m_memoryType(memoryType), m_assetType(type), m_subFolder(subFolder)
    {
        Config.GetProperty("AssetBasePath", m_assetPath);
    }
}  // namespace C3D