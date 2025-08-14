
#pragma once
#include "containers/dynamic_array.h"
#include "defines.h"
#include "logger/logger.h"
#include "resources/managers/resource_manager.h"
#include "resources/types.h"
#include "systems/system.h"

namespace C3D
{
    struct ResourceSystemConfig
    {
        u32 maxLoaderCount = 32;
        String assetBasePath;  // Relative base path
    };

    class ResourceSystem final : public SystemWithConfig
    {
    public:
        ResourceSystem();

        bool OnInit(const CSONObject& config) override;

        void OnShutdown() override;

        C3D_API bool RegisterManager(IResourceManager* newManager);

        template <typename Type>
        C3D_API bool Write(Type& resource)
        {
            static_assert(std::is_base_of_v<IResource, Type>, "A writable resource must extend the IResource type.");

            auto manager = dynamic_cast<ResourceManager<Type>*>(m_registeredManagers[ToUnderlying(resource.resourceType)]);
            return manager->Write(resource);
        }

        template <typename Type, typename Params>
        C3D_API bool Read(const String& name, Type& resource, const Params& params)
        {
            static_assert(std::is_base_of_v<IResource, Type>, "A readable resource must extend the IResource type.");

            auto manager = dynamic_cast<ResourceManager<Type>*>(m_registeredManagers[ToUnderlying(resource.resourceType)]);
            return manager->Read(name, resource, params);
        }

        template <typename Type>
        C3D_API bool Read(const String& name, Type& resource)
        {
            static_assert(std::is_base_of_v<IResource, Type>, "A readable resource must extend the IResource type.");

            auto manager = dynamic_cast<ResourceManager<Type>*>(m_registeredManagers[ToUnderlying(resource.resourceType)]);
            return manager->Read(name, resource);
        }

        template <typename Type>
        C3D_API void Cleanup(Type& resource) const
        {
            static_assert(std::is_base_of_v<IResource, Type>, "A resource must extend the IResource type.");

            auto manager = dynamic_cast<ResourceManager<Type>*>(m_registeredManagers[ToUnderlying(resource.resourceType)]);
            manager->Cleanup(resource);
        }

        C3D_API const String& GetBasePath() const;

    private:
        ResourceSystemConfig m_config;

        DynamicArray<IResourceManager*> m_registeredManagers;

        const char* m_resourceManagerTypes[ToUnderlying(ResourceType::MaxValue)];
    };
}  // namespace C3D
