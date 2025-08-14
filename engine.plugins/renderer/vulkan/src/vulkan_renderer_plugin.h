
#pragma once
#include <renderer/renderer_plugin.h>

#include "vulkan_context.h"

namespace C3D
{
    class Viewport;

    extern "C" {
    C3D_API RendererPlugin* CreatePlugin();
    C3D_API void DeletePlugin(RendererPlugin* plugin);
    }

    class VulkanRendererPlugin final : public RendererPlugin
    {
    public:
        VulkanRendererPlugin() = default;

        bool OnInit(const RendererPluginConfig& config) override;
        void OnShutdown() override;

        bool Begin(Window& window) override;
        bool End(Window& window) override;

        bool Submit(Window& window) override;
        bool Present(Window& window) override;

        bool OnCreateWindow(Window& window) override;
        bool OnResizeWindow(Window& window) override;
        void OnDestroyWindow(Window& window) override;

    private:
        VkShaderModule m_vertexShader;
        VkShaderModule m_fragmentShader;

        VkPipelineLayout m_triangleLayout;
        VkPipeline m_trianglePipeline;

        VulkanContext m_context;
    };
}  // namespace C3D
