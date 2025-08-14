
#include "vulkan_renderer_plugin.h"

#include <engine.h>
#include <events/event_system.h>
#include <logger/logger.h>
#include <metrics/metrics.h>
#include <platform/platform.h>
#include <platform/platform_types.h>
#include <resources/managers/binary_manager.h>
#include <resources/resource_system.h>
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
    VkShaderModule LoadShader(VulkanContext& context, const char* name)
    {
        BinaryResource binary;
        if (!Resources.Read(name, binary))
        {
            FATAL_LOG("Failed to read '{}' source", name);
        }

        VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize                 = binary.size;
        createInfo.pCode                    = reinterpret_cast<const u32*>(binary.data);

        VkShaderModule shaderModule;
        VK_CHECK(vkCreateShaderModule(context.device.GetLogical(), &createInfo, context.allocator, &shaderModule));

        Resources.Cleanup(binary);

        return shaderModule;
    }

    VkPipelineLayout CreatePipelineLayout(VulkanContext& context)
    {
        VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

        VkPipelineLayout layout;
        vkCreatePipelineLayout(context.device.GetLogical(), &createInfo, context.allocator, &layout);

        return layout;
    }

    VkPipeline CreateGraphicsPipeline(VulkanContext& context, VkPipelineCache pipelineCache, VkShaderModule vs, VkShaderModule fs, VulkanSwapchain& swapchain,
                                      VkPipelineLayout layout, const char* shaderName)
    {
        VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        createInfo.layout                       = layout;

        VkPipelineShaderStageCreateInfo stages[2] = {};

        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vs;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fs;
        stages[1].pName  = "main";

        createInfo.stageCount = 2;
        createInfo.pStages    = stages;

        VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        createInfo.pVertexInputState                     = &vertexInput;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        createInfo.pInputAssemblyState                       = &inputAssembly;

        VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount                     = 1;
        viewportState.scissorCount                      = 1;
        createInfo.pViewportState                       = &viewportState;

        VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizationState.lineWidth                              = 1.f;
        createInfo.pRasterizationState                            = &rasterizationState;

        VkPipelineMultisampleStateCreateInfo multiSampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multiSampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
        createInfo.pMultisampleState                          = &multiSampleState;

        VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        createInfo.pDepthStencilState                           = &depthStencilState;

        VkPipelineColorBlendAttachmentState colorAttachmentState;
        colorAttachmentState.blendEnable         = VK_TRUE;
        colorAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorAttachmentState.colorBlendOp        = VK_BLEND_OP_ADD;
        colorAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorAttachmentState.alphaBlendOp        = VK_BLEND_OP_ADD;
        colorAttachmentState.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlendState.logicOpEnable                       = VK_FALSE;
        colorBlendState.logicOp                             = VK_LOGIC_OP_COPY;
        colorBlendState.attachmentCount                     = 1;
        colorBlendState.pAttachments                        = &colorAttachmentState;
        createInfo.pColorBlendState                         = &colorBlendState;

        VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicState.dynamicStateCount                = 2;
        dynamicState.pDynamicStates                   = dynamicStates;
        createInfo.pDynamicState                      = &dynamicState;

        // NOTE: Because we are using dynamic rendering this can be set to VK_NULL_HANDLE
        createInfo.renderPass         = VK_NULL_HANDLE;
        createInfo.basePipelineHandle = VK_NULL_HANDLE;
        createInfo.basePipelineIndex  = -1;

        // NOTE: Because we are using dynamic rendering we need to provide this structure to pNext of createInfo
        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        pipelineRenderingCreateInfo.colorAttachmentCount          = 1;

        // TODO: Quite bad since this assumes we will always have the same single format as was intially picked for the provided swapchain
        auto format                                         = swapchain.GetImageFormat();
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &format;
        createInfo.pNext                                    = &pipelineRenderingCreateInfo;

        VkPipeline pipeline;
        VK_CHECK(vkCreateGraphicsPipelines(context.device.GetLogical(), pipelineCache, 1, &createInfo, context.allocator, &pipeline));

        VK_SET_DEBUG_OBJECT_NAME(&context, VK_OBJECT_TYPE_PIPELINE, pipeline, String::FromFormat("PIPELINE_{}", shaderName));

        return pipeline;
    }

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
        constexpr VkClearColorValue clearColor = { 30.f / 255.f, 54.f / 255.f, 42.f / 255.f, 1 };

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

        TransitionLayout(m_context, commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        VkRenderingAttachmentInfo colorAttachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        colorAttachmentInfo.imageView                 = backendState->swapchain.GetImageView(backendState->imageIndex);
        colorAttachmentInfo.imageLayout               = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachmentInfo.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentInfo.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentInfo.clearValue.color          = clearColor;

        VkRenderingInfo renderInfo      = { VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments    = &colorAttachmentInfo;
        renderInfo.layerCount           = 1;
        renderInfo.renderArea.offset    = { 0, 0 };
        renderInfo.renderArea.extent    = { window.width, window.height };

        vkCmdBeginRendering(commandBuffer, &renderInfo);

        // TODO: move the actual rendering somewhere else
        {
            VkViewport viewport = { 0, 0, static_cast<f32>(window.width), static_cast<f32>(window.height), 0.f, 1.f };
            VkRect2D scissor    = { { 0, 0 }, { window.width, window.height } };

            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_trianglePipeline);
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        }

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

        // Increment the frame index since we have moved on to the next frame
        backendState->frameIndex++;

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

        {
            // TODO: This should not be here! It does not depend on the window we just need access to the swapchain
            // Load default shader
            m_vertexShader   = LoadShader(m_context, "triangle.vert.spv");
            m_fragmentShader = LoadShader(m_context, "triangle.frag.spv");

            VkPipelineCache cache = VK_NULL_HANDLE;

            m_triangleLayout = CreatePipelineLayout(m_context);
            m_trianglePipeline =
                CreateGraphicsPipeline(m_context, cache, m_vertexShader, m_fragmentShader, backend->swapchain, m_triangleLayout, "TRIANGLE_SHADER");
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
            {
                // TODO: This should not be here! It does not depend on the window we just need access to the swapchain
                vkDestroyPipeline(device, m_trianglePipeline, m_context.allocator);

                vkDestroyPipelineLayout(device, m_triangleLayout, m_context.allocator);

                vkDestroyShaderModule(device, m_vertexShader, m_context.allocator);
                vkDestroyShaderModule(device, m_fragmentShader, m_context.allocator);
            }

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
