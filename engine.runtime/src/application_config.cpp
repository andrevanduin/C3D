
#include "application_config.h"

#include "cson/cson_reader.h"

namespace C3D
{
    static void ParseWindowConfig(const CSONObject& config, ApplicationConfig& outConfig)
    {
        auto windowConfig = WindowConfig();
        for (const auto& prop : config.properties)
        {
            if (prop.name.IEquals("name"))
            {
                windowConfig.name = prop.GetString();
            }
            else if (prop.name.IEquals("width"))
            {
                windowConfig.width = prop.GetI64();
            }
            else if (prop.name.IEquals("height"))
            {
                windowConfig.height = prop.GetI64();
            }
            else if (prop.name.IEquals("title"))
            {
                windowConfig.title = prop.GetString();
            }
            else if (prop.name.IEquals("position"))
            {
                const auto& pos = prop.GetString();
                if (pos.IEquals("center"))
                {
                    windowConfig.flags |= WindowFlag::WindowFlagCenter;
                }
            }
            else if (prop.name.IEquals("fullscreen"))
            {
                if (prop.GetBool())
                {
                    windowConfig.flags |= WindowFlag::WindowFlagFullScreen;
                }
            }
        }

        outConfig.windowConfigs.PushBack(windowConfig);
    }

    bool ParseArgs(int argc, char** argv, ApplicationConfig& outConfig)
    {
        // Check if we have an argument passed in for our application path
        if (argc == 1)
        {
            // We just got passed the name of the executable
            ERROR_LOG("Missing application config path argument.")
            return false;
        }

        // Make sure we only pass the application config path as an argument
        if (argc > 2)
        {
            ERROR_LOG("Too many arguments passed. Expected 2 but got: {}", argc);
            return false;
        }

        CSONObject config;
        CSONReader reader;
        if (!reader.ReadFromFile(argv[1], config))
        {
            return false;
        }

        // Initialize our SystemConfigs and Rendergraphs HashMaps
        outConfig.systemConfigs.Create();
        outConfig.rendergraphs.Create();

        for (const auto& property : config.properties)
        {
            if (property.name.IEquals("windows"))
            {
                const auto& windowConfigs = property.GetArray();
                for (const auto& config : windowConfigs.properties)
                {
                    ParseWindowConfig(config.GetObject(), outConfig);
                }
            }
            else if (property.name.IEquals("systemconfigs"))
            {
                const auto& systemConfigs = property.GetArray();
                for (const auto& systemConfig : systemConfigs.properties)
                {
                    // Get the object containing name and config properties
                    const auto& obj = systemConfig.GetObject();
                    // The first property should be the name
                    const auto& name = obj.properties[0].GetString();
                    // The second property should be the config object
                    const auto& config = obj.properties[1].GetObject();
                    // Store the config object in our HashMap
                    outConfig.systemConfigs.Set(name, config);
                }
            }
            else if (property.name.IEquals("rendergraphs"))
            {
                const auto& rendergraphs = property.GetArray();
                for (const auto& graphConfig : rendergraphs.properties)
                {
                    // Get the object containing name and config properties
                    const auto& obj = graphConfig.GetObject();
                    // The first property should be the name
                    const auto& name = obj.properties[0].GetString();
                    // Store the config object in our HashMap
                    outConfig.rendergraphs.Set(name, obj);
                }
            }
        }

        return true;
    }
}  // namespace C3D