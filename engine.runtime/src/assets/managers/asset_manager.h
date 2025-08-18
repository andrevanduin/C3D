
#pragma once
#include "assets/types.h"
#include "logger/logger.h"
#include "platform/file_system.h"
#include "string/string.h"

namespace C3D
{
    class C3D_API IAssetManager
    {
    public:
        IAssetManager(MemoryType memoryType, AssetType type, const char* subFolder);

    protected:
        /** @brief The memory type used for allocations done by this manager. */
        MemoryType m_memoryType;
        /** @brief The type of asset managed by this manager. */
        AssetType m_assetType;
        /** @brief The base path of all assets. */
        String m_assetPath;
        /** @brief The subfolder where this asset type is stored (starting from the base asset folder) */
        const char* m_subFolder = nullptr;
        /** @brief File object used to access the file on disk. */
        File m_file;
    };
}  // namespace C3D
