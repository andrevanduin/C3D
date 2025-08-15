
#include "vulkan_swapchain.h"

#include <logger/logger.h>
#include <platform/platform_types.h>

#include "vulkan_context.h"
#include "vulkan_utils.h"

namespace C3D
{
    bool VulkanSwapchain::Create(VulkanContext* context, Window& window)
    {
        INFO_LOG("Creating Vulkan Swapchain.");

        // Take a copy of our context
        m_context = context;

        // Call our internal create method
        Create(window);

        return true;
    }

    bool VulkanSwapchain::Resize(Window& window)
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

        // Destroy the views if needed
        for (u32 i = 0; i < m_imageCount; ++i)
        {
            if (m_views[i])
            {
                vkDestroyImageView(device, m_views[i], m_context->allocator);
            }
        }

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

    bool VulkanSwapchain::Present(VkQueue presentQueue, WindowRendererBackendState* backendState)
    {
        auto presentSemaphore = backendState->GetPresentSemaphore();

        // Return the image to the SwapChain for presentation
        VkPresentInfoKHR presentInfo   = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &presentSemaphore;
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &m_handle;
        presentInfo.pImageIndices      = &backendState->imageIndex;

        VK_CHECK_SWAPCHAIN(vkQueuePresentKHR(presentQueue, &presentInfo), "vkQueuePresentKHR returned an unexpected result.");
        return true;
    }

    void VulkanSwapchain::Create(Window& window)
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

        m_surfaceFormat            = GetSurfaceFormat();
        createInfo.imageFormat     = m_surfaceFormat.format;
        createInfo.imageColorSpace = m_surfaceFormat.colorSpace;

        createInfo.imageExtent.width     = window.width;
        createInfo.imageExtent.height    = window.height;
        createInfo.imageArrayLayers      = 1;
        createInfo.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
        VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, m_context->allocator, &m_handle));

        // Obtain the number of images in our swapchain
        VK_CHECK(vkGetSwapchainImagesKHR(device, m_handle, &m_imageCount, nullptr));

        // Resize our array to hold all the images
        m_images.Resize(m_imageCount);
        m_views.Resize(m_imageCount);

        // Obtain the actual images from our swapchain
        VK_CHECK(vkGetSwapchainImagesKHR(device, m_handle, &m_imageCount, m_images.GetData()));

        // Obtain image views
        for (u32 i = 0; i < m_imageCount; ++i)
        {
            if (m_views[i])
            {
                // We already have old image views which we should destroy
                vkDestroyImageView(device, m_views[i], m_context->allocator);
            }

            m_views[i] = VulkanUtils::CreateImageView(m_context, m_images[i], createInfo.imageFormat);
        }

        // Set some debug object names for easier debugging down the line
        for (u32 i = 0; i < m_imageCount; i++)
        {
            VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_IMAGE, m_images[i], String::FromFormat("SWAPCHAIN_IMAGE_{}", i));
            VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_IMAGE_VIEW, m_views[i], String::FromFormat("SWAPCHAIN_IMAGE_VIEW_{}", i));
        }
    }

    VkSurfaceFormatKHR VulkanSwapchain::GetSurfaceFormat() const
    {
        const auto& formats = m_context->device.GetSurfaceFormats();
        for (auto& format : formats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                // We have found our preferred format so let's return it.
                return format;
            }
        }

        WARN_LOG("Could not find Preferred Swapchain ImageFormat. Falling back to first format in the list.");
        return formats[0];
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