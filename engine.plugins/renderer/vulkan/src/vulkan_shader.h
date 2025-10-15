#pragma once
#include <events/types.h>
#include <platform/platform_types.h>
#include <string/string.h>
#include <volk.h>

#include "vulkan_buffer.h"
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

        DescriptorInfo(VulkanBuffer buffer_)
        {
            buffer.buffer = buffer_.GetHandle();
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
        /** @brief An initialize list containing all constants we want to pass to the Shader. */
        std::initializer_list<i32> constants;
        /** @brief The cache for the pipelines. */
        VkPipelineCache cache = VK_NULL_HANDLE;
        /** @brief The size of the push constants block that will be used by this Shader. */
        u64 pushConstantsSize = 0;
        /** @brief A pointer to the Vulkan context. */
        VulkanContext* context = nullptr;
    };

    class VulkanShader
    {
    public:
        bool Create(const VulkanShaderCreateInfo& createInfo);

        void Bind(VkCommandBuffer commandBuffer) const;
        void Dispatch(VkCommandBuffer commandBuffer, u32 countX, u32 countY, u32 countZ) const;
        void DispatchIndirect(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer, VkDeviceSize offset) const;

        void PushDescriptorSet(VkCommandBuffer commandBuffer, DescriptorInfo* descriptors) const;
        void PushConstants(VkCommandBuffer commandBuffer, const void* data, u64 size) const;

        void Destroy();

    private:
        bool Recreate();

        void DestroyInternal(VkDescriptorSetLayout setLayout, VkPipelineLayout pipelineLayout = nullptr, VkPipeline pipeline = nullptr,
                             VkDescriptorUpdateTemplate updateTemplate = nullptr);

        u32 GatherResources(VkDescriptorType (&resourceTypes)[32]);

        VkDescriptorSetLayout CreateSetLayout();
        VkPipelineLayout CreatePipelineLayout(VkDescriptorSetLayout setLayout);
        VkPipeline CreateGraphicsPipeline(VkPipelineLayout layout);
        VkPipeline CreateComputePipeline(VkPipelineLayout layout);

        VkDescriptorUpdateTemplate CreateDescriptorUpdateTemplate(VkPipelineLayout layout);

        /** @brief The user-provided name of this Shader. Used for debugging purposes. */
        String m_name;
        /** @brief The bind point used by this Shader. */
        VkPipelineBindPoint m_bindPoint;
        /** @brief The local size x of the shader module. Used in Dispatch calls to calculate number of dispatches. */
        u32 m_localSizeX = 0, m_localSizeY = 0, m_localSizeZ = 0;
        /** @brief The size of the push constants used by this Shader in bytes. */
        u32 m_pushConstantsSize = 0;
        /** @brief The constant values used by this Shader. */
        std::initializer_list<i32> m_constants;
        /** @brief An array of VulkanShaderModules used by this Shader. */
        DynamicArray<VulkanShaderModule*> m_shaderModules;
        /** @brief An array of FileWatchIds for the VulkanShaderModules used by this Shader. */
        DynamicArray<FileWatchId> m_shaderModuleFileIds;
        /** @brief The registerd event callback for changes on watched files. */
        RegisteredEventCallback m_watchedFilesCallback;
        /** @brief A handle to the set layout used by this Shader. */
        VkDescriptorSetLayout m_setLayout = nullptr;
        /** @brief A handle to the layout used by this Shader. */
        VkPipelineLayout m_layout = nullptr;
        /** @brief A handle to the pipeline used by this Shader. */
        VkPipeline m_pipeline = nullptr;
        /** @brief A handle to the pipeline cache used by this Shader. */
        VkPipelineCache m_pipelineCache = nullptr;
        /** @brief A field that contains all Shader stages that use Push Constants. */
        VkShaderStageFlags m_pushConstantStages = 0;
        /** @brief A handle to the Descriptor Update Template used by this Shader. */
        VkDescriptorUpdateTemplate m_updateTemplate = nullptr;
        /** @brief A pointer the our Vulkan context. */
        VulkanContext* m_context = nullptr;
    };
}  // namespace C3D