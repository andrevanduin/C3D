
#include "resource_system.h"

#include "cson/cson_types.h"
#include "logger/logger.h"

// Default loaders
#include "resources/managers/binary_manager.h"

namespace C3D
{
    const static String empty = "";

    ResourceSystem::ResourceSystem()
    {
        m_resourceManagerTypes[ToUnderlying(ResourceType::None)]   = "None";
        m_resourceManagerTypes[ToUnderlying(ResourceType::Binary)] = "Binary";
        m_resourceManagerTypes[ToUnderlying(ResourceType::Custom)] = "Custom";
    }

    bool ResourceSystem::OnInit(const CSONObject& config)
    {
        // Parse the user provided config
        for (const auto& prop : config.properties)
        {
            if (prop.name.IEquals("assetBasePath"))
            {
                m_config.assetBasePath = prop.GetString();
            }
        }

        if (m_config.maxLoaderCount == 0)
        {
            FATAL_LOG("Failed because config.maxLoaderCount == 0.");
            return false;
        }

        m_initialized = true;

        const auto binaryLoader = Memory.New<ResourceManager<BinaryResource>>(MemoryType::ResourceLoader);

        IResourceManager* managers[] = { binaryLoader };

        m_registeredManagers.Resize(16);

        for (const auto manager : managers)
        {
            if (!RegisterManager(manager))
            {
                FATAL_LOG("Failed for '{}' manager.", m_resourceManagerTypes[ToUnderlying(manager->type)]);
                return false;
            }
        }

        INFO_LOG("Initialized successfully with base path '{}'.", m_config.assetBasePath);
        return true;
    }

    void ResourceSystem::OnShutdown()
    {
        INFO_LOG("Destroying all registered loaders.");
        for (const auto manager : m_registeredManagers)
        {
            if (manager)
            {
                INFO_LOG("{}Manager shutdown.", m_resourceManagerTypes[ToUnderlying(manager->type)]);
                manager->Shutdown();
                Memory.Delete(manager);
            }
        }
        m_registeredManagers.Destroy();
    }

    bool ResourceSystem::RegisterManager(IResourceManager* newManager)
    {
        if (!m_initialized) return false;

        if (newManager->id == INVALID_ID_U16)
        {
            ERROR_LOG("Manager has an invalid id.");
            return false;
        }

        if (m_registeredManagers[newManager->id])
        {
            ERROR_LOG("Manager at index: {} already exists.", newManager->id);
            return false;
        }

        m_registeredManagers[newManager->id] = newManager;

        INFO_LOG("{}Manager registered.", m_resourceManagerTypes[ToUnderlying(newManager->type)]);
        return true;
    }

    const String& ResourceSystem::GetBasePath() const
    {
        if (m_initialized) return m_config.assetBasePath;

        ERROR_LOG("Called before initialization. Returning empty string.");
        return empty;
    }
}  // namespace C3D
