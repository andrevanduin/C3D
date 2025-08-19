
#include "vulkan_renderer_plugin.h"

#include <assets/managers/binary_manager.h>
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
#include <shaderc/status.h>
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
    VkShaderModule LoadShader(VulkanContext& context, const char* name)
    {
        BinaryManager binaryManager;
        BinaryAsset binary;

        if (!binaryManager.Read(name, binary))
        {
            FATAL_LOG("Failed to read '{}' source", name);
        }

        VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize                 = binary.size;
        createInfo.pCode                    = reinterpret_cast<const u32*>(binary.data);

        VkShaderModule shaderModule;
        VK_CHECK(vkCreateShaderModule(context.device.GetLogical(), &createInfo, context.allocator, &shaderModule));

        BinaryManager::Cleanup(binary);

        return shaderModule;
    }

    VkPipelineLayout CreatePipelineLayout(VulkanContext& context, VkDescriptorSetLayout& setLayout)
    {
        auto device = context.device.GetLogical();

#if C3D_MESH_SHADER
        VkDescriptorSetLayoutBinding setBindings[2] = {};
        setBindings[0].binding                      = 0;
        setBindings[0].descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        setBindings[0].descriptorCount              = 1;
        setBindings[0].stageFlags                   = VK_SHADER_STAGE_MESH_BIT_EXT;
        setBindings[1].binding                      = 1;
        setBindings[1].descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        setBindings[1].descriptorCount              = 1;
        setBindings[1].stageFlags                   = VK_SHADER_STAGE_MESH_BIT_EXT;
#else
        VkDescriptorSetLayoutBinding setBindings[1] = {};
        setBindings[0].binding                      = 0;
        setBindings[0].descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        setBindings[0].descriptorCount              = 1;
        setBindings[0].stageFlags                   = VK_SHADER_STAGE_VERTEX_BIT;
#endif

        VkDescriptorSetLayoutCreateInfo setCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        setCreateInfo.flags                           = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

#if !C3D_FVF
        setCreateInfo.bindingCount = ARRAY_SIZE(setBindings);
        setCreateInfo.pBindings    = setBindings;
#endif

        VK_CHECK(vkCreateDescriptorSetLayout(device, &setCreateInfo, context.allocator, &setLayout));

        VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        createInfo.setLayoutCount             = 1;
        createInfo.pSetLayouts                = &setLayout;

        VkPipelineLayout layout;
        VK_CHECK(vkCreatePipelineLayout(device, &createInfo, context.allocator, &layout));

        return layout;
    }

    VkPipeline CreateGraphicsPipeline(VulkanContext& context, VkPipelineCache pipelineCache, VkShaderModule vs, VkShaderModule fs, VulkanSwapchain& swapchain,
                                      VkPipelineLayout layout, const char* shaderName)
    {
        VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        createInfo.layout                       = layout;

        VkPipelineShaderStageCreateInfo stages[2] = {};

        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

#if C3D_MESH_SHADER
        stages[0].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
#else
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
#endif

        stages[0].module = vs;
        stages[0].pName  = "main";

        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fs;
        stages[1].pName  = "main";

        createInfo.stageCount = 2;
        createInfo.pStages    = stages;

        VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
#if C3D_FVF
        VkVertexInputBindingDescription fvfBindings[1] = {};
        fvfBindings[0].binding                         = 0;
        fvfBindings[0].stride                          = sizeof(Vertex);
        fvfBindings[0].inputRate                       = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription fvfAttributes[3] = {};
        fvfAttributes[0].location                          = 0;
        fvfAttributes[0].format                            = VK_FORMAT_R16G16B16_SFLOAT;
        fvfAttributes[0].offset                            = offsetof(Vertex, vx);
        fvfAttributes[1].location                          = 1;
        fvfAttributes[1].format                            = VK_FORMAT_R8G8B8A8_UINT;
        fvfAttributes[1].offset                            = offsetof(Vertex, nx);
        fvfAttributes[2].location                          = 2;
        fvfAttributes[2].format                            = VK_FORMAT_R16G16_SFLOAT;
        fvfAttributes[2].offset                            = offsetof(Vertex, tx);

        vertexInput.vertexBindingDescriptionCount   = ARRAY_SIZE(fvfBindings);
        vertexInput.pVertexBindingDescriptions      = fvfBindings;
        vertexInput.vertexAttributeDescriptionCount = ARRAY_SIZE(fvfAttributes);
        vertexInput.pVertexAttributeDescriptions    = fvfAttributes;
#endif
        createInfo.pVertexInputState = &vertexInput;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        createInfo.pInputAssemblyState                       = &inputAssembly;

        VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount                     = 1;
        viewportState.scissorCount                      = 1;
        createInfo.pViewportState                       = &viewportState;

        VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizationState.lineWidth                              = 1.f;
        rasterizationState.frontFace                              = VK_FRONT_FACE_CLOCKWISE;
        rasterizationState.cullMode                               = VK_CULL_MODE_BACK_BIT;
        createInfo.pRasterizationState                            = &rasterizationState;

        VkPipelineMultisampleStateCreateInfo multiSampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multiSampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
        createInfo.pMultisampleState                          = &multiSampleState;

        VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        createInfo.pDepthStencilState                           = &depthStencilState;

        VkPipelineColorBlendAttachmentState colorAttachmentState = {};
        colorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

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

#if C3D_MESH_SHADER
        MeshUtils::BuildMeshlets(m_mesh);
#endif

        // Create our buffers
        if (!m_context.stagingBuffer.Create(&m_context, "STAGING", MebiBytes(128), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            ERROR_LOG("Failed to create vertex buffer.");
            return false;
        }

#if C3D_FVF
        if (!m_vertexBuffer.Create(&m_context, "VERTEX", MebiBytes(128), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create vertex buffer.");
            return false;
        }
#else
        if (!m_vertexBuffer.Create(&m_context, "VERTEX", MebiBytes(128), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create vertex buffer.");
            return false;
        }
#endif

        if (!m_indexBuffer.Create(&m_context, "INDEX", MebiBytes(128), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create index buffer.");
            return false;
        }

#if C3D_MESH_SHADER
        if (!m_meshBuffer.Create(&m_context, "MESH", MebiBytes(128), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create mesh buffer.");
            return false;
        }
#endif

        INFO_LOG("Initialized successfully.");
        return true;
    }

    void VulkanRendererPlugin::OnShutdown()
    {
        INFO_LOG("Shutting down.");

        MeshManager::Cleanup(m_mesh);

        INFO_LOG("Destroying Vulkan buffers.");
        m_vertexBuffer.Destroy();
        m_indexBuffer.Destroy();

#if C3D_MESH_SHADER
        m_meshBuffer.Destroy();
#endif

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

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_meshPipeline);

#if C3D_FVF
            VkDeviceSize vbOffset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, m_vertexBuffer.GetHandlePtr(), &vbOffset);
            vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, static_cast<u32>(m_mesh.indices.Size()), 1, 0, 0, 0);
#elif C3D_MESH_SHADER
            VkDescriptorBufferInfo vbInfo = {};
            vbInfo.buffer                 = m_vertexBuffer.GetHandle();
            vbInfo.offset                 = 0;
            vbInfo.range                  = m_vertexBuffer.GetSize();

            VkDescriptorBufferInfo mbInfo = {};
            mbInfo.buffer                 = m_meshBuffer.GetHandle();
            mbInfo.offset                 = 0;
            mbInfo.range                  = m_meshBuffer.GetSize();

            VkWriteDescriptorSet descriptors[2] = {};
            descriptors[0].sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptors[0].dstBinding           = 0;
            descriptors[0].descriptorCount      = 1;
            descriptors[0].descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptors[0].pBufferInfo          = &vbInfo;

            descriptors[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptors[1].dstBinding      = 1;
            descriptors[1].descriptorCount = 1;
            descriptors[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptors[1].pBufferInfo     = &mbInfo;

            vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_meshLayout, 0, ARRAY_SIZE(descriptors), descriptors);
            vkCmdDrawMeshTasksEXT(commandBuffer, m_mesh.meshlets.Size(), 1, 1);
#else
            VkDescriptorBufferInfo vbInfo = {};
            vbInfo.buffer                 = m_vertexBuffer.GetHandle();
            vbInfo.offset                 = 0;
            vbInfo.range                  = m_vertexBuffer.GetSize();

            VkWriteDescriptorSet descriptors[1] = {};
            descriptors[0].sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptors[0].dstBinding           = 0;
            descriptors[0].descriptorCount      = 1;
            descriptors[0].descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptors[0].pBufferInfo          = &vbInfo;

            vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_meshLayout, 0, ARRAY_SIZE(descriptors), descriptors);

            vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, static_cast<u32>(m_mesh.indices.Size()), 1, 0, 0, 0);
#endif
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

        Platform::SetWindowTitle(
            window, String::FromFormat("cpu: {:.2f} ms; gpu: {:.2f} ms; triangles: {}; vertices: {}; indices: {}; meshlets: {}", m_frameCpuAvg, m_frameGpuAvg,
                                       m_mesh.indices.Size() / 3, m_mesh.vertices.Size(), m_mesh.indices.Size(), m_mesh.meshlets.Size()));

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
#if C3D_FVF
            m_vertexShader = LoadShader(m_context, "meshfvf.vert.spv");
#elif C3D_MESH_SHADER
            m_vertexShader = LoadShader(m_context, "meshlet.mesh.spv");
#else
            m_vertexShader = LoadShader(m_context, "mesh.vert.spv");
#endif

            m_fragmentShader = LoadShader(m_context, "mesh.frag.spv");

            VkPipelineCache cache = VK_NULL_HANDLE;

            m_meshLayout   = CreatePipelineLayout(m_context, m_setLayout);
            m_meshPipeline = CreateGraphicsPipeline(m_context, cache, m_vertexShader, m_fragmentShader, backend->swapchain, m_meshLayout, "MESH_SHADER");

            auto commandBuffer = backend->GetCommandBuffer();
            auto commandPool   = backend->GetCommandPool();

            m_vertexBuffer.Upload(commandBuffer, commandPool, m_mesh.vertices.GetData(), sizeof(Vertex) * m_mesh.vertices.Size());
            m_indexBuffer.Upload(commandBuffer, commandPool, m_mesh.indices.GetData(), sizeof(u32) * m_mesh.indices.Size());

#if C3D_MESH_SHADER
            m_meshBuffer.Upload(commandBuffer, commandPool, m_mesh.meshlets.GetData(), sizeof(Meshlet) * m_mesh.meshlets.Size());
#endif
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

        // Wait for our device to go idle
        m_context.device.WaitIdle();

        if (backend)
        {
            auto device = m_context.device.GetLogical();

            {
                // TODO: This should not be here! It does not depend on the window we just need access to the swapchain

                vkDestroyDescriptorSetLayout(device, m_setLayout, m_context.allocator);
                vkDestroyPipelineLayout(device, m_meshLayout, m_context.allocator);
                vkDestroyPipeline(device, m_meshPipeline, m_context.allocator);

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
