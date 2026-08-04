
#pragma once
#include <renderer/mesh.h>
#include <renderer/renderer_plugin.h>

#include "containers/dynamic_array.h"
#include "containers/hash_map.h"
#include "string/string.h"
#include "vulkan_buffer.h"
#include "vulkan_context.h"
#include "vulkan_shader.h"
#include "vulkan_shader_module.h"
#include "vulkan_texture.h"

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

        bool OnRun(const Geometry& geometry) override;

        bool Begin(Window& window) override;
        bool End(Window& window) override;

        bool Submit(Window& window) override;
        bool Present(Window& window) override;

        bool OnCreateWindow(Window& window) override;
        bool OnResizeWindow(Window& window) override;
        void OnDestroyWindow(Window& window) override;

        bool UploadGeometry(const Geometry& geometry) override;
        bool UploadTexture(const TextureAsset& texture) override;

        bool GenerateDrawCommands(const Geometry& geometry) override;
        bool UploadDrawCommands(const Geometry& geometry, const DynamicArray<MeshDraw>& draws) override;

        void SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth, f32 maxDepth) override;
        void SetScissor(i32 offsetX, i32 offsetY, u32 width, u32 height) override;
        void SetCamera(const Camera& camera) override;
        void SetSunDirection(const vec3& sunDirection) override;

        bool SupportsFeature(RendererSupportFlag feature) const override;

        u8* GetStagingBuffer() const override { return static_cast<u8*>(m_context.stagingBuffer.GetData()); }
        u32 GetStagingBufferSize() const override { return m_context.stagingBuffer.GetSize(); }

    private:
        void BeginRendering(VkCommandBuffer commandBuffer, VkImageView colorView, VkImageView depthView, const VkClearColorValue& clearColor,
                            const VkClearDepthStencilValue& clearDepthStencil, u32 width, u32 height, bool late) const;

        void CullStep(VkCommandBuffer commandBuffer, const VulkanShader& shader, VulkanTexture& depthPyramid, const CullData& cullData, u32 timestamp, bool taskSubmit, bool late,
                      u32 postPass = 0) const;
        void RenderStep(VkCommandBuffer commandBuffer, const VulkanTexture& colorTarget, const VulkanTexture& depthTarget, const VulkanTexture& depthPyramid,
                        const Globals& globals, const Window& window, u32 query, u32 timeStamp, bool taskSubmit, bool clusterSubmit, bool late, u32 postPass = 0) const;
        void DepthPyramidStep(VkCommandBuffer commandBuffer, VulkanTexture& depthTarget, VulkanTexture& depthPyramid) const;

        /** @brief A boolean indicating if we are using mesh shading. */
        bool m_meshShadingEnabled = true;
        /** @brief A boolean indicating if we are using ray tracing. */
        bool m_rayTracingEnabled = true;
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
        /** @brief A boolean indicating if shadows are enabled (with Ray Tracing). */
        bool m_shadowsEnabled = true;
        /** @brief A boolean indicating if we are rendering debug lods. */
        bool m_debugLods = false;
        /** @brief The lod level we are displaying as part of our lod debugging. */
        u32 m_debugLodStep = 0;

        HashMap<String, VulkanShaderModule> m_shaderModules;

        VulkanShader m_depthReduceShader;

        VulkanShader m_meshShader;
        VulkanShader m_meshPostShader;

        VulkanShader m_taskCullShader;
        VulkanShader m_taskCullLateShader;
        VulkanShader m_taskSubmitShader;
        VulkanShader m_taskMeshletShader;
        VulkanShader m_taskMeshletLateShader;
        VulkanShader m_taskMeshletPostShader;

        VulkanShader m_drawCullShader;
        VulkanShader m_drawCullLateShader;

        VulkanShader m_clusterCullShader;
        VulkanShader m_clusterCullLateShader;
        VulkanShader m_clusterSubmitShader;
        VulkanShader m_clusterMeshletShader;
        VulkanShader m_clusterPostMeshletShader;

        VulkanShader m_blitShader;

        VkSampler m_textureSampler;
        VkSampler m_readSampler;
        VkSampler m_depthSampler;

        DynamicArray<MeshDraw> m_draws;
        DynamicArray<VulkanTexture> m_textures;

        DynamicArray<VkAccelerationStructureKHR> m_blas;
        VkAccelerationStructureKHR m_tlas;

        VkDescriptorPool m_textureDescriptorPool;
        VkDescriptorSetLayout m_textureDescriptorSetLayout;
        VkDescriptorSet m_textureDescriptorSet;

        Camera m_camera;
        vec3 m_sunDirection = vec3(1.0f);

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

        VulkanBuffer m_blasBuffer;
        VulkanBuffer m_tlasBuffer;

        VkViewport m_viewport;
        VkRect2D m_scissor;

        VulkanContext m_context;
    };
}  // namespace C3D
