
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

        // Create our query pool for timestamps
        m_queryPoolTimestamps = VkUtils::CreateQueryPool(m_context, 128, VK_QUERY_TYPE_TIMESTAMP);
        // And for pipeline statistics
        m_queryPoolStatistics = VkUtils::CreateQueryPool(m_context, 1, VK_QUERY_TYPE_PIPELINE_STATISTICS);

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

        if (!m_indexBuffer.Create(&m_context, "INDEX", MebiBytes(64), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create index buffer.");
            return false;
        }

        if (!m_meshBuffer.Create(&m_context, "MESH", MebiBytes(32), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create mesh buffer.");
            return false;
        }

        if (!m_drawBuffer.Create(&m_context, "DRAW", MebiBytes(64), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create draw buffer.");
            return false;
        }

        if (!m_drawCommandBuffer.Create(&m_context, "DRAW_COMMAND", MebiBytes(64), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create draw command buffer.");
            return false;
        }

        if (!m_drawCommandCountBuffer.Create(&m_context, "DRAW_COMMAND_COUNT", 4,
                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create draw command count buffer.");
            return false;
        }

        if (!m_drawVisibilityBuffer.Create(&m_context, "DRAW_VISIBILITY", MebiBytes(8), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create draw visibility buffer.");
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
            switch (context.data.u32[0])
            {
                case C3D::KeyM:
                    if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
                    {
                        m_meshShadingEnabled ^= true;
                    }
                    else
                    {
                        WARN_LOG("Mesh shading is not support by the current GPU: '{}'.", m_context.device.GetProperties().deviceName);
                    }
                    break;
                case C3D::KeyC:
                    m_cullingEnabled ^= true;
                    break;
                case C3D::KeyO:
                    m_occlusionCullingEnabled ^= true;
                    break;
                case C3D::KeyL:
                    m_lodEnabled ^= true;
                    break;
                case C3D::KeyP:
                    m_debugPyramid ^= true;
                    break;
            }

            return true;
        });
        Event.Register(EventCodeDebug1, [this](const u16 code, void* sender, const EventContext& context) {
            m_debugPyramidLevel = context.data.u32[0];
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
        m_meshBuffer.Destroy();
        m_drawBuffer.Destroy();
        m_drawCommandBuffer.Destroy();
        m_drawCommandCountBuffer.Destroy();
        m_drawVisibilityBuffer.Destroy();

        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
        {
            m_meshletBuffer.Destroy();
            m_meshletDataBuffer.Destroy();
        }

        m_context.stagingBuffer.Destroy();

        INFO_LOG("Destroying Query pools");
        vkDestroyQueryPool(m_context.device.GetLogical(), m_queryPoolTimestamps, m_context.allocator);
        vkDestroyQueryPool(m_context.device.GetLogical(), m_queryPoolStatistics, m_context.allocator);

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

    void VulkanRendererPlugin::BeginRendering(VkCommandBuffer commandBuffer, VkImageView colorView, VkImageView depthView, const VkClearColorValue& clearColor,
                                              const VkClearDepthStencilValue& clearDepthStencil, u32 width, u32 height)
    {
        VkRenderingAttachmentInfo colorAttachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        colorAttachmentInfo.imageView                 = colorView;
        colorAttachmentInfo.imageLayout               = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachmentInfo.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentInfo.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentInfo.clearValue.color          = clearColor;

        VkRenderingAttachmentInfo depthAttachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        depthAttachmentInfo.clearValue.depthStencil   = clearDepthStencil;
        depthAttachmentInfo.imageView                 = depthView;
        depthAttachmentInfo.imageLayout               = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachmentInfo.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentInfo.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfo renderInfo      = { VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments    = &colorAttachmentInfo;
        renderInfo.pDepthAttachment     = &depthAttachmentInfo;
        renderInfo.layerCount           = 1;
        renderInfo.renderArea.offset    = { 0, 0 };
        renderInfo.renderArea.extent    = { width, height };

        vkCmdBeginRendering(commandBuffer, &renderInfo);
    }

    void VulkanRendererPlugin::BeginRenderingLate(VkCommandBuffer commandBuffer, VkImageView colorView, VkImageView depthView,
                                                  const VkClearColorValue& clearColor, const VkClearDepthStencilValue& clearDepthStencil, u32 width, u32 height)
    {
        VkRenderingAttachmentInfo colorAttachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        colorAttachmentInfo.imageView                 = colorView;
        colorAttachmentInfo.imageLayout               = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachmentInfo.loadOp                    = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachmentInfo.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentInfo.clearValue.color          = clearColor;

        VkRenderingAttachmentInfo depthAttachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        depthAttachmentInfo.clearValue.depthStencil   = clearDepthStencil;
        depthAttachmentInfo.imageView                 = depthView;
        depthAttachmentInfo.imageLayout               = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachmentInfo.loadOp                    = VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachmentInfo.storeOp                   = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        VkRenderingInfo renderInfo      = { VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments    = &colorAttachmentInfo;
        renderInfo.pDepthAttachment     = &depthAttachmentInfo;
        renderInfo.layerCount           = 1;
        renderInfo.renderArea.offset    = { 0, 0 };
        renderInfo.renderArea.extent    = { width, height };

        vkCmdBeginRendering(commandBuffer, &renderInfo);
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

        // Keep track of the cpu time at which we start Begin()
        m_frameCpuBegin = Platform::GetAbsoluteTime() * 1000;

        // Reset our pools
        vkCmdResetQueryPool(commandBuffer, m_queryPoolTimestamps, 0, 128);
        vkCmdResetQueryPool(commandBuffer, m_queryPoolStatistics, 0, 1);

        // Write the start (top of pipeline) timestamp
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPoolTimestamps, 0);
        // Begin quering our pipeline statistics
        vkCmdBeginQuery(commandBuffer, m_queryPoolStatistics, 0, 0);

        static bool dvbCleared = false;
        if (!dvbCleared)
        {
            m_drawVisibilityBuffer.Fill(commandBuffer, 0, 4 * m_drawCount, 0);

            VkBufferMemoryBarrier fillBarrier = VkUtils::CreateBufferBarrier(m_drawVisibilityBuffer.GetHandle(), VK_ACCESS_TRANSFER_WRITE_BIT,
                                                                             VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &fillBarrier, 0,
                                 nullptr);
            dvbCleared = true;
        }

        constexpr f32 zNear = 1.f;

        auto projection = MakePerspectiveProjection(glm::radians(70.0f), static_cast<f32>(window.width) / static_cast<f32>(window.height), zNear);

        glm::mat4 projectionT = glm::transpose(projection);

        u32 depthPyramidWidth  = backendState->depthPyramid.GetWidth();
        u32 depthPyramidHeight = backendState->depthPyramid.GetHeight();

        DrawCullData cullData            = {};
        cullData.frustum[0]              = NormalizePlane(projectionT[3] + projectionT[0]);  // x + w < 0
        cullData.frustum[1]              = NormalizePlane(projectionT[3] - projectionT[0]);  // x - w > 0
        cullData.frustum[2]              = NormalizePlane(projectionT[3] + projectionT[1]);  // y + w < 0
        cullData.frustum[3]              = NormalizePlane(projectionT[3] - projectionT[1]);  // y - w > 0
        cullData.frustum[4]              = NormalizePlane(projectionT[3] - projectionT[2]);  // z - w > 0 -- reverse z
        cullData.frustum[5]              = vec4(0, 0, -1, m_drawDistance);                   // reverse z, infinite far plane
        cullData.drawCount               = m_drawCount;
        cullData.cullingEnabled          = m_cullingEnabled;
        cullData.occlusionCullingEnabled = m_occlusionCullingEnabled;
        cullData.lodEnabled              = m_lodEnabled;
        cullData.p00                     = projection[0][0];
        cullData.p11                     = projection[1][1];
        cullData.zNear                   = zNear;
        cullData.pyramidWidth            = depthPyramidWidth;
        cullData.pyramidHeight           = depthPyramidHeight;

        Globals globals    = {};
        globals.projection = projection;

        // Early cull: frustum cull & fill objects that *were* visible last frame
        {
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPoolTimestamps, 2);

            VkBufferMemoryBarrier prefillBarrier =
                VkUtils::CreateBufferBarrier(m_drawCommandCountBuffer.GetHandle(), VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &prefillBarrier, 0,
                                 nullptr);

            // Zero initialize our draw command count buffer
            m_drawCommandCountBuffer.Fill(commandBuffer, 0, 4, 0);

            VkBufferMemoryBarrier fillBarrier = VkUtils::CreateBufferBarrier(m_drawCommandCountBuffer.GetHandle(), VK_ACCESS_TRANSFER_WRITE_BIT,
                                                                             VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &fillBarrier, 0,
                                 nullptr);

            m_drawCullShader.Bind(commandBuffer);

            DescriptorInfo descriptors[] = {
                m_drawBuffer.GetHandle(),           m_meshBuffer.GetHandle(), m_drawCommandBuffer.GetHandle(), m_drawCommandCountBuffer.GetHandle(),
                m_drawVisibilityBuffer.GetHandle(),
            };
            m_drawCullShader.PushDescriptorSet(commandBuffer, descriptors);

            m_drawCullShader.PushConstants(commandBuffer, &cullData, sizeof(DrawCullData));
            m_drawCullShader.Dispatch(commandBuffer, static_cast<u32>(m_draws.Size()), 1, 1);

            VkBufferMemoryBarrier cullBarriers[] = {
                VkUtils::CreateBufferBarrier(m_drawCommandBuffer.GetHandle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT),
                VkUtils::CreateBufferBarrier(m_drawCommandCountBuffer.GetHandle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT),
            };

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr,
                                 ARRAY_SIZE(cullBarriers), cullBarriers, 0, nullptr);

            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPoolTimestamps, 3);
        }

        // Our color and depth target need to be in ATTACHMENT OPTIMAL layout before
        // we can start rendering
        VkImageMemoryBarrier renderBeginBarriers[] = {
            backendState->colorTarget.CreateBarrier(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
            backendState->depthTarget.CreateBarrier(0, 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
        };

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0,
                             0, ARRAY_SIZE(renderBeginBarriers), renderBeginBarriers);

        // First commands are to set the viewport and scissor
        vkCmdSetViewport(commandBuffer, 0, 1, &m_viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &m_scissor);

        // Early render: render objects that were visible last frame
        {
            BeginRendering(commandBuffer, backendState->colorTarget.GetView(), backendState->depthTarget.GetView(), clearColor, clearDepthStencil, window.width,
                           window.height);

            if (m_meshShadingEnabled)
            {
                m_meshletShader.Bind(commandBuffer);

                DescriptorInfo descriptors[] = {
                    m_drawCommandBuffer.GetHandle(), m_drawBuffer.GetHandle(),   m_meshletBuffer.GetHandle(),
                    m_meshletDataBuffer.GetHandle(), m_vertexBuffer.GetHandle(),
                };
                m_meshletShader.PushDescriptorSet(commandBuffer, descriptors);
                m_meshletShader.PushConstants(commandBuffer, &globals, sizeof(globals));

                vkCmdDrawMeshTasksIndirectCountEXT(commandBuffer, m_drawCommandBuffer.GetHandle(), offsetof(MeshDrawCommand, indirectMS),
                                                   m_drawCommandCountBuffer.GetHandle(), 0, static_cast<u32>(m_draws.Size()), sizeof(MeshDrawCommand));
            }
            else
            {
                m_meshShader.Bind(commandBuffer);

                DescriptorInfo descriptors[] = { m_drawCommandBuffer.GetHandle(), m_drawBuffer.GetHandle(), m_vertexBuffer.GetHandle() };
                m_meshShader.PushDescriptorSet(commandBuffer, descriptors);

                vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);

                m_meshShader.PushConstants(commandBuffer, &globals, sizeof(globals));
                vkCmdDrawIndexedIndirectCount(commandBuffer, m_drawCommandBuffer.GetHandle(), offsetof(MeshDrawCommand, indirect),
                                              m_drawCommandCountBuffer.GetHandle(), 0, static_cast<u32>(m_draws.Size()), sizeof(MeshDrawCommand));
            }

            // End our rendering
            vkCmdEndRendering(commandBuffer);
        }

        // Depth pyramid generation
        {
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPoolTimestamps, 4);

            // Wait for all depth data to be written to the depth target before we start
            // reading
            VkImageMemoryBarrier depthReadBarriers[] = {
                backendState->depthTarget.CreateBarrier(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
                backendState->depthPyramid.CreateBarrier(0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL),
            };

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0,
                                 0, 0, 0, ARRAY_SIZE(depthReadBarriers), depthReadBarriers);

            // Bind our depth reduce shader
            m_depthReduceShader.Bind(commandBuffer);

            // Build our depth pyramid
            const auto& mips = backendState->depthPyramid.GetMips();
            for (u32 i = 0; i < mips.Size(); ++i)
            {
                DescriptorInfo sourceDepth = (i == 0)
                                                 ? DescriptorInfo(m_depthSampler, backendState->depthTarget.GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                                 : DescriptorInfo(m_depthSampler, mips[i - 1], VK_IMAGE_LAYOUT_GENERAL);

                DescriptorInfo descriptors[] = { { mips[i], VK_IMAGE_LAYOUT_GENERAL }, sourceDepth };
                m_depthReduceShader.PushDescriptorSet(commandBuffer, descriptors);

                u32 levelWidth  = Max<u32>(1, depthPyramidWidth >> i);
                u32 levelHeight = Max<u32>(1, depthPyramidHeight >> i);

                DepthReduceData depthReduceData = { vec2(levelWidth, levelHeight) };

                m_depthReduceShader.PushConstants(commandBuffer, &depthReduceData, sizeof(depthReduceData));
                m_depthReduceShader.Dispatch(commandBuffer, levelWidth, levelHeight, 1);

                VkImageMemoryBarrier reduceBarrier =
                    backendState->depthPyramid.CreateBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL);

                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0,
                                     0, 0, 0, 1, &reduceBarrier);
            }

            // Wait for the depth target to be writable again
            VkImageMemoryBarrier depthWriteBarrier = backendState->depthTarget.CreateBarrier(
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_DEPENDENCY_BY_REGION_BIT,
                                 0, 0, 0, 0, 1, &depthWriteBarrier);

            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPoolTimestamps, 5);
        }

        // Late cull: frustum + occlusion cull & fill object that were *not* visible last frame
        {
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPoolTimestamps, 6);

            VkBufferMemoryBarrier prefillBarrier =
                VkUtils::CreateBufferBarrier(m_drawCommandCountBuffer.GetHandle(), VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &prefillBarrier, 0,
                                 nullptr);

            // Zero initialize our draw command count buffer
            m_drawCommandCountBuffer.Fill(commandBuffer, 0, 4, 0);

            VkBufferMemoryBarrier fillBarrier = VkUtils::CreateBufferBarrier(m_drawCommandCountBuffer.GetHandle(), VK_ACCESS_TRANSFER_WRITE_BIT,
                                                                             VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &fillBarrier, 0,
                                 nullptr);

            m_drawCullLateShader.Bind(commandBuffer);

            DescriptorInfo descriptors[] = {
                m_drawBuffer.GetHandle(),           m_meshBuffer.GetHandle(),
                m_drawCommandBuffer.GetHandle(),    m_drawCommandCountBuffer.GetHandle(),
                m_drawVisibilityBuffer.GetHandle(), DescriptorInfo(m_depthSampler, backendState->depthPyramid.GetView(), VK_IMAGE_LAYOUT_GENERAL)
            };
            m_drawCullLateShader.PushDescriptorSet(commandBuffer, descriptors);

            m_drawCullLateShader.PushConstants(commandBuffer, &cullData, sizeof(DrawCullData));
            m_drawCullLateShader.Dispatch(commandBuffer, static_cast<u32>(m_draws.Size()), 1, 1);

            VkBufferMemoryBarrier cullBarriers[] = {
                VkUtils::CreateBufferBarrier(m_drawCommandBuffer.GetHandle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT),
                VkUtils::CreateBufferBarrier(m_drawCommandCountBuffer.GetHandle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT),
            };

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr,
                                 ARRAY_SIZE(cullBarriers), cullBarriers, 0, nullptr);

            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPoolTimestamps, 7);
        }

        // Late render: render objects that are visible this frame but weren't drawn in the early pass
        {
            BeginRenderingLate(commandBuffer, backendState->colorTarget.GetView(), backendState->depthTarget.GetView(), clearColor, clearDepthStencil,
                               window.width, window.height);

            if (m_meshShadingEnabled)
            {
                m_meshletShader.Bind(commandBuffer);

                DescriptorInfo descriptors[] = {
                    m_drawCommandBuffer.GetHandle(), m_drawBuffer.GetHandle(),   m_meshletBuffer.GetHandle(),
                    m_meshletDataBuffer.GetHandle(), m_vertexBuffer.GetHandle(),
                };
                m_meshletShader.PushDescriptorSet(commandBuffer, descriptors);
                m_meshletShader.PushConstants(commandBuffer, &globals, sizeof(globals));

                vkCmdDrawMeshTasksIndirectCountEXT(commandBuffer, m_drawCommandBuffer.GetHandle(), offsetof(MeshDrawCommand, indirectMS),
                                                   m_drawCommandCountBuffer.GetHandle(), 0, static_cast<u32>(m_draws.Size()), sizeof(MeshDrawCommand));
            }
            else
            {
                m_meshShader.Bind(commandBuffer);

                DescriptorInfo descriptors[] = { m_drawCommandBuffer.GetHandle(), m_drawBuffer.GetHandle(), m_vertexBuffer.GetHandle() };
                m_meshShader.PushDescriptorSet(commandBuffer, descriptors);

                vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);

                m_meshShader.PushConstants(commandBuffer, &globals, sizeof(globals));
                vkCmdDrawIndexedIndirectCount(commandBuffer, m_drawCommandBuffer.GetHandle(), offsetof(MeshDrawCommand, indirect),
                                              m_drawCommandCountBuffer.GetHandle(), 0, static_cast<u32>(m_draws.Size()), sizeof(MeshDrawCommand));
            }

            vkCmdEndRendering(commandBuffer);
        }

        return true;
    }

    bool VulkanRendererPlugin::End(Window& window)
    {
        auto backendState   = window.rendererState->backendState;
        auto commandBuffer  = backendState->GetCommandBuffer();
        auto swapchainImage = backendState->swapchain.GetImage(backendState->imageIndex);

        // End our query for pipeline statistics
        vkCmdEndQuery(commandBuffer, m_queryPoolStatistics, 0);

        // Setup some copy barriers
        VkImageMemoryBarrier copyBarriers[] = {
            backendState->colorTarget.CreateBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
            VkUtils::CreateImageBarrier(swapchainImage, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_IMAGE_ASPECT_COLOR_BIT),
        };

        // Wait for color target to be in TRANSFER_SRC_OPTIMAL and wait for swapchain
        // image to be in TRANSFER_DST_OPTIMAL
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0,
                             0, ARRAY_SIZE(copyBarriers), copyBarriers);

        if (m_debugPyramid)
        {
            u32 depthPyramidWidth  = backendState->depthPyramid.GetWidth();
            u32 depthPyramidHeight = backendState->depthPyramid.GetHeight();

            VkImageBlit blitRegion               = {};
            blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.srcSubresource.mipLevel   = m_debugPyramidLevel;
            blitRegion.srcSubresource.layerCount = 1;
            blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.dstSubresource.layerCount = 1;

            blitRegion.srcOffsets[0] = { 0, 0, 0 };
            blitRegion.srcOffsets[1] = { Max<i32>(1, depthPyramidWidth >> m_debugPyramidLevel), Max<i32>(1, depthPyramidHeight >> m_debugPyramidLevel), 1 };
            blitRegion.dstOffsets[0] = { 0, 0, 0 };
            blitRegion.dstOffsets[1] = { window.width, window.height, 1 };

            vkCmdBlitImage(commandBuffer, backendState->depthPyramid.GetImage(), VK_IMAGE_LAYOUT_GENERAL, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blitRegion, VK_FILTER_NEAREST);
        }
        else
        {
            // Copy the contents of our color target to the current swapchain image
            backendState->colorTarget.CopyTo(commandBuffer, swapchainImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        }

        // Setup a present barrier
        VkImageMemoryBarrier presentBarrier = VkUtils::CreateImageBarrier(swapchainImage, VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);

        // Wait for the swapchain image to go from TRANSFER_DST_OPTIMAL to
        // PRESENT_SRC_KHR
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1,
                             &presentBarrier);

        // Keep track of the renderer End() timestamp
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPoolTimestamps, 1);

        // Finally we end our command buffer
        auto result = vkEndCommandBuffer(commandBuffer);
        if (!VkUtils::IsSuccess(result))
        {
            ERROR_LOG("vkEndCommandBuffer failed with error: '{}'.", VkUtils::ResultString(result));
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
        if (!VkUtils::IsSuccess(result))
        {
            ERROR_LOG("vkQueueSubmit failed with error: '{}'.", VkUtils::ResultString(result));
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

        u64 timestampResults[8] = {};
        VK_CHECK(vkGetQueryPoolResults(device, m_queryPoolTimestamps, 0, ARRAY_SIZE(timestampResults), sizeof(timestampResults), timestampResults,
                                       sizeof(timestampResults[0]), VK_QUERY_RESULT_64_BIT));

        u32 statResults[1] = {};
        VK_CHECK(vkGetQueryPoolResults(device, m_queryPoolStatistics, 0, 1, sizeof(statResults), statResults, sizeof(statResults[0]), 0));

        auto props = m_context.device.GetProperties();

        f64 frameGpuBegin   = static_cast<f64>(timestampResults[0]) * props.limits.timestampPeriod * 1e-6;
        f64 frameGpuEnd     = static_cast<f64>(timestampResults[1]) * props.limits.timestampPeriod * 1e-6;
        f64 cullGpuTime     = static_cast<f64>(timestampResults[3] - timestampResults[2]) * props.limits.timestampPeriod * 1e-6;
        f64 pyramidGpuTime  = static_cast<f64>(timestampResults[5] - timestampResults[4]) * props.limits.timestampPeriod * 1e-6;
        f64 cullLateGpuTime = static_cast<f64>(timestampResults[7] - timestampResults[6]) * props.limits.timestampPeriod * 1e-6;

        f64 frameCpuEnd = Platform::GetAbsoluteTime() * 1000;

        m_frameCpuAvg = m_frameCpuAvg * 0.95 + (frameCpuEnd - m_frameCpuBegin) * 0.05;
        m_frameGpuAvg = m_frameGpuAvg * 0.95 + (frameGpuEnd - frameGpuBegin) * 0.05;

        f64 triangleCount = static_cast<f64>(statResults[0]);

        f64 trianglesPerSecond = triangleCount / (m_frameGpuAvg * 1e-3);
        f64 drawsPerSecond     = static_cast<f64>(m_drawCount) / (m_frameGpuAvg * 1e-3);

        auto meshShadingEnabledAndSupported = m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING) && m_meshShadingEnabled;

        titleText.Clear();
        titleText.Format(
            "Mesh Shading: {}; Cull: {}; Occlusion: {}; LOD: {}; cpu: {:.2f} ms; gpu: {:.2f} ms; (cull {:.2f} ms; pyramid {:.2f} ms; cull late: {:.2f} "
            "ms); triangles {:.1f}M; {:.1f}B tri/sec; {:.1f}M draws/sec;",
            meshShadingEnabledAndSupported ? "ON" : "OFF", m_cullingEnabled ? "ON" : "OFF", m_occlusionCullingEnabled ? "ON" : "OFF",
            m_lodEnabled ? "ON" : "OFF", m_frameCpuAvg, m_frameGpuAvg, cullGpuTime, pyramidGpuTime, cullLateGpuTime, triangleCount * 1e-6,
            trianglesPerSecond * 1e-9, drawsPerSecond * 1e-6);

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
        createInfo.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        if (!backend->depthTarget.Create(createInfo))
        {
            ERROR_LOG("Failed to create Depth target for window: '{}'.", window.name);
            return false;
        }

        // Using the previous power of 2 ensures our reductions are at most 2x2 which
        // makes them conservative
        u32 depthPyramidWidth  = FindPreviousPow2(window.width);
        u32 depthPyramidHeight = FindPreviousPow2(window.height);

        // Create the depth pyramid for this window
        createInfo.name      = "DEPTH_PYRAMID";
        createInfo.width     = depthPyramidWidth;
        createInfo.height    = depthPyramidHeight;
        createInfo.format    = VK_FORMAT_R32_SFLOAT;
        createInfo.usage     = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        createInfo.mipLevels = VkUtils::CalculateImageMiplevels(depthPyramidWidth, depthPyramidHeight);

        if (!backend->depthPyramid.Create(createInfo))
        {
            ERROR_LOG("Failed to create Depth pyramid for window: '{}'.", window.name);
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

        // Create our depth sampler
        m_depthSampler = VkUtils::CreateSampler(&m_context, VK_SAMPLER_REDUCTION_MODE_MIN);
        if (!m_depthSampler)
        {
            ERROR_LOG("Failed to create depth sampler.");
            return false;
        }

        {
            // TODO: This should not be here! It does not depend on the window we just
            // need access to the swapchain

            if (!m_drawCullShaderModule.Create(&m_context, "drawcull.comp"))
            {
                ERROR_LOG("Failed to create drawcull ShaderModule");
                return false;
            }

            if (!m_drawCullLateShaderModule.Create(&m_context, "drawcull_late.comp"))
            {
                ERROR_LOG("Failed to create drawcull_late ShaderModule");
                return false;
            }

            if (!m_depthReduceShaderModule.Create(&m_context, "depth_reduce.comp"))
            {
                ERROR_LOG("Failed to create depth_reduce ShaderModule");
                return false;
            }

            if (!m_meshShaderModule.Create(&m_context, "mesh.vert"))
            {
                ERROR_LOG("Failed to create mesh.vert ShaderModule.");
                return false;
            }

            if (!m_fragmentShaderModule.Create(&m_context, "mesh.frag"))
            {
                ERROR_LOG("Failed to create mesh.frag ShaderModule.");
                return false;
            }

            VulkanShaderCreateInfo createInfo;
            createInfo.context           = &m_context;
            createInfo.name              = "DRAW_CULL_SHADER";
            createInfo.bindPoint         = VK_PIPELINE_BIND_POINT_COMPUTE;
            createInfo.pushConstantsSize = sizeof(DrawCullData);
            createInfo.cache             = VK_NULL_HANDLE;
            createInfo.swapchain         = &backend->swapchain;
            createInfo.modules           = { &m_drawCullShaderModule };

            if (!m_drawCullShader.Create(createInfo))
            {
                ERROR_LOG("Failed to create DrawCull shader.");
                return false;
            }

            createInfo.name    = "DRAW_CULL_LATE_SHADER";
            createInfo.modules = { &m_drawCullLateShaderModule };

            if (!m_drawCullLateShader.Create(createInfo))
            {
                ERROR_LOG("Failed to create DrawCullLate shader.");
                return false;
            }

            createInfo.name              = "DEPTH_REDUCE_SHADER";
            createInfo.pushConstantsSize = sizeof(DepthReduceData);
            createInfo.modules           = { &m_depthReduceShaderModule };

            if (!m_depthReduceShader.Create(createInfo))
            {
                ERROR_LOG("Failed to create DepthReduce shader.");
                return false;
            }

            createInfo.name              = "MESH_SHADER";
            createInfo.bindPoint         = VK_PIPELINE_BIND_POINT_GRAPHICS;
            createInfo.pushConstantsSize = sizeof(Globals);
            createInfo.modules           = { &m_meshShaderModule, &m_fragmentShaderModule };

            if (!m_meshShader.Create(createInfo))
            {
                ERROR_LOG("Failed to create Mesh shader.");
                return false;
            }

            if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
            {
                if (!m_meshletShaderModule.Create(&m_context, "meshlet.mesh"))
                {
                    ERROR_LOG("Failed to create meshlet.mesh ShaderModule.");
                    return false;
                }

                if (!m_meshletTaskShaderModule.Create(&m_context, "meshlet.task"))
                {
                    ERROR_LOG("Failed to create meshlet.task ShaderModule.");
                    return false;
                }

                // For the meshlet shader only the name and and modules change
                createInfo.name    = "MESHLET_SHADER";
                createInfo.modules = { &m_meshletTaskShaderModule, &m_meshletShaderModule, &m_fragmentShaderModule };

                if (!m_meshletShader.Create(createInfo))
                {
                    ERROR_LOG("Failed to create Meshlet shader.");
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

            u32 depthPyramidWidth  = FindPreviousPow2(window.width);
            u32 depthPyramidHeight = FindPreviousPow2(window.height);

            if (!backend->depthPyramid.Resize(depthPyramidWidth, depthPyramidHeight, VkUtils::CalculateImageMiplevels(depthPyramidWidth, depthPyramidHeight)))
            {
                ERROR_LOG("Failed to resize Depth pyramid.");
                return false;
            }

            // TODO: For now we assume scissor and viewport are always the size of the
            // full window
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
                vkDestroySampler(m_context.device.GetLogical(), m_depthSampler, m_context.allocator);

                // TODO: This should not be here! It does not depend on the window we just
                // need access to the swapchain
                m_meshShader.Destroy();
                m_meshShaderModule.Destroy();
                m_fragmentShaderModule.Destroy();

                m_drawCullShader.Destroy();
                m_drawCullShaderModule.Destroy();

                m_drawCullLateShader.Destroy();
                m_drawCullLateShaderModule.Destroy();

                m_depthReduceShader.Destroy();
                m_depthReduceShaderModule.Destroy();

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
            // Also destroy our depth pyramid
            backend->depthPyramid.Destroy();

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

        if (!m_meshBuffer.Upload(commandBuffer, commandPool, geometry.meshes.GetData(), sizeof(Mesh) * geometry.meshes.Size()))
        {
            ERROR_LOG("Failed to upload meshes.");
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

        m_drawCount = 1000000;
        m_draws.Resize(m_drawCount);

        for (auto& draw : m_draws)
        {
            u64 meshIndex    = Random.Generate(static_cast<u64>(0), geometry.meshes.Size() - 1);
            const auto& mesh = geometry.meshes[meshIndex];

            draw.position.x = Random.Generate(-m_sceneRadius, m_sceneRadius);
            draw.position.y = Random.Generate(-m_sceneRadius, m_sceneRadius);
            draw.position.z = Random.Generate(-m_sceneRadius, m_sceneRadius);
            draw.scale      = Random.Generate(2.f, 4.f);

            vec3 axis        = vec3(Random.Generate(-1.0f, 1.0f), Random.Generate(-1.0f, 1.0f), Random.Generate(-1.0f, 1.0f));
            f32 angle        = glm::radians(Random.Generate(0.f, 90.0f));
            draw.orientation = glm::rotate(glm::quat(1, 0, 0, 0), angle, axis);

            draw.meshIndex    = static_cast<u32>(meshIndex);
            draw.vertexOffset = mesh.vertexOffset;
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

    bool VulkanRendererPlugin::SupportsFeature(RendererSupportFlag feature) const
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
