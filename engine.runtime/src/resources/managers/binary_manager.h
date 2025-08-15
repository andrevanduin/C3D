
#pragma once
#include "resource_manager.h"

namespace C3D
{
    struct BinaryResource final : IResource
    {
        BinaryResource() : IResource(ResourceType::Binary) {}

        char* data = nullptr;
        u64 size   = 0;
    };

    template <>
    class C3D_API ResourceManager<BinaryResource> final : public IResourceManager
    {
    public:
        ResourceManager();

        bool Read(const String& name, BinaryResource& resource);
        void Cleanup(BinaryResource& resource) const;
    };
}  // namespace C3D