
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

        bool CreateResources() override;

        bool Begin(Window& window) override;
        bool End(Window& window) override;

        bool Submit(Window& window) override;
        bool Present(Window& window) override;

        bool OnCreateWindow(Window& window) override;
        bool OnResizeWindow(Window& window) override;
        void OnDestroyWindow(Window& window) override;

        bool UploadGeometry(const Window& window, const Geometry& geometry) override;

        bool GenerateDrawCommands(const Window& window, const Geometry& geometry) override;
        bool UploadDrawCommands(const Window& window, const Geometry& geometry, const DynamicArray<MeshDraw>& draws) override;

        void SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth, f32 maxDepth) override;
        void SetScissor(i32 offsetX, i32 offsetY, u32 width, u32 height) override;
        void SetCamera(const Camera& camera) override;

        bool SupportsFeature(RendererSupportFlag feature) const override;

    private:
        void BeginRendering(VkCommandBuffer commandBuffer, VkImageView colorView, VkImageView depthView, const VkClearColorValue& clearColor,
                            const VkClearDepthStencilValue& clearDepthStencil, u32 width, u32 height, bool late) const;

        void CullStep(VkCommandBuffer commandBuffer, const VulkanShader& shader, VulkanTexture& depthPyramid, const CullData& cullData, u32 timestamp,
                      bool taskSubmit, bool late) const;
        void RenderStep(VkCommandBuffer commandBuffer, const VulkanTexture& colorTarget, const VulkanTexture& depthTarget, const VulkanTexture& depthPyramid,
                        const Globals& globals, const Window& window, u32 query, u32 timeStamp, bool taskSubmit, bool clusterSubmit, bool late) const;
        void DepthPyramidStep(VkCommandBuffer commandBuffer, VulkanTexture& depthTarget, VulkanTexture& depthPyramid) const;

        /** @brief A boolean indicating if we are using mesh shading. */
        bool m_meshShadingEnabled = true;
        /** @brief A boolean indicating if we are using task shaders during mesh shading.
         * This works well on Nvidia but gives bad performance on AMD */
        bool m_taskShadingEnabled = false;
        /** @brief A boolean indicating if we are doing any culling. */
        bool m_cullingEnabled = true;
        /** @brief A boolean indicating if we are doing occlusion culling. */
        bool m_occlusionCullingEnabled = true;
        /** @brief A boolean indicating ifwe are doing occlusion culling on a cluster (meshlet) level. */
        bool m_clusterOcclusionCullingEnabled = true;
        /** @brief A boolean indicating if we are doing LODs for meshes. */
        bool m_lodEnabled = true;
        /** @brief A boolean indicating if we are rendering our depth pyramid for debugging. */
        bool m_debugPyramid = false;
        /** @brief The (mip) level we are displaying as part of our depth pyramid debugging. */
        u32 m_debugPyramidLevel = 0;
        /** @brief A boolean indicating if we are rendering debug lods. */
        bool m_debugLods = false;
        /** @brief The lod level we are displaying as part of our lod debugging. */
        u32 m_debugLodStep = 0;

        VulkanShaderModule m_cullShaderModule;
        VulkanShaderModule m_clusterCullShaderModule;

        VulkanShaderModule m_taskSubmitShaderModule;
        VulkanShaderModule m_clusterSubmitShaderModule;

        VulkanShaderModule m_depthReduceShaderModule;
        VulkanShaderModule m_meshShaderModule;
        VulkanShaderModule m_fragmentShaderModule;
        VulkanShaderModule m_meshletShaderModule;
        VulkanShaderModule m_meshletTaskShaderModule;

        VulkanShader m_depthReduceShader;

        VulkanShader m_meshShader;
        VulkanShader m_meshletShader;
        VulkanShader m_meshletLateShader;
        VulkanShader m_clusterMeshletShader;

        VulkanShader m_drawCullShader;
        VulkanShader m_drawCullLateShader;

        VulkanShader m_taskCullShader;
        VulkanShader m_taskCullLateShader;

        VulkanShader m_clusterCullShader;
        VulkanShader m_clusterCullLateShader;

        VulkanShader m_taskSubmitShader;
        VulkanShader m_clusterSubmitShader;

        VkSampler m_depthSampler;

        DynamicArray<MeshDraw> m_draws;

        Camera m_camera;

        VkQueryPool m_queryPoolTimestamps;
        VkQueryPool m_queryPoolStatistics;

        f64 m_frameCpuAvg = 0, m_frameGpuAvg = 0, m_frameCpuBegin = 0;

        u32 m_meshletVisibilityBytes = 0;

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
        VulkanBuffer m_meshletVisibilityBuffer;
        VulkanBuffer m_clusterIndexBuffer;
        VulkanBuffer m_clusterCountBuffer;

        VkViewport m_viewport;
        VkRect2D m_scissor;

        VulkanContext m_context;
    };
}  // namespace C3D
