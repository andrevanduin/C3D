
#pragma once
#include <colors.h>
#include <string/string.h>

#include "vulkan_types.h"

#define VK_CHECK(expr) { C3D_ASSERT((expr) == VK_SUCCESS) }

namespace C3D
{
    struct VulkanContext;

    namespace VkUtils
    {

        /** @brief A method to check if the passed VkResult is one of the results that are considered a SUCCESS. */
        bool IsSuccess(VkResult result);

        /** @brief This method converts a VkResult into a human-readable result string. */
        const char* ResultString(VkResult result, bool getExtended = true);

        /** @brief Calculate the available GPU memory in MebiBytes. */
        u32 GetAvailableGPUMemoryInMB(VkPhysicalDeviceMemoryProperties properties);

        /**
         * @brief Creates a Vulkan Image Barrier.
         *
         * @param image The image for which the barrier should be created
         * @param srcStageMask The source stage mask
         * @param srcAccessMask The source access mask
         * @param oldLayout The old layout of the image
         * @param dstStageMask The destination stage mask
         * @param dstAccessMask The destination access mask
         * @param newLayout The new layout of the image
         * @param aspectMask The aspect mask (= VK_IMAGE_ASPECT_COLOR_BIT by default)
         * @param baseMipLevel The base mip level to start at (= 0 by default)
         * @param levelCount The number of mip levels to include in the barrier (= VK_REMAINING_MIP_LEVELS by default)
         * @return A VkImageMemoryBarrier2
         */
        VkImageMemoryBarrier2 ImageBarrier(VkImage image, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkImageLayout oldLayout,
                                           VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask, VkImageLayout newLayout,
                                           VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, u32 baseMipLevel = 0,
                                           u32 levelCount = VK_REMAINING_MIP_LEVELS);

        /**
         * @brief Creates a Vulkan Buffer Barrier.
         *
         * @param buffer The buffer for which a barrier should be created
         * @param srcStageMask The src stage mask
         * @param srcAccessMask The source access mask
         * @param dstStageMask The destination stage mask
         * @param dstAccessMask The destination access mask
         * @return A VkBufferMemoryBarrier2
         */
        VkBufferMemoryBarrier2 BufferBarrier(VkBuffer buffer, VkPipelineStageFlags2 srcStageMask, VkAccessFlags srcAccessMask,
                                             VkPipelineStageFlags2 dstStageMask, VkAccessFlags dstAccessMask);

        /**
         * @brief Sets up a Vulkan Pipeline Barrier.
         *
         * @param commandBuffer The command buffer to use for the pipeline barrier
         * @param dependencyFlags The dependency flags for the barrier
         * @param bufferBarrierCount The number of buffer barriers
         * @param pBufferBarriers A pointer to the buffer barriers
         * @param imageBarrierCount The number of image barriers
         * @param pImageBarriers A pointer to the image barriers
         */
        void PipelineBarrier(VkCommandBuffer commandBuffer, VkDependencyFlags dependencyFlags, u32 bufferBarrierCount,
                             const VkBufferMemoryBarrier2* pBufferBarriers, u32 imageBarrierCount, const VkImageMemoryBarrier2* pImageBarriers);

        /**
         * @brief Creates a Vulkan Command Pool.
         *
         * @param context A pointer to the Vulkan context
         * @param name The name of the commandpool (used for debugging purposes)
         * @return A VkCommandPool if successful; nullptr otherwise
         */
        VkCommandPool CreateCommandPool(VulkanContext* context, const String& name);

        /**
         * @brief Allocate a Vulkan Command Buffer from a Command Pool.
         *
         * @param context  A pointer to the Vulkan context
         * @param name The name of the command buffer (used for debugging purposes)
         * @param commandPool A command pool to allocate from
         * @param level The command buffer level (primary, or secondary)
         * @return A VkCommandBuffer if successful; nullptr otherwise
         */
        VkCommandBuffer AllocateCommandBuffer(VulkanContext* context, const String& name, VkCommandPool commandPool, VkCommandBufferLevel level);
        /**
         * @brief Creates a Vulkan Query Pool.
         *
         * @param context  A pointer to the vulkan context
         * @param queryCount The number of queries managed by the pool
         * @param queryType The type of queries the pool will manage
         * @return A VkQueryPool if successful; nullptr otherwise
         */
        VkQueryPool CreateQueryPool(VulkanContext* context, u32 queryCount, VkQueryType queryType);

        /**
         * @brief Creates a Vulkan image.
         *
         * @param context A pointer to the vulkan context
         * @param width The width of the image
         * @param height The height of the image
         * @param format The format of the image
         * @param mipLevels The number of mips in the image
         * @param usage The usage flags for this image
         * @return A VkImage if successful; nullptr otherwise
         */
        VkImage CreateImage(VulkanContext* context, const String& name, u32 width, u32 height, VkFormat format, u32 mipLevels, VkImageUsageFlags usage);

        /**
         * @brief Creates a Vulkan image View.
         *
         * @param context A pointer to the vulkan context
         * @param image A handle to the vulkan image
         * @param format The format of the vulkan image
         * @param aspectMask An aspect mask for the view
         * @param mipLevel The mip level for thie view
         * @param levelCount The number of levels in this view
         * @return A VkImageView if successful; nullptr otherwise
         */
        VkImageView CreateImageView(VulkanContext* context, const String& name, VkImage image, VkFormat format, VkImageAspectFlags aspectMask, u32 mipLevel,
                                    u32 levelCount);

        /**
         * @brief Calculates the number of mip levels required for an image of given width and height.
         *
         * @param width The width of the image
         * @param height The height of the image
         * @return The number of mips required
         */
        u32 CalculateImageMiplevels(u32 width, u32 height);

        /**
         * @brief Create a Vulkan Sampler.
         *
         * @param context  A pointer to the Vulkan context
         * @param name The name of the sampler (used for debugging purposes)
         * @param reductionMode The reduction mode used by the sampler
         * @return A VkSampler if successful; nullptr otherwise
         */
        VkSampler CreateSampler(VulkanContext* context, const String& name, VkSamplerReductionMode reductionMode);

        /**
         * @brief Create a Vulkan Semaphore.
         *
         * @param context A pointer to the Vulkan context
         * @param name The name of the semaphore (used for debugging purposes)
         * @return VkSemaphore if successful; nullptr otherwise
         */
        VkSemaphore CreateSemaphore(VulkanContext* context, const String& name);

        /**
         * @brief Creates a Vulkan fence.
         *
         * @param context A pointer to the Vulkan context
         * @param name The name of the fence (used for debugging purposes)
         * @return A VkFence if successful; nullptr otherwise
         */
        VkFence CreateFence(VulkanContext* context, const String& name);

        /** @brief Helper method to enable easier loading of vulkan extension functions. */
        template <typename T>
        T LoadExtensionFunction(VkInstance instance, const char* name)
        {
            static_assert(std::is_pointer_v<T>, "LoadVulkanExtensionFunction only accepts a template argument of type pointer.");

            T func = reinterpret_cast<T>(vkGetInstanceProcAddr(instance, name));
            if (!func)
            {
                Logger::Fatal("LoadVulkanExtensionFunction() - Failed to obtain extension function: '{}'.", name);
                return nullptr;
            }

            return func;
        }

#if defined(_DEBUG)
        const char* VkMessageTypeToString(VkDebugUtilsMessageTypeFlagsEXT s);

        VkBool32 VkDebugLog(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

        void SetDebugObjectName(const VulkanContext* context, VkObjectType type, void* handle, const String& name);
        void SetDebugObjectTag(const VulkanContext* context, VkObjectType type, void* handle, u64 tagSize, const void* tagData);
        void BeginCmdDebugLabel(const VulkanContext* context, VkCommandBuffer buffer, const String& label, const RGBA& color);
        void EndCmdDebugLabel(const VulkanContext* context, VkCommandBuffer buffer);

#define VK_SET_DEBUG_OBJECT_NAME(context, type, handle, name) VkUtils::SetDebugObjectName(context, type, handle, name)

#define VK_SET_DEBUG_OBJECT_TAG(context, type, handle, tagSize, tagData) VkUtils::SetDebugObjectTag(context, type, handle, tagSize, tagData)

#define VK_BEGIN_CMD_DEBUG_LABEL(context, buffer, label, color) VkUtils::BeginCmdDebugLabel(context, buffer, label, color)

#define VK_END_CMD_DEBUG_LABEL(context, buffer) VkUtils::EndCmdDebugLabel(context, buffer)

#else
/* Both macros simply do nothing in non-debug builds */
#define VK_SET_DEBUG_OBJECT_NAME(context, type, handle, name)
#define VK_SET_DEBUG_OBJECT_TAG(context, type, handle, tagSize, tagData)
#define VK_BEGIN_CMD_DEBUG_LABEL(context, buffer, label, color)
#define VK_END_CMD_DEBUG_LABEL(context, buffer)
#endif
    }  // namespace VkUtils
}  // namespace C3D