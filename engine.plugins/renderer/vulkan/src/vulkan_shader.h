#pragma once
#include <string/string.h>
#include <volk.h>

#include "vulkan_swapchain.h"

namespace C3D
{
    struct VulkanContext;
    struct VulkanShaderModule;

    struct DescriptorInfo
    {
        DescriptorInfo(VkImageView imageView, VkImageLayout imageLayout)
        {
            image.sampler     = VK_NULL_HANDLE;
            image.imageView   = imageView;
            image.imageLayout = imageLayout;
        }

        DescriptorInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
        {
            image.sampler     = sampler;
            image.imageView   = imageView;
            image.imageLayout = imageLayout;
        }

        DescriptorInfo(VkBuffer buffer_, VkDeviceSize offset, VkDeviceSize range)
        {
            buffer.buffer = buffer_;
            buffer.offset = offset;
            buffer.range  = range;
        }

        DescriptorInfo(VkBuffer buffer_)
        {
            buffer.buffer = buffer_;
            buffer.offset = 0;
            buffer.range  = VK_WHOLE_SIZE;
        }

        union {
            VkDescriptorImageInfo image;
            VkDescriptorBufferInfo buffer;
        };
    };

    /** @brief A structure used as input for creating a shader. */
    struct VulkanShaderCreateInfo
    {
        /** @brief The name of the Shader. Used for logging and debugging purposes. */
        const char* name = nullptr;
        /** @brief The bindpoint of the Shader. This defaults to GRAPHICS. */
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        /** @brief An initializer list containing all ShaderModules that should be used by this Shader. */
        std::initializer_list<VulkanShaderModule*> modules;
        /** @brief A pointer to the Vulkan context. */
        VulkanContext* context = nullptr;
        /** @brief A pointer to the swapchain. */
        VulkanSwapchain* swapchain = nullptr;
        /** @brief The cache for the pipelines. */
        VkPipelineCache cache = VK_NULL_HANDLE;
        /** @brief The size of the push constants block that will be used by this Shader. */
        u64 pushConstantsSize = 0;
    };

    class VulkanShader
    {
    public:
        bool Create(const VulkanShaderCreateInfo& createInfo);

        void Bind(VkCommandBuffer commandBuffer) const;
        void Dispatch(VkCommandBuffer commandBuffer, u32 countX, u32 countY, u32 countZ) const;

        void PushDescriptorSet(VkCommandBuffer commandBuffer, DescriptorInfo* descriptors) const;
        void PushConstants(VkCommandBuffer commandBuffer, const void* data, u64 size) const;

        void Destroy();

    private:
        u32 GatherResources(VkDescriptorType (&resourceTypes)[32]);

        bool CreateSetLayout();
        bool CreatePipelineLayout(u64 pushConstantsSize);
        bool CreateGraphicsPipeline(VkPipelineCache pipelineCache, VulkanSwapchain& swapchain);
        bool CreateComputePipeline(VkPipelineCache pipelineCache);

        bool CreateDescriptorUpdateTemplate();

        /** @brief The user-provided name of this Shader. Used for debugging purposes. */
        String m_name;
        /** @brief The bind point used by this Shader. */
        VkPipelineBindPoint m_bindPoint;
        /** @brief The local size x of the shader module. Used in Dispatch calls to calculate number of dispatches. */
        u32 m_localSizeX = 0, m_localSizeY = 0, m_localSizeZ = 0;
        /** @brief An array of VulkanShaderModules used by this Shader. */
        DynamicArray<const VulkanShaderModule*> m_shaderModules;
        /** @brief A handle to the set layout used by this Shader. */
        VkDescriptorSetLayout m_setLayout = nullptr;
        /** @brief A handle to the layout used by this Shader. */
        VkPipelineLayout m_layout = nullptr;
        /** @brief A handle to the pipeline used by this Shader. */
        VkPipeline m_pipeline = nullptr;
        /** @brief A field that contains all Shader stages that use Push Constants. */
        VkShaderStageFlags m_pushConstantStages = 0;
        /** @brief A handle to the Descriptor Update Template used by this Shader. */
        VkDescriptorUpdateTemplate m_updateTemplate = nullptr;
        /** @brief A pointer the our Vulkan context. */
        VulkanContext* m_context = nullptr;
    };
}  // namespace C3D