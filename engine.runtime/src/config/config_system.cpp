
#include "config_system.h"

namespace C3D
{
#define CHECK_PROPERTY_NAME(name)                                 \
    if (!m_config.HasProperty(name))                              \
    {                                                             \
        ERROR_LOG("Config property: '{}' does not exist.", name); \
        return false;                                             \
    }

    bool ConfigSystem::OnInit(const CSONObject& config)
    {
        INFO_LOG("Initializing config system.");
        m_config = config;
        return true;
    }

    bool ConfigSystem::GetProperty(const String& name, u64& value)
    {
        CHECK_PROPERTY_NAME(name);
        return m_config.GetPropertyValueByName(name, value);
    }

    bool ConfigSystem::GetProperty(const String& name, String& value)
    {
        CHECK_PROPERTY_NAME(name);
        return m_config.GetPropertyValueByName(name, value);
    }

    bool ConfigSystem::GetProperty(const String& name, DynamicArray<String>& value)
    {
        CHECK_PROPERTY_NAME(name);

        // First we get the CSONArray from the property
        CSONArray array;
        m_config.GetPropertyValueByName(name, array);
        // Then we can resize the output array to the size of the CSONArray
        value.Reserve(array.properties.Size());
        // Then we loop over the properties and populate them
        for (auto& prop : array.properties)
        {
            value.PushBack(prop.GetString());
        }
        return true;
    }

    void ConfigSystem::OnShutdown()
    {
        INFO_LOG("Shutting down config system.");
        m_config.properties.Destroy();
    }

}  // namespace C3D