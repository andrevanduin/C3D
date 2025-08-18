
#include "config_system.h"

namespace C3D
{
    bool ConfigSystem::OnInit(const CSONObject& config)
    {
        INFO_LOG("Initializing config system.");
        m_config = config;
        return true;
    }

    bool ConfigSystem::GetProperty(const String& name, u64& value)
    {
        if (!m_config.HasProperty(name))
        {
            ERROR_LOG("Config property does not exist.");
            return false;
        }
        return m_config.GetPropertyValueByName(name, value);
    }

    bool ConfigSystem::GetProperty(const String& name, String& value)
    {
        if (!m_config.HasProperty(name))
        {
            ERROR_LOG("Config property does not exist.");
            return false;
        }
        return m_config.GetPropertyValueByName(name, value);
    }

    void ConfigSystem::OnShutdown()
    {
        INFO_LOG("Shutting down config system.");
        m_config.properties.Destroy();
    }

}  // namespace C3D