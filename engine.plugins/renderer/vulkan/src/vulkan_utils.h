
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
         * @brief Creates an image barrier structure.
         *
         * @param image A handle to the Vulkan image
         * @param srcAccessMask The
         * @param dstAccessMask
         * @param oldLayout
         * @param newLayout
         * @param aspectMask
         * @return VkImageMemoryBarrier
         */
        VkImageMemoryBarrier CreateImageBarrier(VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout,
                                                VkImageLayout newLayout, VkImageAspectFlags aspectMask);

        VkBufferMemoryBarrier CreateBufferBarrier(VkBuffer buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);

        /**
         * @brief Creates a Vulkan query pool
         *
         * @param context The vulkan context
         * @param queryCount The number of queries managed by the pool
         * @param queryType The type of queries the pool will manage
         * @return a VkQueryPool if successful; nullptr otherwise
         */
        VkQueryPool CreateQueryPool(VulkanContext& context, u32 queryCount, VkQueryType queryType);

        /**
         * @brief Creates a Vulkan image
         *
         * @param context A pointer to the vulkan context
         * @param width The width of the image
         * @param height The height of the image
         * @param format The format of the image
         * @param mipLevels The number of mips in the image
         * @param usage The usage flags for this image
         * @return A VkImage if successful; nullptr otherwise
         */
        VkImage CreateImage(VulkanContext* context, u32 width, u32 height, VkFormat format, u32 mipLevels, VkImageUsageFlags usage);

        /**
         * @brief Creates a Vulkan image View
         *
         * @param context A pointer to the vulkan context
         * @param image A handle to the vulkan image
         * @param format The format of the vulkan image
         * @param aspectMask An aspect mask for the view
         * @param mipLevel The mip level for thie view
         * @param levelCount The number of levels in this view
         * @return a VkImageView if successful; nullptr otherwise
         */
        VkImageView CreateImageView(VulkanContext* context, VkImage image, VkFormat format, VkImageAspectFlags aspectMask, u32 mipLevel, u32 levelCount);

        /**
         * @brief Calculates the number of mip levels required for an image of given width and height.
         *
         * @param width The width of the image
         * @param height The height of the image
         * @return The number of mips required
         */
        u32 CalculateImageMiplevels(u32 width, u32 height);

        VkSampler CreateSampler(VulkanContext* context);

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