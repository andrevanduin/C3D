
#include "render_system.h"

#include "cson/cson_types.h"
#include "renderer_plugin.h"

namespace C3D
{
    bool RenderSystem::OnInit(const CSONObject& config)
    {
        // Parse the user provided config
        for (const auto& prop : config.properties)
        {
            if (prop.name.IEquals("backend"))
            {
                m_config.rendererPlugin = prop.GetString();
            }
            else if (prop.name.IEquals("vsync"))
            {
                if (prop.GetBool()) m_config.flags |= FlagVSync;
            }
            else if (prop.name.IEquals("powersaving"))
            {
                if (prop.GetBool()) m_config.flags |= FlagPowerSaving;
            }
            else if (prop.name.IEquals("validationlayers"))
            {
                if (prop.GetBool()) m_config.flags |= FlagValidationLayers;
            }
            else if (prop.name.IEquals("PCF"))
            {
                if (prop.GetBool()) m_config.flags |= FlagPCF;
            }
        }

        // Load the backend plugin
        m_backendDynamicLibrary.Load(m_config.rendererPlugin);

        m_backendPlugin = m_backendDynamicLibrary.CreatePlugin<RendererPlugin>();
        if (!m_backendPlugin)
        {
            FATAL_LOG("Failed to create valid renderer plugin.");
            return false;
        }

        RendererPluginConfig rendererPluginConfig{};
        rendererPluginConfig.flags = m_config.flags;

        if (!m_backendPlugin->OnInit(rendererPluginConfig))
        {
            FATAL_LOG("Failed to Initialize Renderer Backend.");
            return false;
        }

        INFO_LOG("Initialized successfully.");
        return true;
    }

    void RenderSystem::OnShutdown()
    {
        INFO_LOG("Shutting down.");

        // Shutdown our plugin
        m_backendPlugin->OnShutdown();

        // Delete the plugin
        m_backendDynamicLibrary.DeletePlugin(m_backendPlugin);

        // Unload the library
        if (!m_backendDynamicLibrary.Unload())
        {
            ERROR_LOG("Failed to unload backend plugin dynamic library.");
        }
    }

    bool RenderSystem::Begin(Window& window) const { return m_backendPlugin->Begin(window); }

    bool RenderSystem::End(Window& window) const { return m_backendPlugin->End(window); }

    bool RenderSystem::Submit(Window& window) const { return m_backendPlugin->Submit(window); }

    bool RenderSystem::Present(Window& window) const { return m_backendPlugin->Present(window); }

    bool RenderSystem::OnCreateWindow(Window& window) const
    {
        // Create the renderer state for this window
        window.rendererState = Memory.New<WindowRendererState>(MemoryType::RenderSystem);

        if (!m_backendPlugin->OnCreateWindow(window))
        {
            ERROR_LOG("The Renderer backend failed to create resources for window: '{}'.", window.name);
            return false;
        }
        return true;
    }

    bool RenderSystem::OnResizeWindow(Window& window) const { return m_backendPlugin->OnResizeWindow(window); }

    void RenderSystem::OnDestroyWindow(Window& window) const
    {
        m_backendPlugin->OnDestroyWindow(window);

        if (window.rendererState)
        {
            Memory.Delete(window.rendererState);
            window.rendererState = nullptr;
        }
    }

}  // namespace C3D