
#include "vulkan_debugger.h"

#include <defines.h>
#include <logger/logger.h>
#include <volk.h>

#include "vulkan_context.h"
#include "vulkan_utils.h"

namespace C3D
{
    bool VulkanDebugger::Create(VulkanContext& context)
    {
        u32 logSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

        u32 messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        debugCreateInfo.messageSeverity                    = logSeverity;
        debugCreateInfo.messageType                        = messageType;
        debugCreateInfo.pfnUserCallback                    = VkUtils::VkDebugLog;

        VK_CHECK(vkCreateDebugUtilsMessengerEXT(context.instance, &debugCreateInfo, context.allocator, &context.debugMessenger));

        INFO_LOG("Debugger created Successfully.");

        return true;
    }

    void VulkanDebugger::Destroy(VulkanContext& context)
    {
        if (context.debugMessenger)
        {
            INFO_LOG("Destroying Vulkan Debugger.");

            vkDestroyDebugUtilsMessengerEXT(context.instance, context.debugMessenger, context.allocator);
            context.debugMessenger = nullptr;
        }
    }
}  // namespace C3D