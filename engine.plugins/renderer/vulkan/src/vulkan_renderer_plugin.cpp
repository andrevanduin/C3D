
#include "vulkan_renderer_plugin.h"

#include <assets/managers/mesh_manager.h>
#include <config/config_system.h>
#include <engine.h>
#include <events/event_system.h>
#include <logger/logger.h>
#include <metrics/metrics.h>
#include <platform/platform.h>
#include <platform/platform_types.h>
#include <random/random.h>
#include <renderer/utils/mesh_utils.h>
#include <shaderc/shaderc.h>
#include <system/system_manager.h>

#include "platform/vulkan_platform.h"
#include "vulkan_allocator.h"
#include "vulkan_debugger.h"
#include "vulkan_instance.h"
#include "vulkan_swapchain.h"
#include "vulkan_types.h"
#include "vulkan_utils.h"

namespace C3D
{
    VkQueryPool CreateQueryPool(VulkanContext& context, u32 queryCount)
    {
        VkQueryPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
        createInfo.queryType             = VK_QUERY_TYPE_TIMESTAMP;
        createInfo.queryCount            = queryCount;

        VkQueryPool queryPool = 0;
        VK_CHECK(vkCreateQueryPool(context.device.GetLogical(), &createInfo, context.allocator, &queryPool));

        return queryPool;
    }

    bool VulkanRendererPlugin::OnInit(const RendererPluginConfig& config)
    {
        // Our backend is implemented in Vulkan
        m_type = RendererPluginType::Vulkan;

        // Copy over the renderer flags
        m_context.flags = config.flags;

        VK_CHECK(volkInitialize());

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

        volkLoadInstance(m_context.instance);

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

        volkLoadDevice(m_context.device.GetLogical());

        // Initialize shaderc
        m_context.shaderCompiler = shaderc_compiler_initialize();
        if (!m_context.shaderCompiler)
        {
            ERROR_LOG("Failed to initialize shaderc compiler.");
            return false;
        }

        // Create our query pool
        m_queryPool = CreateQueryPool(m_context, 128);

        // Create our buffers
        if (!m_context.stagingBuffer.Create(&m_context, "STAGING", MebiBytes(64), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            ERROR_LOG("Failed to create vertex buffer.");
            return false;
        }

        if (!m_vertexBuffer.Create(&m_context, "VERTEX", MebiBytes(32), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create vertex buffer.");
            return false;
        }

        if (!m_indexBuffer.Create(&m_context, "INDEX", MebiBytes(32), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create index buffer.");
            return false;
        }

        if (!m_drawBuffer.Create(&m_context, "DRAW", MebiBytes(8), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create draw buffer.");
            return false;
        }

        if (!m_drawCommandBuffer.Create(&m_context, "DRAW_COMMANDS", MebiBytes(8), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create draw buffer.");
            return false;
        }

        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
        {
            if (!m_meshletBuffer.Create(&m_context, "MESHLET", MebiBytes(32), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            {
                ERROR_LOG("Failed to create mesh buffer.");
                return false;
            }

            if (!m_meshletDataBuffer.Create(&m_context, "MESHLET_DATA", MebiBytes(32), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            {
                ERROR_LOG("Failed to create mesh buffer.");
                return false;
            }
        }

        Event.Register(EventCodeDebug0, [this](const u16 code, void* sender, const EventContext& context) {
            if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
            {
                m_meshShadingEnabled ^= true;
            }
            else
            {
                WARN_LOG("Mesh shading is not support by the current GPU: '{}'.", m_context.device.GetProperties().deviceName);
            }
            return true;
        });
        Event.Register(EventCodeDebug1, [this](const u16 code, void* sender, const EventContext& context) {
            m_cullingEnabled ^= true;
            return true;
        });

        INFO_LOG("Initialized successfully.");
        return true;
    }

    void VulkanRendererPlugin::OnShutdown()
    {
        INFO_LOG("Shutting down.");

        Event.UnregisterAll(EventCodeDebug0);
        Event.UnregisterAll(EventCodeDebug1);

        m_draws.Destroy();

        if (m_context.shaderCompiler)
        {
            shaderc_compiler_release(m_context.shaderCompiler);
            m_context.shaderCompiler = nullptr;
        }

        INFO_LOG("Destroying Vulkan buffers.");
        m_vertexBuffer.Destroy();
        m_indexBuffer.Destroy();
        m_drawBuffer.Destroy();
        m_drawCommandBuffer.Destroy();

        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
        {
            m_meshletBuffer.Destroy();
            m_meshletDataBuffer.Destroy();
        }

        m_context.stagingBuffer.Destroy();

        INFO_LOG("Destroying Query pool");
        vkDestroyQueryPool(m_context.device.GetLogical(), m_queryPool, m_context.allocator);

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

    mat4 MakePerspectiveProjection(f32 fovY, f32 aspect, f32 zNear)
    {
        f32 f = 1.0f / Tan(fovY / 2.0f);
        return mat4(f / aspect, 0.0f, 0.0f, 0.0f, 0.0f, f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, zNear, 0.0f);
    }

    bool VulkanRendererPlugin::Begin(Window& window)
    {
        constexpr VkClearColorValue clearColor               = { 30.f / 255.f, 54.f / 255.f, 42.f / 255.f, 1 };
        constexpr VkClearDepthStencilValue clearDepthStencil = { 0.f, 0 };

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

        m_frameCpuBegin = Platform::GetAbsoluteTime() * 1000;

        vkCmdResetQueryPool(commandBuffer, m_queryPool, 0, 128);
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, 0);

        auto projection  = MakePerspectiveProjection(glm::radians(70.0f), static_cast<f32>(window.width) / static_cast<f32>(window.height), 0.01f);
        f32 drawDistance = 100;

        // Bind, push our descriptors and then dispatch our compute shader
        {
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, 2);

            vec4 frustum[6] = {};

            if (m_cullingEnabled)
            {
                glm::mat4 projectionT = glm::transpose(projection);

                frustum[0] = NormalizePlane(projectionT[3] + projectionT[0]);  // x + w < 0
                frustum[1] = NormalizePlane(projectionT[3] - projectionT[0]);  // x - w > 0
                frustum[2] = NormalizePlane(projectionT[3] + projectionT[1]);  // y + w < 0
                frustum[3] = NormalizePlane(projectionT[3] - projectionT[1]);  // y - w > 0
                frustum[4] = NormalizePlane(projectionT[3] - projectionT[2]);  // z - w > 0 -- reverse z
                frustum[5] = vec4(0, 0, -1, drawDistance);                     // reverse z, infinite far plane
            }

            m_drawCommandShader.Bind(commandBuffer);

            DescriptorInfo descriptors[] = { m_drawBuffer.GetHandle(), m_drawCommandBuffer.GetHandle() };
            m_drawCommandShader.PushDescriptorSet(commandBuffer, descriptors);

            m_drawCommandShader.PushConstants(commandBuffer, frustum, sizeof(frustum));
            m_drawCommandShader.Dispatch(commandBuffer, static_cast<u32>((m_draws.Size() + 31) / 32));

            VkBufferMemoryBarrier drawCommandEndBarrier =
                VulkanUtils::CreateBufferBarrier(m_drawCommandBuffer.GetHandle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, 1,
                                 &drawCommandEndBarrier, 0, nullptr);

            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, 3);
        }

        auto swapchainImage = backendState->swapchain.GetImage(backendState->imageIndex);

        // Our color and depth target need to be in ATTACHMENT OPTIMAL layout before we can start rendering
        VkImageMemoryBarrier renderBeginBarriers[] = {
            backendState->colorTarget.CreateBarrier(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
            backendState->depthTarget.CreateBarrier(0, 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
        };

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0,
                             0, ARRAY_SIZE(renderBeginBarriers), renderBeginBarriers);

        VkRenderingAttachmentInfo colorAttachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        colorAttachmentInfo.imageView                 = backendState->colorTarget.GetView();
        colorAttachmentInfo.imageLayout               = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachmentInfo.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentInfo.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentInfo.clearValue.color          = clearColor;

        VkRenderingAttachmentInfo depthAttachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        depthAttachmentInfo.clearValue.depthStencil   = clearDepthStencil;
        depthAttachmentInfo.imageView                 = backendState->depthTarget.GetView();
        depthAttachmentInfo.imageLayout               = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachmentInfo.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentInfo.storeOp                   = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        VkRenderingInfo renderInfo      = { VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments    = &colorAttachmentInfo;
        renderInfo.pDepthAttachment     = &depthAttachmentInfo;
        renderInfo.layerCount           = 1;
        renderInfo.renderArea.offset    = { 0, 0 };
        renderInfo.renderArea.extent    = { window.width, window.height };

        vkCmdBeginRendering(commandBuffer, &renderInfo);

        // First commands are to set the viewport and scissor
        vkCmdSetViewport(commandBuffer, 0, 1, &m_viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &m_scissor);

        // TODO: move the actual rendering somewhere else
        {
            Globals globals    = {};
            globals.projection = projection;

            if (m_meshShadingEnabled)
            {
                m_meshletShader.Bind(commandBuffer);

                DescriptorInfo descriptors[] = {
                    m_drawBuffer.GetHandle(),
                    m_meshletBuffer.GetHandle(),
                    m_meshletDataBuffer.GetHandle(),
                    m_vertexBuffer.GetHandle(),
                };
                m_meshletShader.PushDescriptorSet(commandBuffer, descriptors);
                m_meshletShader.PushConstants(commandBuffer, &globals, sizeof(globals));

                vkCmdDrawMeshTasksIndirectEXT(commandBuffer, m_drawCommandBuffer.GetHandle(), offsetof(MeshDrawCommand, indirectMS),
                                              static_cast<u32>(m_draws.Size()), sizeof(MeshDrawCommand));
            }
            else
            {
                m_meshShader.Bind(commandBuffer);

                DescriptorInfo descriptors[] = { m_drawBuffer.GetHandle(), m_vertexBuffer.GetHandle() };
                m_meshShader.PushDescriptorSet(commandBuffer, descriptors);

                vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);

                m_meshShader.PushConstants(commandBuffer, &globals, sizeof(globals));
                vkCmdDrawIndexedIndirect(commandBuffer, m_drawCommandBuffer.GetHandle(), offsetof(MeshDrawCommand, indirect), static_cast<u32>(m_draws.Size()),
                                         sizeof(MeshDrawCommand));
            }
        }

        return true;
    }

    bool VulkanRendererPlugin::End(Window& window)
    {
        auto backendState   = window.rendererState->backendState;
        auto commandBuffer  = backendState->GetCommandBuffer();
        auto swapchainImage = backendState->swapchain.GetImage(backendState->imageIndex);

        // End our rendering
        vkCmdEndRendering(commandBuffer);

        // Setup some copy barriers
        VkImageMemoryBarrier copyBarriers[] = {
            backendState->colorTarget.CreateBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
            VulkanUtils::CreateImageBarrier(swapchainImage, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                            VK_IMAGE_ASPECT_COLOR_BIT),
        };

        // Wait for color target to be in TRANSFER_SRC_OPTIMAL and wait for swapchain image to be in TRANSFER_DST_OPTIMAL
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0,
                             0, ARRAY_SIZE(copyBarriers), copyBarriers);

        // Copy the contents of our color target to the current swapchain image
        backendState->colorTarget.CopyTo(commandBuffer, swapchainImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // Setup a present barrier
        VkImageMemoryBarrier presentBarrier = VulkanUtils::CreateImageBarrier(
            swapchainImage, VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);

        // Wait for the swapchain image to go from TRANSFER_DST_OPTIMAL to PRESENT_SRC_KHR
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1,
                             &presentBarrier);

        // Keep track of the renderer End() timestamp
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, 1);

        // Finally we end our command buffer
        auto result = vkEndCommandBuffer(commandBuffer);
        if (!VulkanUtils::IsSuccess(result))
        {
            ERROR_LOG("vkEndCommandBuffer failed with error: '{}'.", VulkanUtils::ResultString(result));
            return false;
        }

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

        auto result = vkQueueSubmit(queue, 1, &submitInfo, frameFence);
        if (!VulkanUtils::IsSuccess(result))
        {
            ERROR_LOG("vkQueueSubmit failed with error: '{}'.", VulkanUtils::ResultString(result));
            return false;
        }

        return true;
    }

    bool VulkanRendererPlugin::Present(Window& window)
    {
        static String titleText;

        auto backendState = window.rendererState->backendState;

        auto result = backendState->swapchain.Present(backendState);

        auto device = m_context.device.GetLogical();
        auto fence  = backendState->GetFence();

        VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(device, 1, &fence));

        u64 queryResults[4];
        VK_CHECK(vkGetQueryPoolResults(device, m_queryPool, 0, ARRAY_SIZE(queryResults), sizeof(queryResults), queryResults, sizeof(queryResults[0]),
                                       VK_QUERY_RESULT_64_BIT));

        auto props = m_context.device.GetProperties();

        f64 frameGpuBegin = static_cast<f64>(queryResults[0]) * props.limits.timestampPeriod * 1e-6;
        f64 frameGpuEnd   = static_cast<f64>(queryResults[1]) * props.limits.timestampPeriod * 1e-6;
        f64 cullGpuTime   = static_cast<f64>(queryResults[3] - queryResults[2]) * props.limits.timestampPeriod * 1e-6;

        f64 frameCpuEnd = Platform::GetAbsoluteTime() * 1000;

        m_frameCpuAvg = m_frameCpuAvg * 0.95 + (frameCpuEnd - m_frameCpuBegin) * 0.05;
        m_frameGpuAvg = m_frameGpuAvg * 0.95 + (frameGpuEnd - frameGpuBegin) * 0.05;

        f64 trianglesPerSecond = static_cast<f64>(m_triangleCount) / (m_frameGpuAvg * 1e-3);
        f64 drawsPerSecond     = static_cast<f64>(m_drawCount) / (m_frameGpuAvg * 1e-3);

        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING) && m_meshShadingEnabled)
        {
            titleText.Clear();
            titleText.Format("Mesh Shading: ON; Culling: {}; cpu: {:.2f} ms; gpu: {:.2f} ms; (cull {:.2f} ms); {:.1f}B tri/sec; {:.1f}M draws/sec;",
                             m_cullingEnabled ? "ON" : "OFF", m_frameCpuAvg, m_frameGpuAvg, cullGpuTime, trianglesPerSecond * 1e-9, drawsPerSecond * 1e-6);
        }
        else
        {
            titleText.Clear();
            titleText.Format("Mesh Shading: OFF; Culling: {}; cpu: {:.2f} ms; gpu: {:.2f} ms; (cull {:.2f} ms); {:.1f}B tri/sec; {:.1f}M draws/sec;",
                             m_cullingEnabled ? "ON" : "OFF", m_frameCpuAvg, m_frameGpuAvg, cullGpuTime, trianglesPerSecond * 1e-9, drawsPerSecond * 1e-6);
        }

        Platform::SetWindowTitle(window, titleText);

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

        VulkanTextureCreateInfo createInfo;
        createInfo.name    = "COLOR_TARGET";
        createInfo.context = &m_context;
        createInfo.width   = window.width;
        createInfo.height  = window.height;
        createInfo.format  = backend->swapchain.GetImageFormat();
        createInfo.usage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        // Create color and depth target for this window
        if (!backend->colorTarget.Create(createInfo))
        {
            ERROR_LOG("Failed to create Color target for window: '{}'.", window.name);
            return false;
        }

        createInfo.name   = "DEPTH_TARGET";
        createInfo.format = VK_FORMAT_D32_SFLOAT;
        createInfo.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        if (!backend->depthTarget.Create(createInfo))
        {
            ERROR_LOG("Failed to create Depth target for window: '{}'.", window.name);
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

            if (!m_drawCommandShaderModule.Create(&m_context, "drawcmd.comp"))
            {
                ERROR_LOG("Failed to create ShaderModule");
                return false;
            }

            if (!m_meshShaderModule.Create(&m_context, "mesh.vert"))
            {
                ERROR_LOG("Failed to create ShaderModule.");
                return false;
            }

            if (!m_fragmentShaderModule.Create(&m_context, "mesh.frag"))
            {
                ERROR_LOG("Failed to create ShaderModule.");
                return false;
            }

            VulkanShaderCreateInfo createInfo;
            createInfo.context           = &m_context;
            createInfo.name              = "DRAW_COMMAND_SHADER";
            createInfo.bindPoint         = VK_PIPELINE_BIND_POINT_COMPUTE;
            createInfo.pushConstantsSize = 6 * sizeof(vec4);
            createInfo.cache             = VK_NULL_HANDLE;
            createInfo.swapchain         = &backend->swapchain;
            createInfo.modules           = { &m_drawCommandShaderModule };

            if (!m_drawCommandShader.Create(createInfo))
            {
                ERROR_LOG("Failed to create DrawCommandShader.");
                return false;
            }

            createInfo.name              = "MESH_SHADER";
            createInfo.bindPoint         = VK_PIPELINE_BIND_POINT_GRAPHICS;
            createInfo.pushConstantsSize = sizeof(Globals);
            createInfo.modules           = { &m_meshShaderModule, &m_fragmentShaderModule };

            if (!m_meshShader.Create(createInfo))
            {
                ERROR_LOG("Failed to create MeshShader.");
                return false;
            }

            if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
            {
                if (!m_meshletShaderModule.Create(&m_context, "meshlet.mesh"))
                {
                    ERROR_LOG("Failed to create ShaderModule.");
                    return false;
                }

                if (!m_meshletTaskShaderModule.Create(&m_context, "meshlet.task"))
                {
                    ERROR_LOG("Failed to create ShaderModule.");
                    return false;
                }

                // For the meshlet shader only the name and and modules change
                createInfo.name    = "MESHLET_SHADER";
                createInfo.modules = { &m_meshletTaskShaderModule, &m_meshletShaderModule, &m_fragmentShaderModule };

                if (!m_meshletShader.Create(createInfo))
                {
                    ERROR_LOG("Failed to create MeshletShader.");
                    return false;
                }
            }
        }

        return true;
    }

    bool VulkanRendererPlugin::OnResizeWindow(Window& window)
    {
        INFO_LOG("Window resized. The size is now: {}x{}", window.width, window.height);

        auto backend = window.rendererState->backendState;

        bool resizeResult = backend->swapchain.Resize(window);
        if (resizeResult)
        {
            if (!backend->colorTarget.Resize(window.width, window.height))
            {
                ERROR_LOG("Failed to resize Color target.");
                return false;
            }

            if (!backend->depthTarget.Resize(window.width, window.height))
            {
                ERROR_LOG("Failed to resize Depth target.");
                return false;
            }

            // TODO: For now we assume scissor and viewport are always the size of the full window
            SetViewport(0, static_cast<f32>(window.height), static_cast<f32>(window.width), -static_cast<f32>(window.height), 0.f, 1.f);
            SetScissor(0, 0, window.width, window.height);
        }
        return resizeResult;
    }

    void VulkanRendererPlugin::OnDestroyWindow(Window& window)
    {
        WindowRendererState* internal       = window.rendererState;
        WindowRendererBackendState* backend = internal->backendState;

        // Wait for our device to go idle
        m_context.device.WaitIdle();

        if (backend)
        {
            auto device = m_context.device.GetLogical();

            {
                // TODO: This should not be here! It does not depend on the window we just need access to the swapchain
                m_meshShader.Destroy();
                m_meshShaderModule.Destroy();
                m_fragmentShaderModule.Destroy();

                m_drawCommandShader.Destroy();
                m_drawCommandShaderModule.Destroy();

                if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
                {
                    m_meshletShader.Destroy();
                    m_meshletShaderModule.Destroy();
                    m_meshletTaskShaderModule.Destroy();
                }
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

            // Destroy the color and depth target
            backend->colorTarget.Destroy();
            backend->depthTarget.Destroy();

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

    bool VulkanRendererPlugin::UploadGeometry(const Window& window, const Geometry& geometry)
    {
        auto backend = window.rendererState->backendState;

        auto commandBuffer = backend->GetCommandBuffer();
        auto commandPool   = backend->GetCommandPool();

        if (!m_vertexBuffer.Upload(commandBuffer, commandPool, geometry.vertices.GetData(), sizeof(Vertex) * geometry.vertices.Size()))
        {
            ERROR_LOG("Failed to upload vertices.");
            return false;
        }

        if (!m_indexBuffer.Upload(commandBuffer, commandPool, geometry.indices.GetData(), sizeof(u32) * geometry.indices.Size()))
        {
            ERROR_LOG("Failed to upload indices.");
            return false;
        }

        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
        {
            if (!m_meshletBuffer.Upload(commandBuffer, commandPool, geometry.meshlets.GetData(), sizeof(Meshlet) * geometry.meshlets.Size()))
            {
                ERROR_LOG("Failed to upload meshlets.");
                return false;
            }
            if (!m_meshletDataBuffer.Upload(commandBuffer, commandPool, geometry.meshletData.GetData(), sizeof(u32) * geometry.meshletData.Size()))
            {
                ERROR_LOG("Failed to upload meshlet data.");
                return false;
            }
        }

        return true;
    }

    bool VulkanRendererPlugin::GenerateDrawCommands(const Window& window, const Geometry& geometry)
    {
        auto backend = window.rendererState->backendState;

        auto commandBuffer = backend->GetCommandBuffer();
        auto commandPool   = backend->GetCommandPool();

        m_drawCount = 25000;
        m_draws.Resize(m_drawCount);

        for (u32 i = 0; i < m_drawCount; ++i)
        {
            const auto& mesh = geometry.meshes[Random.Generate(static_cast<u64>(0), geometry.meshes.Size() - 1)];
            auto& draw       = m_draws[i];

            draw.position.x = Random.Generate(-50.f, 50.f);
            draw.position.y = Random.Generate(-50.f, 50.f);
            draw.position.z = Random.Generate(-50.f, 50.f);
            draw.scale      = Random.Generate(0.8f, 1.5f);

            vec3 axis        = vec3(Random.Generate(-1.0f, 1.0f), Random.Generate(-1.0f, 1.0f), Random.Generate(-1.0f, 1.0f));
            f32 angle        = glm::radians(Random.Generate(0.f, 90.0f));
            draw.orientation = glm::rotate(glm::quat(1, 0, 0, 0), angle, axis);

            draw.center = mesh.center;
            draw.radius = mesh.radius;

            draw.vertexOffset  = mesh.vertexOffset;
            draw.indexOffset   = mesh.indexOffset;
            draw.indexCount    = mesh.indexCount;
            draw.meshletOffset = mesh.meshletOffset;
            draw.meshletCount  = mesh.meshletCount;

            m_triangleCount += mesh.indexCount / 3;
        }

        return m_drawBuffer.Upload(commandBuffer, commandPool, m_draws.GetData(), sizeof(MeshDraw) * m_draws.Size());
    }

    void VulkanRendererPlugin::SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth, f32 maxDepth)
    {
        m_viewport = { x, y, width, height, minDepth, maxDepth };
    }

    void VulkanRendererPlugin::SetScissor(i32 offsetX, i32 offsetY, u32 width, u32 height)
    {
        VkOffset2D offset = { offsetX, offsetY };
        VkExtent2D extent = { width, height };
        m_scissor         = { offset, extent };
    }

    bool VulkanRendererPlugin::SupportsFeature(RendererSupportFlag feature)
    {
        switch (feature)
        {
            case RENDERER_SUPPORT_FLAG_MESH_SHADING:
                return m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING);
            default:
                C3D_ASSERT_MSG(false, "Unsupported RendererSupportFlag");
        }
        return false;
    }

    RendererPlugin* CreatePlugin() { return Memory.New<VulkanRendererPlugin>(MemoryType::RenderSystem); }

    void DeletePlugin(RendererPlugin* plugin) { Memory.Delete(plugin); }

}  // namespace C3D
