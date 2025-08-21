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
        /** @brief The name of the shader. Used for logging and debugging purposes. */
        const char* name = nullptr;
        /** @brief An array of ShaderModules that should be used by this shader. */
        const VulkanShaderModule** modules = nullptr;
        /** @brief The number of ShaderModules in the provided array. */
        u32 numModules = 0;

        VulkanContext* context     = nullptr;
        VulkanSwapchain* swapchain = nullptr;

        VkPipelineCache cache = VK_NULL_HANDLE;
    };

    class VulkanShader
    {
    public:
        bool Create(const VulkanShaderCreateInfo& createInfo);

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

        VulkanShaderModule** m_shaderModules = nullptr;
        u32 m_numShaderModules               = 0;

        VkDescriptorSetLayout m_setLayout = nullptr;
        VkPipelineLayout m_layout         = nullptr;
        VkPipeline m_pipeline             = nullptr;

        VkDescriptorUpdateTemplate m_updateTemplate = nullptr;

        VulkanContext* m_context = nullptr;
    };
}  // namespace C3D