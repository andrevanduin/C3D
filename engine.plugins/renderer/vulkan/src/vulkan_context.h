
#pragma once
#include <defines.h>
#include <renderer/types.h>
#include <vulkan/vulkan.h>

#include "vulkan_device.h"
#include "vulkan_types.h"

namespace C3D
{
    struct VulkanContext
    {
        /** @brief A handle to the Vulkan instance.
         * The vulkan instance is our interface with the Vulkan library
         */
        VkInstance instance;
        /** @brief A pointer to the allocator to be used by Vulkan for all it's memory allocations. */
        VkAllocationCallbacks* allocator = nullptr;

        /** @brief The vulkan device (GPU). */
        VulkanDevice device;

        /** @brief The current Vulkan API version. */
        VulkanAPIVersion api;

        /** @brief The Renderer flags passed by the renderer frontend */
        RendererConfigFlags flags;

#if defined(_DEBUG)
        /** @brief Function pointer to set debug object names. */
        PFN_vkSetDebugUtilsObjectNameEXT pfnSetDebugUtilsObjectNameEXT;
        /** @brief Function pointer to set debug object tag data. */
        PFN_vkSetDebugUtilsObjectTagEXT pfnSetDebugUtilsObjectTagEXT;
        /** @brief Function pointer to set the start of a debug label for a cmd. */
        PFN_vkCmdBeginDebugUtilsLabelEXT pfnCmdBeginDebugUtilsLabelEXT;
        /** @brief Function pointer to set the end of a debug label for a cmd. */
        PFN_vkCmdEndDebugUtilsLabelEXT pfnCmdEndDebugUtilsLabelEXT;

        /** @brief A pointer to the DebugUtilsMessenger extension object used for debugging purposes.*/
        VkDebugUtilsMessengerEXT debugMessenger;
#endif
    };
}  // namespace C3D