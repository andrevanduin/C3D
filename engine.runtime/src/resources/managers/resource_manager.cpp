
#include "resource_manager.h"

#include "logger/logger.h"

namespace C3D
{
    IResourceManager::IResourceManager(const MemoryType memoryType, const ResourceType type, const char* customType, const char* path)
        : id(ToUnderlying(type)), type(type), customType(customType), typePath(path), m_memoryType(memoryType)
    {}
}  // namespace C3D
