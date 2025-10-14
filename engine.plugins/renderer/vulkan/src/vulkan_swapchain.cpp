
#include "vulkan_swapchain.h"

#include <logger/logger.h>
#include <platform/platform_types.h>

#include "vulkan_context.h"
#include "vulkan_utils.h"

namespace C3D
{
    bool VulkanSwapchain::Create(VulkanContext* context, const Window& window)
    {
        INFO_LOG("Creating Vulkan Swapchain.");

        // Take a copy of our context
        m_context = context;

        // Call our internal create method
        Create(window);

        return true;
    }

    bool VulkanSwapchain::Resize(const Window& window)
    {
        // Take a copy of the old swapchain handle (since it will be overridden by the Create() call)
        auto old = m_handle;

        // Create a new swapchain
        Create(window);

        // Wait for the device to be idle (otherwise we can't destroy the old swapchain yet)
        m_context->device.WaitIdle();

        // Destroy the old one
        vkDestroySwapchainKHR(m_context->device.GetLogical(), old, m_context->allocator);

        return true;
    }

    void VulkanSwapchain::Destroy()
    {
        INFO_LOG("Destroying Vulkan Swapchain.");
        auto device = m_context->device.GetLogical();

        // Destroy the swapchain itself
        vkDestroySwapchainKHR(device, m_handle, m_context->allocator);
    }

    bool VulkanSwapchain::AcquireNextImageIndex(const u64 timeoutNs, WindowRendererBackendState* backendState)
    {
        auto acquireSemaphore = backendState->GetAcquireSemaphore();

        auto result = vkAcquireNextImageKHR(m_context->device.GetLogical(), m_handle, timeoutNs, acquireSemaphore, 0, &backendState->imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            INFO_LOG("Swapchain is out of date. Can't be used for rendering this frame.");
            return false;
        }

        VK_CHECK_SWAPCHAIN(result, "vkAcquireNextImageKHR produced invalid result.");
        return true;
    }

    bool VulkanSwapchain::Present(WindowRendererBackendState* backendState)
    {
        auto presentSemaphore = backendState->GetPresentSemaphore();
        auto frameFence       = backendState->GetFence();

        // Return the image to the SwapChain for presentation
        VkPresentInfoKHR presentInfo   = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &presentSemaphore;
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &m_handle;
        presentInfo.pImageIndices      = &backendState->imageIndex;

        auto presentQueue = m_context->device.GetDeviceQueue();
        VK_CHECK_SWAPCHAIN(vkQueuePresentKHR(presentQueue, &presentInfo), "vkQueuePresentKHR returned an unexpected result.");

        // Wait for our fences
        auto device = m_context->device.GetLogical();
        VK_CHECK(vkWaitForFences(device, 1, &frameFence, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(device, 1, &frameFence));

        return true;
    }

    bool VulkanSwapchain::Create(const Window& window)
    {
        WindowRendererState* windowState               = window.rendererState;
        WindowRendererBackendState* windowBackendState = window.rendererState->backendState;

        // Query the swapchain support since it might have changed
        m_context->device.QuerySwapchainSupport(windowBackendState->surface);

        // Get the capabilities from our device
        const auto& capabilities = m_context->device.GetSurfaceCapabilities();

        VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        createInfo.surface                  = windowBackendState->surface;
        createInfo.minImageCount            = Max(MIN_IMAGES, capabilities.minImageCount);

        m_surfaceFormat            = m_context->device.GetPreferredSurfaceFormat();
        createInfo.imageFormat     = m_surfaceFormat.format;
        createInfo.imageColorSpace = m_surfaceFormat.colorSpace;

        createInfo.imageExtent.width     = window.width;
        createInfo.imageExtent.height    = window.height;
        createInfo.imageArrayLayers      = 1;
        createInfo.imageUsage            = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        createInfo.queueFamilyIndexCount = 1;
        auto familyIndex                 = m_context->device.GetGraphicsFamilyIndex();
        createInfo.pQueueFamilyIndices   = &familyIndex;
        createInfo.presentMode           = GetPresentMode();

        createInfo.preTransform   = capabilities.currentTransform;
        createInfo.compositeAlpha = GetCompositeAlpha(capabilities);
        createInfo.clipped        = VK_TRUE;
        // NOTE: This handle will be VK_NULL the first time we create the swapchain
        createInfo.oldSwapchain = m_handle;

        auto device = m_context->device.GetLogical();

        // Create our swapchain
        auto result = vkCreateSwapchainKHR(device, &createInfo, m_context->allocator, &m_handle);
        if (!VkUtils::IsSuccess(result))
        {
            ERROR_LOG("vkCreateSwapchainKHR failed with error: '{}'.", VkUtils::ResultString(result));
            return false;
        }

        // Set the debug name for the swapchain handle
        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_SWAPCHAIN_KHR, m_handle, String::FromFormat("{}_SWAPCHAIN", window.name));

        // Obtain the number of images in our swapchain
        result = vkGetSwapchainImagesKHR(device, m_handle, &m_imageCount, nullptr);
        if (!VkUtils::IsSuccess(result))
        {
            ERROR_LOG("vkGetSwapchainImagesKHR(1) failed with error: '{}'.", VkUtils::ResultString(result));
            return false;
        }

        // Resize our array to hold all the images
        m_images.Resize(m_imageCount);

        // Obtain the actual images from our swapchain
        result = vkGetSwapchainImagesKHR(device, m_handle, &m_imageCount, m_images.GetData());
        if (!VkUtils::IsSuccess(result))
        {
            ERROR_LOG("vkGetSwapchainImagesKHR(2) failed with error: '{}'.", VkUtils::ResultString(result));
            return false;
        }

        // Set some debug object names for each image
        for (u32 i = 0; i < m_imageCount; i++)
        {
            VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_IMAGE, m_images[i], String::FromFormat("{}_SWAPCHAIN_IMAGE_{}", window.name, i));
        }

        return true;
    }

    VkPresentModeKHR VulkanSwapchain::GetPresentMode() const
    {
        // VSync is enabled
        if (m_context->flags & FlagVSync)
        {
            if (!(m_context->flags & FlagPowerSaving))
            {
                // If power saving is not enabled we can try to see if Mailbox is supported
                // this will generate frames as fast as it can (which is less power efficient but it reduces input lag)
                const auto& presentModes = m_context->device.GetPresentModes();
                for (auto presentMode : presentModes)
                {
                    if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
                    {
                        return presentMode;
                    }
                }
            }

            // If power saving is enabled or mailbox is not supported we fallback to FIFO
            // which is guaranteed to be supported by all GPUs
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        // We use immediate mode if VSync is disabled which will render as much fps as possible
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    VkCompositeAlphaFlagBitsKHR VulkanSwapchain::GetCompositeAlpha(VkSurfaceCapabilitiesKHR capabilities) const
    {
        if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
        {
            return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        }
        if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
        {
            return VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
        }
        if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
        {
            return VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
        }
        if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
        {
            return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        }

        FATAL_LOG("Could not find a supported composite alpha from the surface capabilities.");
        return VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR;
    }
}  // namespace C3D