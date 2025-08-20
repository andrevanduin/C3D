#pragma once
#include <string/string.h>
#include <volk.h>

#include "vulkan_swapchain.h"

namespace C3D
{
    struct VulkanContext;

    struct DescriptorInfo
    {
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

    class VulkanShader
    {
    public:
        bool Create(VulkanContext* context, VkPipelineCache pipelineCache, VulkanSwapchain& swapchain, const char* name, const char* vertexShaderName,
                    const char* fragmentShaderName, bool meshShadingEnabled);

        void Bind(VkCommandBuffer commandBuffer) const;
        void PushDescriptorSet(VkCommandBuffer commandBuffer, DescriptorInfo* descriptors) const;

        void Destroy();

    private:
        bool LoadShaderModule(const char* name, VkShaderModule& module);

        bool CreateSetLayout();
        bool CreatePipelineLayout();
        bool CreateGraphicsPipeline(VkPipelineCache pipelineCache, VulkanSwapchain& swapchain);
        bool CreateDescriptorUpdateTemplate();

        String m_name;

        VkPipelineBindPoint m_bindPoint;

        VkShaderModule m_vertexShaderModule   = nullptr;
        VkShaderModule m_fragmentShaderModule = nullptr;

        VkDescriptorSetLayout m_setLayout = nullptr;
        VkPipelineLayout m_layout         = nullptr;
        VkPipeline m_pipeline             = nullptr;

        VkDescriptorUpdateTemplate m_updateTemplate = nullptr;

        bool m_meshShadingEnabled = false;

        VulkanContext* m_context = nullptr;
    };
}  // namespace C3D