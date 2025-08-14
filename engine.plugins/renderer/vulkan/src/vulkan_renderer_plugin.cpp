
#include "vulkan_renderer_plugin.h"

#include <engine.h>
#include <events/event_system.h>
#include <logger/logger.h>
#include <metrics/metrics.h>
#include <platform/platform.h>
#include <platform/platform_types.h>
#include <shaderc/shaderc.h>
#include <shaderc/status.h>
#include <systems/system_manager.h>

#include "platform/vulkan_platform.h"
#include "vulkan_allocator.h"
#include "vulkan_debugger.h"
#include "vulkan_instance.h"
#include "vulkan_swapchain.h"
#include "vulkan_types.h"
#include "vulkan_utils.h"

namespace C3D
{
    bool VulkanRendererPlugin::OnInit(const RendererPluginConfig& config)
    {
        // Our backend is implemented in Vulkan
        m_type = RendererPluginType::Vulkan;

        // Copy over the renderer flags
        m_context.flags = config.flags;

#ifdef C3D_VULKAN_USE_CUSTOM_ALLOCATOR
        m_context.allocator = Memory.Allocate<VkAllocationCallbacks>(MemoryType::Vulkan);
        if (!VulkanAllocator::Create(m_context.allocator))
        {
            ERROR_LOG("Creation of Custom Vulkan Allocator failed.");
            return false;
        }
#else
        m_context.allocator = nullptr;
#endif

        if (!VulkanInstance::Create(m_context, config.applicationName, config.applicationVersion))
        {
            ERROR_LOG("Creation of Vulkan Instance failed.");
            return false;
        }

#if defined(_DEBUG)
        if (!VulkanDebugger::Create(m_context))
        {
            ERROR_LOG("Creation of Vulkan debugger failed.");
            return false;
        }
#endif

        if (!m_context.device.Create(&m_context))
        {
            ERROR_LOG("Failed to create Vulkan Device.");
            return false;
        }

        INFO_LOG("Initialized successfully.");
        return true;
    }

    void VulkanRendererPlugin::OnShutdown()
    {
        INFO_LOG("Shutting down.");

        m_context.device.Destroy();

#if defined(_DEBUG)
        VulkanDebugger::Destroy(m_context);
#endif

        VulkanInstance::Destroy(m_context);

#ifdef C3D_VULKAN_USE_CUSTOM_ALLOCATOR
        if (m_context.allocator)
        {
            Memory.Free(m_context.allocator);
            m_context.allocator = nullptr;
        }
#endif

        INFO_LOG("Shutdown successful.");
    }

    void TransitionLayout(VulkanContext& context, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout fromLayout, VkImageLayout toLayout,
                          VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
    {
        auto graphicsQueueIndex = context.device.GetGraphicsFamilyIndex();

        VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.oldLayout            = fromLayout;
        barrier.newLayout            = toLayout;
        barrier.image                = image;
        // TODO: Only works for color images
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        // Mips
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount   = 1;
        // Start at the first layer
        barrier.subresourceRange.baseArrayLayer = 0;
        // Transition all layers at once
        barrier.subresourceRange.layerCount = 1;
        // Source and destination access masks
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstAccessMask = dstAccessMask;

        // Use a pipeline barrier to transition to the new layout
        vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    bool VulkanRendererPlugin::Begin(Window& window)
    {
        auto backendState = window.rendererState->backendState;

        // Acquire our next image index
        auto acquireResult = backendState->swapchain.AcquireNextImageIndex(UINT64_MAX, backendState);
        if (!acquireResult)
        {
            // Since we failed to acquire we should skip rendering this frame.
            return false;
        }

        // Reset command pool
        VK_CHECK(vkResetCommandPool(m_context.device.GetLogical(), backendState->GetCommandPool(), 0));

        // Get the command buffer
        auto commandBuffer = backendState->GetCommandBuffer();

        // Begin the command buffer
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        auto swapchainImage = backendState->swapchain.GetImage(backendState->imageIndex);

        // Transition image from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
        TransitionLayout(m_context, commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        VkRenderingAttachmentInfoKHR colorAttachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR };
        colorAttachmentInfo.imageView                    = backendState->swapchain.GetImageView(backendState->imageIndex);
        colorAttachmentInfo.imageLayout                  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachmentInfo.loadOp                       = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentInfo.storeOp                      = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentInfo.clearValue.color             = { 1, 0, 1, 1 };

        VkRenderingInfoKHR renderInfo   = { VK_STRUCTURE_TYPE_RENDERING_INFO_KHR };
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments    = &colorAttachmentInfo;
        renderInfo.layerCount           = 1;
        renderInfo.renderArea.offset    = { 0, 0 };
        renderInfo.renderArea.extent    = { window.width, window.height };

        vkCmdBeginRendering(commandBuffer, &renderInfo);

        return true;
    }

    bool VulkanRendererPlugin::End(Window& window)
    {
        auto backendState   = window.rendererState->backendState;
        auto commandBuffer  = backendState->GetCommandBuffer();
        auto swapchainImage = backendState->swapchain.GetImage(backendState->imageIndex);

        vkCmdEndRendering(commandBuffer);

        TransitionLayout(m_context, commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        VK_CHECK(vkEndCommandBuffer(commandBuffer));
        return true;
    }

    bool VulkanRendererPlugin::Submit(Window& window)
    {
        auto backendState     = window.rendererState->backendState;
        auto acquireSemaphore = backendState->GetAcquireSemaphore();
        auto presentSemaphore = backendState->GetPresentSemaphore();
        auto frameFence       = backendState->GetFence();
        auto commandBuffer    = backendState->GetCommandBuffer();
        auto queue            = m_context.device.GetDeviceQueue();

        VkPipelineStageFlags submitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo         = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &acquireSemaphore;
        submitInfo.pWaitDstStageMask    = &submitStageMask;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &presentSemaphore;

        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, frameFence));

        return true;
    }

    bool VulkanRendererPlugin::Present(Window& window)
    {
        auto backendState = window.rendererState->backendState;
        auto queue        = m_context.device.GetDeviceQueue();

        auto result = backendState->swapchain.Present(queue, backendState);

        auto device = m_context.device.GetLogical();
        auto fence  = backendState->GetFence();

        VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

        VK_CHECK(vkResetFences(device, 1, &fence));

        VK_CHECK(vkDeviceWaitIdle(device));

        return result;
    }

    bool VulkanRendererPlugin::OnCreateWindow(Window& window)
    {
        WindowRendererState* internal = window.rendererState;
        internal->backendState        = Memory.New<WindowRendererBackendState>(MemoryType::Vulkan);

        WindowRendererBackendState* backend = internal->backendState;

        // Create the Vulkan surface for this window
        if (!VulkanPlatform::CreateSurface(m_context, window))
        {
            ERROR_LOG("Failed to create Vulkan Surface for window: '{}'.", window.name);
            return false;
        }

        // Create the Vulkan swapchain for this window
        if (!backend->swapchain.Create(&m_context, window))
        {
            ERROR_LOG("Failed to create Vulkan swapchain for window: '{}'.", window.name);
            return false;
        }

        auto device = m_context.device.GetLogical();

        INFO_LOG("Creating semaphores.");
        for (u32 i = 0; i < MAX_FRAMES; ++i)
        {
            VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
            VK_CHECK(vkCreateSemaphore(device, &createInfo, m_context.allocator, &backend->acquireSemaphores[i]));

            VK_SET_DEBUG_OBJECT_NAME(&m_context, VK_OBJECT_TYPE_SEMAPHORE, backend->acquireSemaphores[i], String::FromFormat("VULKAN_ACQUIRE_SEMAPHORE_{}", i));
        }

        // Get the number of swapchain images
        auto swapchainImageCount = backend->swapchain.GetImageCount();
        // Resize our present semaphores array to that size
        backend->presentSemaphores.Resize(swapchainImageCount);
        // Then create the semaphores
        for (u32 i = 0; i < swapchainImageCount; ++i)
        {
            VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
            VK_CHECK(vkCreateSemaphore(device, &createInfo, m_context.allocator, &backend->presentSemaphores[i]));

            VK_SET_DEBUG_OBJECT_NAME(&m_context, VK_OBJECT_TYPE_SEMAPHORE, backend->presentSemaphores[i], String::FromFormat("VULKAN_PRESENT_SEMAPHORE_{}", i));
        }

        INFO_LOG("Creating fences.");
        for (u32 i = 0; i < MAX_FRAMES; ++i)
        {
            VkFenceCreateInfo createInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            VK_CHECK(vkCreateFence(device, &createInfo, m_context.allocator, &backend->fences[i]));

            VK_SET_DEBUG_OBJECT_NAME(&m_context, VK_OBJECT_TYPE_FENCE, backend->fences[i], String::FromFormat("VULKAN_FENCE_{}", i));
        }

        INFO_LOG("Creating command pools and buffers");
        for (u32 i = 0; i < MAX_FRAMES; ++i)
        {
            VkCommandPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
            createInfo.queueFamilyIndex        = m_context.device.GetGraphicsFamilyIndex();
            createInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

            VK_CHECK(vkCreateCommandPool(device, &createInfo, m_context.allocator, &backend->commandPools[i]));

            VK_SET_DEBUG_OBJECT_NAME(&m_context, VK_OBJECT_TYPE_COMMAND_POOL, backend->commandPools[i], String::FromFormat("VULKAN_COMMAND_POOL_{}", i));

            VkCommandBufferAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            allocateInfo.commandPool                 = backend->commandPools[i];
            allocateInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocateInfo.commandBufferCount          = 1;

            VK_CHECK(vkAllocateCommandBuffers(device, &allocateInfo, &backend->commandBuffers[i]));

            VK_SET_DEBUG_OBJECT_NAME(&m_context, VK_OBJECT_TYPE_COMMAND_BUFFER, backend->commandBuffers[i], String::FromFormat("VULKAN_COMMAND_BUFFER_{}", i));
        }

        return true;
    }

    bool VulkanRendererPlugin::OnResizeWindow(Window& window)
    {
        INFO_LOG("Window resized. The size is now: {}x{}", window.width, window.height);
        return window.rendererState->backendState->swapchain.Resize(window);
    }

    void VulkanRendererPlugin::OnDestroyWindow(Window& window)
    {
        WindowRendererState* internal       = window.rendererState;
        WindowRendererBackendState* backend = internal->backendState;

        auto device = m_context.device.GetLogical();

        if (backend)
        {
            INFO_LOG("Destoying Command Pools")
            for (auto pool : backend->commandPools)
            {
                vkDestroyCommandPool(device, pool, m_context.allocator);
            }

            INFO_LOG("Destroying fences.");
            for (auto fence : backend->fences)
            {
                vkDestroyFence(device, fence, m_context.allocator);
            }

            INFO_LOG("Destroying semaphores.");
            for (auto semaphore : backend->acquireSemaphores)
            {
                vkDestroySemaphore(device, semaphore, m_context.allocator);
            }

            for (auto semaphore : backend->presentSemaphores)
            {
                vkDestroySemaphore(device, semaphore, m_context.allocator);
            }
            backend->presentSemaphores.Destroy();

            // Destroy the swapchain
            backend->swapchain.Destroy();

            // Destroy the surface
            if (backend->surface)
            {
                vkDestroySurfaceKHR(m_context.instance, backend->surface, m_context.allocator);
                backend->surface = nullptr;
            }

            // Free the memory that we allocated for our backend state
            Memory.Delete(backend);
            internal->backendState = nullptr;
        }
    }

    RendererPlugin* CreatePlugin() { return Memory.New<VulkanRendererPlugin>(MemoryType::RenderSystem); }

    void DeletePlugin(RendererPlugin* plugin) { Memory.Delete(plugin); }

}  // namespace C3D
