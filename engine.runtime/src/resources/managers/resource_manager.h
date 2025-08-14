
#pragma once
#include "logger/logger.h"
#include "resources/types.h"

namespace C3D
{
    class C3D_API IResourceManager
    {
    public:
        IResourceManager(MemoryType memoryType, ResourceType type, const char* customType, const char* path);

        virtual bool Init() { return true; }
        virtual void Shutdown() {}

        u16 id = INVALID_ID_U16;
        ResourceType type;

        String customType;
        String typePath;

    protected:
        MemoryType m_memoryType;
    };

    template <typename T>
    class ResourceManager : public IResourceManager
    {
    public:
        explicit ResourceManager() : IResourceManager(MemoryType::Unknown, ResourceType::None, nullptr, nullptr) {}

        ResourceManager(MemoryType memoryType, ResourceType type, const char* customType, const char* path)
            : IResourceManager(memoryType, type, customType, path)
        {}

        bool Read(const String& name, T& resource) const;
        bool Write(const T& resource) const;
        void Cleanup(T& resource) const;
    };
}  // namespace C3D
