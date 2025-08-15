
#include <defines.h>

#ifdef C3D_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Undef Windows macros that cause issues with C3D Engine
#undef CopyFile
#undef max
#undef min
#undef RGB
#undef CreateWindow

#include <platform/platform.h>
#include <platform/platform_types.h>
#include <systems/system_manager.h>
#include <volk.h>

#include "vulkan_context.h"
#include "vulkan_platform.h"
#include "vulkan_types.h"
#include "vulkan_utils.h"

namespace C3D
{
    struct WindowPlatformState
    {
        HWND hwnd;
    };

    struct Win32HandleInfo
    {
        HINSTANCE hInstance = nullptr;
    };

    bool VulkanPlatform::CreateSurface(VulkanContext& context, Window& window)
    {
        INFO_LOG("Creating Win32 surface.");

        auto handle = static_cast<Win32HandleInfo*>(Platform::GetHandleInfo());

        VkWin32SurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
        createInfo.hinstance                   = handle->hInstance;
        createInfo.hwnd                        = window.platformState->hwnd;

        auto result = vkCreateWin32SurfaceKHR(context.instance, &createInfo, context.allocator, &window.rendererState->backendState->surface);
        if (!VulkanUtils::IsSuccess(result))
        {
            ERROR_LOG("vkCreateWin32SurfaceKHR failed with the following error: '{}'.", VulkanUtils::ResultString(result));
            return false;
        }

        return true;
    }

    DynamicArray<const char*> VulkanPlatform::GetRequiredExtensionNames() { return { "VK_KHR_win32_surface" }; }

    bool VulkanPlatform::GetPresentationSupport(VkPhysicalDevice physicalDevice, u32 queueFamilyIndex)
    {
        return static_cast<bool>(vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, queueFamilyIndex));
    }
}  // namespace C3D

#endif
