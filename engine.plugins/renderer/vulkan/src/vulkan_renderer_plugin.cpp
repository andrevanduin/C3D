
#include "vulkan_renderer_plugin.h"

#include <assets/managers/mesh_manager.h>
#include <config/config_system.h>
#include <engine.h>
#include <events/event_system.h>
#include <logger/logger.h>
#include <metrics/metrics.h>
#include <platform/platform.h>
#include <platform/platform_types.h>
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

        MeshManager meshManager;
        String meshName;
        if (!Config.GetProperty("TestMesh", meshName))
        {
            ERROR_LOG("Failed to get the name of the test mesh.");
            return false;
        }

        if (!meshManager.Read(meshName, m_mesh))
        {
            ERROR_LOG("Failed to load mesh.");
            return false;
        }

        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
        {
            // Determine our upper bound of meshlets
            u32 maxMeshlets = MeshUtils::DetermineMaxMeshlets(m_mesh.indices.Size(), MESHLET_MAX_VERTICES, MESHLET_MAX_TRIANGLES);
            // Preallocate enough memory for the maximum number of meshlets
            DynamicArray<MeshUtils::Meshlet> meshlets(maxMeshlets);
            // Generate our meshlets
            u32 numMeshlets = MeshUtils::GenerateMeshlets(m_mesh, meshlets, 0.25f);
            // Resize our meshlet array to the actual number of meshlets
            meshlets.Resize(numMeshlets);
            // TODO: we don't really need this but this makes sure we can assume that we need all 32 meshlets in task shader
            while (meshlets.Size() % 32) meshlets.EmplaceBack();

            // Reserve enough space for all meshlets
            m_mesh.meshlets.Reserve(meshlets.Size());

            for (const auto& meshlet : meshlets)
            {
                Meshlet m       = {};
                m.vertexCount   = meshlet.vertexCount;
                m.triangleCount = meshlet.triangleCount;
                m.dataOffset    = m_mesh.meshletData.Size();

                // Populate the vertex indices
                for (u32 i = 0; i < meshlet.vertexCount; ++i)
                {
                    m_mesh.meshletData.PushBack(meshlet.vertices[i]);
                }

                if (meshlet.triangleCount > 0)
                {
                    // Get a pointer to the triangle data as u32
                    const u32* triangleData = reinterpret_cast<const u32*>(meshlet.indices);
                    // Count the number of indices (triangles * 3) and add 3 to ensure that rounded down we always have enough room
                    u32 packedTriangleCount = (meshlet.triangleCount * 3 + 3) / 4;

                    // Populate the triangle indices
                    for (u32 i = 0; i < packedTriangleCount; ++i)
                    {
                        m_mesh.meshletData.PushBack(triangleData[i]);
                    }
                }

                MeshletBounds bounds = MeshUtils::GenerateMeshletBounds(m_mesh, meshlet);

                m.cone.x = bounds.coneAxis.x;
                m.cone.y = bounds.coneAxis.y;
                m.cone.z = bounds.coneAxis.z;
                m.cone.w = bounds.coneCutoff;

                m_mesh.meshlets.PushBack(m);
            }

#if 1
            constexpr vec3 view = vec3(0, 0, 1);
            u32 numCulled       = 0;

            for (auto& meshlet : m_mesh.meshlets)
            {
                if (meshlet.cone[2] > meshlet.cone[3])
                {
                    numCulled++;
                }
            }

            INFO_LOG("Number of meshlets culled with view (0, 0, 1): {}/{} ({:.2f}%)", numCulled, m_mesh.meshlets.Size(),
                     (static_cast<f32>(numCulled) / m_mesh.meshlets.Size()) * 100.f);
#endif
        }

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
                INFO_LOG("Mesh shading is now {}.", m_meshShadingEnabled ? "enabled" : "disabled");
            }
            else
            {
                WARN_LOG("Mesh shading is not support by the current GPU: '{}'.", m_context.device.GetProperties().deviceName);
            }
            return true;
        });

        INFO_LOG("Initialized successfully.");
        return true;
    }

    void VulkanRendererPlugin::OnShutdown()
    {
        INFO_LOG("Shutting down.");

        Event.UnregisterAll(EventCodeDebug0);

        MeshManager::Cleanup(m_mesh);

        if (m_context.shaderCompiler)
        {
            shaderc_compiler_release(m_context.shaderCompiler);
            m_context.shaderCompiler = nullptr;
        }

        INFO_LOG("Destroying Vulkan buffers.");
        m_vertexBuffer.Destroy();
        m_indexBuffer.Destroy();

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

    void TransitionLayout(VulkanContext& context, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout fromLayout, VkImageLayout toLayout,
                          VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
    {
        auto graphicsQueueIndex = context.device.GetGraphicsFamilyIndex();

        VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.oldLayout            = fromLayout;
        barrier.newLayout            = toLayout;
        barrier.image                = image;
        barrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        // TODO: Only works for color images
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        // Mips
        barrier.subresourceRange.baseMipLevel = 0;
        // Transition all mip levels
        barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        // Start at the first layer
        barrier.subresourceRange.baseArrayLayer = 0;
        // Transition all layers at once
        barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        // Source and destination access masks
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstAccessMask = dstAccessMask;

        // Use a pipeline barrier to transition to the new layout
        vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    bool VulkanRendererPlugin::Begin(Window& window)
    {
        constexpr VkClearColorValue clearColor = { 30.f / 255.f, 54.f / 255.f, 42.f / 255.f, 1 };
        static std::vector<MeshDraw> draws;

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
            VkViewport viewport = { 0, static_cast<f32>(window.height), static_cast<f32>(window.width), -static_cast<f32>(window.height), 0.f, 1.0f };
            VkRect2D scissor    = { { 0, 0 }, { window.width, window.height } };

            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            draws.resize(m_drawCount);
            u32 c = Sqrt(m_drawCount);
            for (u32 i = 0; i < m_drawCount; ++i)
            {
                draws[i].offset.x = static_cast<f32>(i % c) * (1.f / c) + (0.5f / c);
                draws[i].offset.y = static_cast<f32>(i / c) * (1.f / c) + (0.5f / c);
                draws[i].scale.x  = 1.f / c;
                draws[i].scale.y  = 1.f / c;
            }

            if (m_meshShadingEnabled)
            {
                m_meshletShader.Bind(commandBuffer);

                DescriptorInfo descriptors[] = { m_vertexBuffer.GetHandle(), m_meshletBuffer.GetHandle(), m_meshletDataBuffer.GetHandle() };
                m_meshletShader.PushDescriptorSet(commandBuffer, descriptors);

                for (auto& draw : draws)
                {
                    m_meshletShader.PushConstants(commandBuffer, &draw, sizeof(draw));
                    vkCmdDrawMeshTasksEXT(commandBuffer, m_mesh.meshlets.Size() / 32, 1, 1);
                }
            }
            else
            {
                m_meshShader.Bind(commandBuffer);

                DescriptorInfo descriptors[] = { m_vertexBuffer.GetHandle() };
                m_meshShader.PushDescriptorSet(commandBuffer, descriptors);

                vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);

                for (auto& draw : draws)
                {
                    m_meshShader.PushConstants(commandBuffer, &draw, sizeof(draw));
                    vkCmdDrawIndexed(commandBuffer, static_cast<u32>(m_mesh.indices.Size()), 1, 0, 0, 0);
                }
            }
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

        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, 1);

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
        static String titleText;

        auto backendState = window.rendererState->backendState;
        auto queue        = m_context.device.GetDeviceQueue();

        auto result = backendState->swapchain.Present(queue, backendState);

        auto device = m_context.device.GetLogical();
        auto fence  = backendState->GetFence();

        VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(device, 1, &fence));

        u64 queryResults[2];
        VK_CHECK(vkGetQueryPoolResults(device, m_queryPool, 0, ARRAY_SIZE(queryResults), sizeof(queryResults), queryResults, sizeof(queryResults[0]),
                                       VK_QUERY_RESULT_64_BIT));

        auto props = m_context.device.GetProperties();

        f64 frameGpuBegin = f64(queryResults[0]) * props.limits.timestampPeriod * 1e-6;
        f64 frameGpuEnd   = f64(queryResults[1]) * props.limits.timestampPeriod * 1e-6;

        f64 frameCpuEnd = Platform::GetAbsoluteTime() * 1000;

        m_frameCpuAvg = m_frameCpuAvg * 0.95 + (frameCpuEnd - m_frameCpuBegin) * 0.05;
        m_frameGpuAvg = m_frameGpuAvg * 0.95 + (frameGpuEnd - frameGpuBegin) * 0.05;

        f64 trianglesPerSecond = static_cast<f64>(m_drawCount) * static_cast<f64>(m_mesh.indices.Size() / 3) / (m_frameGpuAvg * 1e-3);

        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING) && m_meshShadingEnabled)
        {
            titleText.Clear();
            titleText.Format("cpu: {:.2f} ms; gpu: {:.2f} ms; triangles: {}; vertices: {}; indices: {}; meshlets: {}; Mesh Shading: ON; {:.1f}B tri/sec",
                             m_frameCpuAvg, m_frameGpuAvg, m_mesh.indices.Size() / 3, m_mesh.vertices.Size(), m_mesh.indices.Size(), m_mesh.meshlets.Size(),
                             trianglesPerSecond * 1e-9);
        }
        else
        {
            titleText.Clear();
            titleText.Format("cpu: {:.2f} ms; gpu: {:.2f} ms; triangles: {}; vertices: {}; indices: {}; meshlets: {}; Mesh Shading: OFF; {:.1f}B tri/sec",
                             m_frameCpuAvg, m_frameGpuAvg, m_mesh.indices.Size() / 3, m_mesh.vertices.Size(), m_mesh.indices.Size(), 0,
                             trianglesPerSecond * 1e-9);
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
        createInfo.format  = VK_FORMAT_R8G8B8A8_UNORM;
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
            createInfo.name              = "MESH_SHADER";
            createInfo.cache             = VK_NULL_HANDLE;
            createInfo.swapchain         = &backend->swapchain;
            createInfo.pushConstantsSize = sizeof(MeshDraw);
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

            auto commandBuffer = backend->GetCommandBuffer();
            auto commandPool   = backend->GetCommandPool();

            m_vertexBuffer.Upload(commandBuffer, commandPool, m_mesh.vertices.GetData(), sizeof(Vertex) * m_mesh.vertices.Size());
            m_indexBuffer.Upload(commandBuffer, commandPool, m_mesh.indices.GetData(), sizeof(u32) * m_mesh.indices.Size());

            if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
            {
                if (!m_meshletBuffer.Upload(commandBuffer, commandPool, m_mesh.meshlets.GetData(), sizeof(Meshlet) * m_mesh.meshlets.Size()))
                {
                    ERROR_LOG("Failed to upload meshlets.");
                    return false;
                }
                if (!m_meshletDataBuffer.Upload(commandBuffer, commandPool, m_mesh.meshletData.GetData(), sizeof(u32) * m_mesh.meshletData.Size()))
                {
                    ERROR_LOG("Failed to upload meshlet data.");
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
            if (backend->colorTarget.Resize(window))
            {
                ERROR_LOG("Failed to resize Color target.");
                return false;
            }

            if (backend->depthTarget.Resize(window))
            {
                ERROR_LOG("Failed to resize Depth target.");
                return false;
            }
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

                if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
                {
                    m_meshletShader.Destroy();
                    m_meshletShaderModule.Destroy();
                    m_meshletTaskShaderModule.Destroy();
                }

                m_fragmentShaderModule.Destroy();
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

    RendererPlugin* CreatePlugin() { return Memory.New<VulkanRendererPlugin>(MemoryType::RenderSystem); }

    void DeletePlugin(RendererPlugin* plugin) { Memory.Delete(plugin); }

}  // namespace C3D
