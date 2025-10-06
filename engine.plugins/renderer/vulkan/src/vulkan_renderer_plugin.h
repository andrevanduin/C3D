
#pragma once
#include <renderer/mesh.h>
#include <renderer/renderer_plugin.h>

#include "vulkan_buffer.h"
#include "vulkan_context.h"
#include "vulkan_shader.h"
#include "vulkan_shader_module.h"

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

        bool UploadGeometry(const Window& window, const Geometry& geometry) override;

        bool GenerateDrawCommands(const Window& window, const Geometry& geometry) override;

        void SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth, f32 maxDepth) override;
        void SetScissor(i32 offsetX, i32 offsetY, u32 width, u32 height) override;

        bool SupportsFeature(RendererSupportFlag feature) const override;

    private:
        void BeginRendering(VkCommandBuffer commandBuffer, VkImageView colorView, VkImageView depthView, const VkClearColorValue& clearColor,
                            const VkClearDepthStencilValue& clearDepthStencil, u32 width, u32 height);

        void BeginRenderingLate(VkCommandBuffer commandBuffer, VkImageView colorView, VkImageView depthView, const VkClearColorValue& clearColor,
                                const VkClearDepthStencilValue& clearDepthStencil, u32 width, u32 height);

        bool m_meshShadingEnabled      = true;
        bool m_cullingEnabled          = true;
        bool m_occlusionCullingEnabled = true;
        bool m_lodEnabled              = true;
        bool m_debugPyramid            = false;
        u32 m_debugPyramidLevel        = 0;

        VulkanShaderModule m_drawCullShaderModule;
        VulkanShaderModule m_drawCullLateShaderModule;
        VulkanShaderModule m_depthReduceShaderModule;
        VulkanShaderModule m_meshShaderModule;
        VulkanShaderModule m_meshletShaderModule;
        VulkanShaderModule m_fragmentShaderModule;
        VulkanShaderModule m_meshletTaskShaderModule;

        VulkanShader m_meshShader;
        VulkanShader m_meshletShader;
        VulkanShader m_drawCullShader;
        VulkanShader m_drawCullLateShader;
        VulkanShader m_depthReduceShader;

        VkSampler m_depthSampler;

        DynamicArray<MeshDraw> m_draws;

        VkQueryPool m_queryPoolTimestamps;
        VkQueryPool m_queryPoolStatistics;

        f64 m_frameCpuAvg = 0, m_frameGpuAvg = 0, m_frameCpuBegin = 0;

        u32 m_drawCount    = 0;
        f32 m_sceneRadius  = 300;
        f32 m_drawDistance = 200;

        VulkanBuffer m_vertexBuffer;
        VulkanBuffer m_indexBuffer;
        VulkanBuffer m_meshBuffer;
        VulkanBuffer m_meshletBuffer;
        VulkanBuffer m_meshletDataBuffer;
        VulkanBuffer m_drawBuffer;
        VulkanBuffer m_drawCommandBuffer;
        VulkanBuffer m_drawCommandCountBuffer;
        VulkanBuffer m_drawVisibilityBuffer;

        VkViewport m_viewport;
        VkRect2D m_scissor;

        VulkanContext m_context;
    };
}  // namespace C3D
