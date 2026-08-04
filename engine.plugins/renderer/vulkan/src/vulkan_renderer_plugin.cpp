
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
#include <shaderc/shaderc.h>
#include <system/system_manager.h>
#include <time/scoped_timer.h>

#include "assets/types/texture_types.h"
#include "containers/dynamic_array.h"
#include "defines.h"
#include "input/keys.h"
#include "platform/vulkan_platform.h"
#include "renderer/mesh.h"
#include "renderer/vertex.h"
#include "time/clock.h"
#include "vulkan_allocator.h"
#include "vulkan_context.h"
#include "vulkan_debugger.h"
#include "vulkan_device.h"
#include "vulkan_instance.h"
#include "vulkan_ray_tracing.h"
#include "vulkan_shader.h"
#include "vulkan_swapchain.h"
#include "vulkan_texture.h"
#include "vulkan_types.h"
#include "vulkan_utils.h"

#define CREATE_RESOURCE(method, fail_msg)             \
    if (!method)                                      \
    {                                                 \
        ERROR_LOG("Failed to create: {}.", fail_msg); \
        return false;                                 \
    }

#define CHECK_RESOURCE(res, fail_msg)                 \
    if (res == nullptr)                               \
    {                                                 \
        ERROR_LOG("Failed to create: {}.", fail_msg); \
        return false;                                 \
    }

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

        // Create our device
        if (!m_context.device.Create(&m_context))
        {
            ERROR_LOG("Failed to create Vulkan Device.");
            return false;
        }

        auto device = m_context.device.GetLogical();

        volkLoadDevice(device);

        // Create command pool and buffer for non-window specific use
        m_context.commandPool = VkUtils::CreateCommandPool(&m_context, "VULKAN_COMMAND_POOL");
        if (!m_context.commandPool)
        {
            ERROR_LOG("Failed to create CommandPool");
            return false;
        }

        m_context.commandBuffer = VkUtils::AllocateCommandBuffer(&m_context, "VULKAN_COMMAND_BUFFER", m_context.commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        if (!m_context.commandBuffer)
        {
            ERROR_LOG("Failed to allocate CommandBuffer.");
            return false;
        }

        // Initialize shaderc
        m_context.shaderCompiler = shaderc_compiler_initialize();
        if (!m_context.shaderCompiler)
        {
            ERROR_LOG("Failed to initialize shaderc compiler.");
            return false;
        }

        // Enable mesh shading and ray tracing if supported
        m_meshShadingEnabled = m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING);
        m_rayTracingEnabled  = m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_RAY_TRACING);

        // Create our query pool for timestamps
        m_queryPoolTimestamps = VkUtils::CreateQueryPool(&m_context, 128, VK_QUERY_TYPE_TIMESTAMP);
        // And for pipeline statistics
        m_queryPoolStatistics = VkUtils::CreateQueryPool(&m_context, 3, VK_QUERY_TYPE_PIPELINE_STATISTICS);

        u32 rayTracingBufferFlags = 0;
        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_RAY_TRACING))
        {
            rayTracingBufferFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        }

        // Create our buffers
        INFO_LOG("Creating buffers...");

        if (!m_context.stagingBuffer.Create(&m_context, "STAGING", MebiBytes(128), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            ERROR_LOG("Failed to create staging buffer.");
            return false;
        }

        if (!m_vertexBuffer.Create(&m_context, "VERTEX", MebiBytes(64), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | rayTracingBufferFlags,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create vertex buffer.");
            return false;
        }

        if (!m_indexBuffer.Create(&m_context, "INDEX", MebiBytes(64), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | rayTracingBufferFlags,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create index buffer.");
            return false;
        }

        if (!m_meshBuffer.Create(&m_context, "MESH", MebiBytes(32), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create mesh buffer.");
            return false;
        }

        if (!m_drawBuffer.Create(&m_context, "DRAW", MebiBytes(64), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create draw buffer.");
            return false;
        }

        if (!m_drawCommandBuffer.Create(&m_context, "DRAW_COMMAND", TASK_WGLIMIT * sizeof(MeshTaskCommand),
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create draw command buffer.");
            return false;
        }

        if (!m_drawCommandCountBuffer.Create(&m_context, "DRAW_COMMAND_COUNT", 16,
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
            if (!m_meshletBuffer.Create(&m_context, "MESHLET", MebiBytes(64), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            {
                ERROR_LOG("Failed to create mesh buffer.");
                return false;
            }

            if (!m_meshletDataBuffer.Create(&m_context, "MESHLET_DATA", MebiBytes(64), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            {
                ERROR_LOG("Failed to create mesh buffer.");
                return false;
            }

            if (!m_clusterIndexBuffer.Create(&m_context, "CLUSTER_INDEX_BUFFER", CLUSTER_LIMIT * sizeof(u32), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            {
                ERROR_LOG("Failed to create cluster index buffer.");
                return false;
            }

            if (!m_clusterCountBuffer.Create(&m_context, "CLUSTER_COUNT_BUFFER", 16,
                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            {
                ERROR_LOG("Failed to create cluster count buffer.");
                return false;
            }
        }

        INFO_LOG("Creating descriptors...");

        m_textureDescriptorSetLayout = VkUtils::CreateDescriptorSetLayout(
            &m_context, "TEXTURE_DESCRIPTOR_SET_LAYOUT", 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_DESCRIPTROS, VK_SHADER_STAGE_FRAGMENT_BIT,
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);

        CHECK_RESOURCE(m_textureDescriptorSetLayout, "Texture Descriptor Set Layout");

        m_textureDescriptorPool = VkUtils::CreateDescriptorPool(&m_context, "TEXTURE_DESCRIPTOR_POOL", VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_ACTIVE_DESCRIPTORS,
                                                                VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
        CHECK_RESOURCE(m_textureDescriptorPool, "Texture Descriptor Pool");

        m_textureDescriptorSet = VkUtils::CreateDescriptorSet(&m_context, "TEXTURE_DESCRIPTOR_SET", MAX_ACTIVE_DESCRIPTORS, m_textureDescriptorPool, m_textureDescriptorSetLayout);
        CHECK_RESOURCE(m_textureDescriptorSet, "Texture Descriptor Set");

        INFO_LOG("Creating Samplers...");

        // Create our texture sampler
        m_textureSampler = VkUtils::CreateSampler(&m_context, "TEXTURE_SAMPLER", VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
        CHECK_RESOURCE(m_textureSampler, "Texture Sampler");

        // Create our read sampler
        m_readSampler = VkUtils::CreateSampler(&m_context, "READ_SAMPLER", VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        CHECK_RESOURCE(m_readSampler, "Read Sampler");

        // Create our depth sampler
        m_depthSampler = VkUtils::CreateSampler(&m_context, "DEPTH_SAMPLER", VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                VK_SAMPLER_REDUCTION_MODE_MIN);
        CHECK_RESOURCE(m_depthSampler, "Depth Sampler");

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
                        WARN_LOG("Mesh shading is not supported by the current GPU: '{}'.", m_context.device.GetProperties().deviceName);
                    }
                    break;
                case C3D::KeyR:
                    if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_RAY_TRACING))
                    {
                        m_rayTracingEnabled ^= true;
                    }
                    else
                    {
                        WARN_LOG("Ray tracing is not supported by the current GPU: '{}'.", m_context.device.GetProperties().deviceName);
                    }
                    break;
                case C3D::KeyS:
                    if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_RAY_TRACING))
                    {
                        m_shadingEnabled ^= true;
                    }
                    else
                    {
                        WARN_LOG("Ray Tracing is not supported by the current GPU: '{}'.", m_context.device.GetProperties().deviceName);
                    }
                    break;
                case C3D::KeyC:
                    m_cullingEnabled ^= true;
                    break;
                case C3D::KeyO:
                    m_occlusionCullingEnabled ^= true;
                    break;
                case C3D::KeyK:
                    m_clusterOcclusionCullingEnabled ^= true;
                    break;
                case C3D::KeyL:
                    m_debugLods ^= true;
                    m_debugLodStep = 0;
                    break;
                case C3D::KeyT:
                    m_taskShadingEnabled ^= true;
                    break;
            }

            return true;
        });
        Event.Register(EventCodeDebug1, [this](const u16 code, void* sender, const EventContext& context) {
            if (m_debugLods)
            {
                m_debugLodStep = context.data.u32[0];
            }

            return true;
        });

        // Setup a default camera
        m_camera.position    = vec3(0);
        m_camera.orientation = quat(1, 0, 0, 0);
        m_camera.fovY        = glm::radians(70.0f);

        INFO_LOG("Initialized successfully.");
        return true;
    }

    void VulkanRendererPlugin::OnShutdown()
    {
        INFO_LOG("Shutting down.");

        auto device = m_context.device.GetLogical();

        Event.UnregisterAll(EventCodeDebug0);
        Event.UnregisterAll(EventCodeDebug1);

        m_draws.Destroy();

        if (m_context.shaderCompiler)
        {
            shaderc_compiler_release(m_context.shaderCompiler);
            m_context.shaderCompiler = nullptr;
        }

        INFO_LOG("Destroying Vulkan Textures.");
        for (auto& texture : m_textures)
        {
            texture.Destroy();
        }
        m_textures.Destroy();

        INFO_LOG("Destroying Texture descriptor pool and layout");
        if (m_textureDescriptorSetLayout)
        {
            vkDestroyDescriptorSetLayout(device, m_textureDescriptorSetLayout, m_context.allocator);
        }
        if (m_textureDescriptorPool)
        {
            vkDestroyDescriptorPool(device, m_textureDescriptorPool, m_context.allocator);
        }

        INFO_LOG("Destroying Vulkan buffers.");

        m_context.stagingBuffer.Destroy();
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
            m_meshletVisibilityBuffer.Destroy();
            m_clusterIndexBuffer.Destroy();
            m_clusterCountBuffer.Destroy();
        }

        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_RAY_TRACING))
        {
            INFO_LOG("Destroying Ray Tracing buffers and structures.");

            for (auto b : m_blas)
            {
                vkDestroyAccelerationStructureKHR(device, b, m_context.allocator);
            }
            m_blas.Destroy();

            vkDestroyAccelerationStructureKHR(device, m_tlas, m_context.allocator);

            m_tlasBuffer.Destroy();
            m_blasBuffer.Destroy();
        }

        INFO_LOG("Destroying Vulkan Shader Modules.");
        for (auto& m : m_shaderModules)
        {
            m.Destroy();
        }
        m_shaderModules.Destroy();

        INFO_LOG("Destroying Vulkan Shaders.");

        m_meshShader.Destroy();
        m_meshPostShader.Destroy();

        m_drawCullShader.Destroy();
        m_taskCullShader.Destroy();
        m_clusterCullShader.Destroy();

        m_drawCullLateShader.Destroy();
        m_taskCullLateShader.Destroy();
        m_clusterCullLateShader.Destroy();

        m_depthReduceShader.Destroy();
        m_taskSubmitShader.Destroy();

        m_clusterSubmitShader.Destroy();

        m_blitShader.Destroy();
        m_shadeShader.Destroy();

        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
        {
            m_taskMeshletShader.Destroy();
            m_taskMeshletLateShader.Destroy();
            m_taskMeshletPostShader.Destroy();
            m_clusterMeshletShader.Destroy();
            m_clusterPostMeshletShader.Destroy();
        }

        INFO_LOG("Destroying Vulkan Samplers.");
        vkDestroySampler(device, m_textureSampler, m_context.allocator);
        vkDestroySampler(device, m_readSampler, m_context.allocator);
        vkDestroySampler(device, m_depthSampler, m_context.allocator);

        INFO_LOG("Destroying Query pools");
        vkDestroyQueryPool(device, m_queryPoolTimestamps, m_context.allocator);
        vkDestroyQueryPool(device, m_queryPoolStatistics, m_context.allocator);

        vkDestroyCommandPool(device, m_context.commandPool, m_context.allocator);

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

    bool VulkanRendererPlugin::OnRun(const Geometry& geometry)
    {
        // Create all required ShaderModules
        DynamicArray<const char*> shader_module_names = { "draw_cull.comp", "cluster_cull.comp", "depth_reduce.comp",   "mesh.vert",
                                                          "mesh.frag",      "task_submit.comp",  "cluster_submit.comp", "blit.comp" };

        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
        {
            shader_module_names.PushBack("meshlet.mesh");
            shader_module_names.PushBack("meshlet.task");
        }
        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_RAY_TRACING))
        {
            shader_module_names.PushBack("shade.comp");
        }

        INFO_LOG("Creating Vulkan Shader Modules.");

        m_shaderModules.Create();
        for (auto name : shader_module_names)
        {
            m_shaderModules.Set(name, {});
            if (!m_shaderModules[name].Create(&m_context, name))
            {
                ERROR_LOG("Failed to create: '{}' ShaderModule.", name);
                return false;
            }
        }

        INFO_LOG("Creating Vulkan Shaders.");

        VulkanShaderCreateInfo createInfo;
        createInfo.context           = &m_context;
        createInfo.name              = "DRAW_CULL_SHADER";
        createInfo.bindPoint         = VK_PIPELINE_BIND_POINT_COMPUTE;
        createInfo.pushConstantsSize = sizeof(CullData);
        createInfo.cache             = VK_NULL_HANDLE;
        createInfo.modules           = { &m_shaderModules["draw_cull.comp"] };
        createInfo.constants         = { /* late = */ false, /* task = */ false };

        CREATE_RESOURCE(m_drawCullShader.Create(createInfo), "DrawCull Shader");

        createInfo.name              = "TASK_CULL_SHADER";
        createInfo.pushConstantsSize = sizeof(CullData);
        createInfo.modules           = { &m_shaderModules["draw_cull.comp"] };
        createInfo.constants         = { /* late = */ false, /* task = */ true };

        CREATE_RESOURCE(m_taskCullShader.Create(createInfo), "TaskCull Shader");

        createInfo.name              = "CLUSTER_CULL_SHADER";
        createInfo.pushConstantsSize = sizeof(CullData);
        createInfo.modules           = { &m_shaderModules["cluster_cull.comp"] };
        createInfo.constants         = { /* late = */ false };

        CREATE_RESOURCE(m_clusterCullShader.Create(createInfo), "ClusterCull Shader");

        createInfo.name              = "DRAW_CULL_LATE_SHADER";
        createInfo.pushConstantsSize = sizeof(CullData);
        createInfo.modules           = { &m_shaderModules["draw_cull.comp"] };
        createInfo.constants         = { /* late = */ true, /* task = */ false };

        CREATE_RESOURCE(m_drawCullLateShader.Create(createInfo), "DrawCullLate Shader");

        createInfo.name              = "TASK_CULL_LATE_SHADER";
        createInfo.pushConstantsSize = sizeof(CullData);
        createInfo.modules           = { &m_shaderModules["draw_cull.comp"] };
        createInfo.constants         = { /* late = */ true, /* task = */ true };

        CREATE_RESOURCE(m_taskCullLateShader.Create(createInfo), "TaskCullLate Shader");

        createInfo.name              = "CLUSTER_CULL_LATE_SHADER";
        createInfo.pushConstantsSize = sizeof(CullData);
        createInfo.modules           = { &m_shaderModules["cluster_cull.comp"] };
        createInfo.constants         = { /* late = */ true };

        CREATE_RESOURCE(m_clusterCullLateShader.Create(createInfo), "ClusterCullLate Shader");

        createInfo.name              = "DEPTH_REDUCE_SHADER";
        createInfo.pushConstantsSize = sizeof(DepthReduceData);
        createInfo.modules           = { &m_shaderModules["depth_reduce.comp"] };
        createInfo.constants         = {};

        CREATE_RESOURCE(m_depthReduceShader.Create(createInfo), "DepthReduce Shader");

        createInfo.name              = "TASK_SUBMIT_SHADER";
        createInfo.pushConstantsSize = 0;
        createInfo.modules           = { &m_shaderModules["task_submit.comp"] };

        CREATE_RESOURCE(m_taskSubmitShader.Create(createInfo), "TaskSubmit Shader");

        createInfo.name    = "CLUSTER_SUBMIT_SHADER";
        createInfo.modules = { &m_shaderModules["cluster_submit.comp"] };

        CREATE_RESOURCE(m_clusterSubmitShader.Create(createInfo), "ClusterSubmit Shader");

        createInfo.name              = "MESH_SHADER";
        createInfo.bindPoint         = VK_PIPELINE_BIND_POINT_GRAPHICS;
        createInfo.pushConstantsSize = sizeof(Globals);
        createInfo.modules           = { &m_shaderModules["mesh.vert"], &m_shaderModules["mesh.frag"] };
        createInfo.setArrayLayout    = m_textureDescriptorSetLayout;

        CREATE_RESOURCE(m_meshShader.Create(createInfo), "Mesh Shader");

        createInfo.name      = "MESH_POST_SHADER";
        createInfo.constants = { /* LATE= */ false, /* TASK= */ false, /* POST= */ 1 };

        CREATE_RESOURCE(m_meshPostShader.Create(createInfo), "MeshPost Shader");

        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
        {
            // For the meshlet shader only the name and and modules change
            createInfo.name              = "MESHLET_SHADER";
            createInfo.pushConstantsSize = sizeof(Globals);
            createInfo.constants         = { /* late = */ false, /* task = */ true };
            createInfo.modules           = { &m_shaderModules["meshlet.task"], &m_shaderModules["meshlet.mesh"], &m_shaderModules["mesh.frag"] };

            CREATE_RESOURCE(m_taskMeshletShader.Create(createInfo), "Meshlet Shader");

            createInfo.name              = "MESHLET_LATE_SHADER";
            createInfo.pushConstantsSize = sizeof(Globals);
            createInfo.constants         = { /* late = */ true, /* task = */ true };
            createInfo.modules           = { &m_shaderModules["meshlet.task"], &m_shaderModules["meshlet.mesh"], &m_shaderModules["mesh.frag"] };

            CREATE_RESOURCE(m_taskMeshletLateShader.Create(createInfo), "MeshletLate Shader");

            createInfo.name              = "MESHLET_POST_SHADER";
            createInfo.pushConstantsSize = sizeof(Globals);
            createInfo.constants         = { /* late = */ true, /* task = */ true, /* post = */ 1 };

            CREATE_RESOURCE(m_taskMeshletPostShader.Create(createInfo), "MeshletPost Shader");

            createInfo.name              = "CLUSTER_MESHLET_SHADER";
            createInfo.pushConstantsSize = sizeof(Globals);
            createInfo.constants         = { /* late = */ false, /* task = */ false };
            createInfo.modules           = { &m_shaderModules["meshlet.mesh"], &m_shaderModules["mesh.frag"] };

            CREATE_RESOURCE(m_clusterMeshletShader.Create(createInfo), "MeshletCluster Shader");

            createInfo.name      = "CLUSTER_POST_MESHLET_SHADER";
            createInfo.constants = { /* late = */ false, /* task = */ false, /* post = */ 1 };

            CREATE_RESOURCE(m_clusterPostMeshletShader.Create(createInfo), "MeshletClusterPost Shader");
        }

        createInfo.name              = "BLIT_SHADER";
        createInfo.bindPoint         = VK_PIPELINE_BIND_POINT_COMPUTE;
        createInfo.modules           = { &m_shaderModules["blit.comp"] };
        createInfo.pushConstantsSize = sizeof(vec4);

        CREATE_RESOURCE(m_blitShader.Create(createInfo), "Blit Shader");

        createInfo.name              = "SHADE_SHADER";
        createInfo.modules           = { &m_shaderModules["shade.comp"] };
        createInfo.pushConstantsSize = sizeof(ShadeData);

        CREATE_RESOURCE(m_shadeShader.Create(createInfo), "Shade Shader");

        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_RAY_TRACING))
        {
            if (!VulkanRayTracing::BuildBLAS(&m_context, geometry.meshes, m_vertexBuffer, m_indexBuffer, m_blas, m_blasBuffer))
            {
                ERROR_LOG("Failed to create BLAS.");
                return false;
            }

            if (!VulkanRayTracing::BuildTLAS(&m_context, m_draws, m_blas, m_tlas, m_tlasBuffer))
            {
                ERROR_LOG("Failed to create TLAS");
                return false;
            }
        }

        return true;
    }

    mat4 MakePerspectiveProjection(f32 fovY, f32 aspect, f32 zNear)
    {
        f32 f = 1.0f / Tan(fovY / 2.0f);
        return mat4(f / aspect, 0.0f, 0.0f, 0.0f, 0.0f, f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, zNear, 0.0f);
    }

    void VulkanRendererPlugin::BeginRendering(VkCommandBuffer commandBuffer, VulkanTexture* gBufferTargets, const VulkanTexture& depthTarget, const VkClearColorValue& clearColor,
                                              const VkClearDepthStencilValue& clearDepthStencil, u32 width, u32 height, bool late) const
    {
        VkRenderingAttachmentInfo gBufferAttachmentInfos[GBUFFER_COUNT] = {};
        for (u32 i = 0; i < GBUFFER_COUNT; ++i)
        {
            gBufferAttachmentInfos[i].sType            = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            gBufferAttachmentInfos[i].imageView        = gBufferTargets[i].GetView();
            gBufferAttachmentInfos[i].imageLayout      = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
            gBufferAttachmentInfos[i].loadOp           = late ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
            gBufferAttachmentInfos[i].storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
            gBufferAttachmentInfos[i].clearValue.color = clearColor;
        }

        VkRenderingAttachmentInfo depthAttachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        depthAttachmentInfo.imageView                 = depthTarget.GetView();
        depthAttachmentInfo.imageLayout               = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        depthAttachmentInfo.loadOp                    = late ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentInfo.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachmentInfo.clearValue.depthStencil   = clearDepthStencil;

        VkRenderingInfo renderInfo      = { VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderInfo.colorAttachmentCount = GBUFFER_COUNT;
        renderInfo.pColorAttachments    = gBufferAttachmentInfos;
        renderInfo.pDepthAttachment     = &depthAttachmentInfo;
        renderInfo.layerCount           = 1;
        renderInfo.renderArea.offset    = { 0, 0 };
        renderInfo.renderArea.extent    = { width, height };

        vkCmdBeginRendering(commandBuffer, &renderInfo);
    }

    void VulkanRendererPlugin::CullStep(VkCommandBuffer commandBuffer, const VulkanShader& shader, VulkanTexture& depthPyramid, const CullData& cullData, u32 timestamp,
                                        bool taskSubmit, bool late, u32 postPass) const
    {
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamps, timestamp + 0);

        u32 rasterizationStage = taskSubmit ? VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT : VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

        auto prefillBarrier = m_drawCommandCountBuffer.Barrier(VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                               VK_ACCESS_TRANSFER_WRITE_BIT);

        VkUtils::PipelineBarrier(commandBuffer, 0, 1, &prefillBarrier, 0, nullptr);

        m_drawCommandCountBuffer.Fill(commandBuffer, 0, 4, 0);

        auto pyramidBarrier = VkUtils::ImageBarrier(depthPyramid.GetImage(), late ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : 0, late ? VK_ACCESS_SHADER_WRITE_BIT : 0,
                                                    late ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                    VK_IMAGE_LAYOUT_GENERAL);

        VkBufferMemoryBarrier2 fillBarriers[] = {
            m_drawCommandBuffer.Barrier(VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | rasterizationStage, VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT),
            m_drawCommandCountBuffer.Barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                             VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
        };

        VkUtils::PipelineBarrier(commandBuffer, 0, ARRAY_SIZE(fillBarriers), fillBarriers, 1, &pyramidBarrier);

        {
            CullData passData = cullData;
            passData.postPass = postPass;

            shader.Bind(commandBuffer);
            DescriptorInfo descriptors[] = {
                m_drawBuffer,           m_meshBuffer,
                m_drawCommandBuffer,    m_drawCommandCountBuffer,
                m_drawVisibilityBuffer, DescriptorInfo(m_depthSampler, depthPyramid.GetView(), VK_IMAGE_LAYOUT_GENERAL),
            };
            shader.PushDescriptorSet(commandBuffer, descriptors);

            shader.PushConstants(commandBuffer, &passData, sizeof(CullData));
            shader.Dispatch(commandBuffer, static_cast<u32>(m_draws.Size()), 1, 1);
        }

        if (taskSubmit)  // We are doing task shading
        {
            auto syncBarrier = m_drawCommandCountBuffer.Barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

            VkUtils::PipelineBarrier(commandBuffer, 0, 1, &syncBarrier, 0, nullptr);

            m_taskSubmitShader.Bind(commandBuffer);

            DescriptorInfo descriptors[] = { m_drawCommandCountBuffer, m_drawCommandBuffer };
            m_taskSubmitShader.PushDescriptorSet(commandBuffer, descriptors);
            m_taskSubmitShader.Dispatch(commandBuffer, 1, 1, 1);
        }

        VkBufferMemoryBarrier2 cullBarriers[] = {
            m_drawCommandBuffer.Barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | rasterizationStage,
                                        VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT),
            m_drawCommandCountBuffer.Barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                                             VK_ACCESS_INDIRECT_COMMAND_READ_BIT),
        };

        VkUtils::PipelineBarrier(commandBuffer, 0, ARRAY_SIZE(cullBarriers), cullBarriers, 0, nullptr);

        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamps, timestamp + 1);
    }

    void VulkanRendererPlugin::RenderStep(VkCommandBuffer commandBuffer, VulkanTexture* gBufferTargets, const VulkanTexture& depthTarget, const VulkanTexture& depthPyramid,
                                          const Globals& globals, const Window& window, u32 query, u32 timeStamp, bool taskSubmit, bool clusterSubmit, bool late,
                                          u32 postPass) const
    {
        constexpr VkClearColorValue clearColor               = { 30.f / 255.f, 54.f / 255.f, 42.f / 255.f, 1 };
        constexpr VkClearDepthStencilValue clearDepthStencil = { 0.f, 0 };

        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamps, timeStamp + 0);

        // Begin quering our pipeline statistics
        vkCmdBeginQuery(commandBuffer, m_queryPoolStatistics, query, 0);

        if (clusterSubmit)  // We are doing cluster submit
        {
            auto prefillBarrier = m_clusterCountBuffer.Barrier(VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                               VK_ACCESS_TRANSFER_WRITE_BIT);
            VkUtils::PipelineBarrier(commandBuffer, 0, 1, &prefillBarrier, 0, nullptr);

            m_clusterCountBuffer.Fill(commandBuffer, 0, 4, 0);

            VkBufferMemoryBarrier2 fillBarriers[] = {
                m_clusterIndexBuffer.Barrier(VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT),
                m_clusterCountBuffer.Barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                             VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
            };
            VkUtils::PipelineBarrier(commandBuffer, 0, ARRAY_SIZE(fillBarriers), fillBarriers, 0, nullptr);

            auto& shader = late ? m_clusterCullLateShader : m_clusterCullShader;

            shader.Bind(commandBuffer);

            DescriptorInfo pyramidDesc(m_depthSampler, depthPyramid.GetView(), VK_IMAGE_LAYOUT_GENERAL);
            DescriptorInfo descriptors[] = {
                m_drawCommandBuffer, m_drawBuffer, m_meshletBuffer, m_meshletVisibilityBuffer, pyramidDesc, m_clusterIndexBuffer, m_clusterCountBuffer,
            };

            shader.PushDescriptorSet(commandBuffer, descriptors);

            CullData passData = globals.cullData;
            passData.postPass = postPass;

            shader.PushConstants(commandBuffer, &passData, sizeof(globals.cullData));
            shader.DispatchIndirect(commandBuffer, m_drawCommandCountBuffer, 4);

            auto syncBarrier = m_clusterCountBuffer.Barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

            VkUtils::PipelineBarrier(commandBuffer, 0, 1, &syncBarrier, 0, nullptr);

            m_clusterSubmitShader.Bind(commandBuffer);

            DescriptorInfo descriptors2[] = { m_clusterCountBuffer, m_clusterIndexBuffer };
            m_clusterSubmitShader.PushDescriptorSet(commandBuffer, descriptors2);
            m_clusterSubmitShader.Dispatch(commandBuffer, 1, 1, 1);

            VkBufferMemoryBarrier2 cullBarriers[] = {
                m_clusterIndexBuffer.Barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT, VK_ACCESS_SHADER_READ_BIT),
                m_clusterCountBuffer.Barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                                             VK_ACCESS_INDIRECT_COMMAND_READ_BIT),
            };

            VkUtils::PipelineBarrier(commandBuffer, 0, ARRAY_SIZE(cullBarriers), cullBarriers, 0, nullptr);
        }

        BeginRendering(commandBuffer, gBufferTargets, depthTarget, clearColor, clearDepthStencil, window.width, window.height, late);

        // First commands are to set the viewport and scissor
        vkCmdSetViewport(commandBuffer, 0, 1, &m_viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &m_scissor);

        Globals passGlobals           = globals;
        passGlobals.cullData.postPass = postPass;

        if (clusterSubmit)
        {
            auto& shader = postPass == 1 ? m_clusterPostMeshletShader : m_clusterMeshletShader;

            shader.Bind(commandBuffer);

            DescriptorInfo descriptors[] = { m_drawCommandBuffer, m_drawBuffer,         m_meshletBuffer,  m_meshletDataBuffer,
                                             m_vertexBuffer,      m_clusterIndexBuffer, DescriptorInfo(), m_textureSampler };
            shader.PushDescriptorSet(commandBuffer, descriptors);
            shader.BindDescriptorSet(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 1, 1, &m_textureDescriptorSet);
            shader.PushConstants(commandBuffer, &passGlobals, sizeof(globals));

            vkCmdDrawMeshTasksIndirectEXT(commandBuffer, m_clusterCountBuffer.GetHandle(), 4, 1, 0);
        }
        else if (taskSubmit)
        {
            auto& shader = postPass == 1 ? m_taskMeshletPostShader : late ? m_taskMeshletLateShader : m_taskMeshletShader;
            shader.Bind(commandBuffer);

            DescriptorInfo pyramidDesc(m_depthSampler, depthPyramid.GetView(), VK_IMAGE_LAYOUT_GENERAL);
            DescriptorInfo descriptors[] = { m_drawCommandBuffer,       m_drawBuffer, m_meshletBuffer, m_meshletDataBuffer, m_vertexBuffer,
                                             m_meshletVisibilityBuffer, pyramidDesc,  m_textureSampler };
            shader.PushDescriptorSet(commandBuffer, descriptors);
            shader.BindDescriptorSet(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 1, 1, &m_textureDescriptorSet);
            shader.PushConstants(commandBuffer, &passGlobals, sizeof(globals));

            vkCmdDrawMeshTasksIndirectEXT(commandBuffer, m_drawCommandCountBuffer.GetHandle(), 4, 1, 0);
        }
        else
        {
            auto& shader = postPass == 1 ? m_meshPostShader : m_meshShader;

            shader.Bind(commandBuffer);

            DescriptorInfo descriptors[] = { m_drawCommandBuffer, m_drawBuffer,     m_vertexBuffer,   DescriptorInfo(),
                                             DescriptorInfo(),    DescriptorInfo(), DescriptorInfo(), m_textureSampler };
            shader.PushDescriptorSet(commandBuffer, descriptors);
            shader.BindDescriptorSet(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 1, 1, &m_textureDescriptorSet);

            vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);

            shader.PushConstants(commandBuffer, &passGlobals, sizeof(globals));
            vkCmdDrawIndexedIndirectCount(commandBuffer, m_drawCommandBuffer.GetHandle(), offsetof(MeshDrawCommand, indirect), m_drawCommandCountBuffer.GetHandle(), 0,
                                          static_cast<u32>(m_draws.Size()), sizeof(MeshDrawCommand));
        }

        // End our rendering
        vkCmdEndRendering(commandBuffer);

        vkCmdEndQuery(commandBuffer, m_queryPoolStatistics, query);

        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamps, timeStamp + 1);
    }

    void VulkanRendererPlugin::DepthPyramidStep(VkCommandBuffer commandBuffer, VulkanTexture& depthTarget, VulkanTexture& depthPyramid) const
    {
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamps, 4);

        // Wait for all depth data to be written to the depth target before we start reading
        VkImageMemoryBarrier2 depthBarriers[] = {
            VkUtils::ImageBarrier(depthTarget.GetImage(), VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                  VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_IMAGE_ASPECT_DEPTH_BIT),
            VkUtils::ImageBarrier(depthPyramid.GetImage(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL),
        };

        VkUtils::PipelineBarrier(commandBuffer, 0, 0, nullptr, ARRAY_SIZE(depthBarriers), depthBarriers);

        // Bind our depth reduce shader
        m_depthReduceShader.Bind(commandBuffer);

        // Build our depth pyramid
        const auto& mips = depthPyramid.GetMips();
        for (u32 i = 0; i < mips.Size(); ++i)
        {
            DescriptorInfo sourceDepth = (i == 0) ? DescriptorInfo(m_depthSampler, depthTarget.GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                                  : DescriptorInfo(m_depthSampler, mips[i - 1], VK_IMAGE_LAYOUT_GENERAL);

            DescriptorInfo descriptors[] = { { mips[i], VK_IMAGE_LAYOUT_GENERAL }, sourceDepth };
            m_depthReduceShader.PushDescriptorSet(commandBuffer, descriptors);

            u32 levelWidth  = Max<u32>(1, depthPyramid.GetWidth() >> i);
            u32 levelHeight = Max<u32>(1, depthPyramid.GetHeight() >> i);

            DepthReduceData depthReduceData = { vec2(levelWidth, levelHeight) };

            m_depthReduceShader.PushConstants(commandBuffer, &depthReduceData, sizeof(depthReduceData));
            m_depthReduceShader.Dispatch(commandBuffer, levelWidth, levelHeight, 1);

            auto reduceBarrier = VkUtils::ImageBarrier(depthPyramid.GetImage(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, i, 1);

            VkUtils::PipelineBarrier(commandBuffer, 0, 0, nullptr, 1, &reduceBarrier);
        }

        // Wait for the depth target to be writable again
        auto depthWriteBarrier =
            VkUtils::ImageBarrier(depthTarget.GetImage(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                  VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

        VkUtils::PipelineBarrier(commandBuffer, 0, 0, nullptr, 1, &depthWriteBarrier);

        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamps, 5);
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

        // Keep track of the cpu time at which we start Begin()
        m_frameCpuBegin = Platform::GetAbsoluteTime() * 1000;

        // Reset our pools
        vkCmdResetQueryPool(commandBuffer, m_queryPoolTimestamps, 0, 128);
        vkCmdResetQueryPool(commandBuffer, m_queryPoolStatistics, 0, 3);

        // Write the start (top of pipeline) timestamp
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPoolTimestamps, 0);

        static bool firstFrame = false;
        if (!firstFrame)
        {
            m_drawVisibilityBuffer.Fill(commandBuffer, 0, sizeof(u32) * m_draws.Size(), 0);

            auto fillBarrier = m_drawVisibilityBuffer.Barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

            VkUtils::PipelineBarrier(commandBuffer, 0, 1, &fillBarrier, 0, nullptr);

            if (m_meshShadingEnabled)
            {
                m_meshletVisibilityBuffer.Fill(commandBuffer, 0, m_meshletVisibilityBytes, 0);

                auto fillBarrier = m_meshletVisibilityBuffer.Barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT,
                                                                     VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                VkUtils::PipelineBarrier(commandBuffer, 0, 1, &fillBarrier, 0, nullptr);
            }

            firstFrame = true;
        }

        m_view    = glm::mat4_cast(m_camera.orientation);
        m_view[3] = vec4(m_camera.position, 1.0f);
        m_view    = glm::inverse(m_view);
        m_view    = glm::scale(glm::identity<glm::mat4>(), vec3(1, 1, -1)) * m_view;

        constexpr f32 zNear = 0.5f;

        m_projection     = MakePerspectiveProjection(m_camera.fovY, static_cast<f32>(window.width) / static_cast<f32>(window.height), zNear);
        mat4 projectionT = glm::transpose(m_projection);

        f32 depthPyramidWidth  = static_cast<f32>(backendState->depthPyramid.GetWidth());
        f32 depthPyramidHeight = static_cast<f32>(backendState->depthPyramid.GetHeight());

        vec4 frustumX = NormalizePlane(projectionT[3] + projectionT[0]);  // x + w < 0
        vec4 frustumY = NormalizePlane(projectionT[3] + projectionT[1]);  // y + w < 0

        CullData cullData   = {};
        cullData.view       = m_view;
        cullData.p00        = m_projection[0][0];
        cullData.p11        = m_projection[1][1];
        cullData.zNear      = zNear;
        cullData.zFar       = m_drawDistance;
        cullData.frustum[0] = frustumX.x;
        cullData.frustum[1] = frustumX.z;
        cullData.frustum[2] = frustumY.y;
        cullData.frustum[3] = frustumY.z;

        cullData.drawCount = m_draws.Size();

        cullData.cullingEnabled                 = m_cullingEnabled;
        cullData.occlusionCullingEnabled        = m_occlusionCullingEnabled;
        cullData.meshShadingEnabled             = m_meshShadingEnabled;
        cullData.clusterOcclusionCullingEnabled = m_occlusionCullingEnabled && m_clusterOcclusionCullingEnabled && m_meshShadingEnabled;
        cullData.lodEnabled                     = m_lodEnabled;
        cullData.lodTarget                      = (2 / cullData.p11) * (1.f / static_cast<f32>(window.height)) * (1 << m_debugLodStep);  // 1px

        cullData.pyramidWidth  = depthPyramidWidth;
        cullData.pyramidHeight = depthPyramidHeight;

        Globals globals      = {};
        globals.projection   = m_projection;
        globals.cullData     = cullData;
        globals.screenWidth  = static_cast<f32>(window.width);
        globals.screenHeight = static_cast<f32>(window.height);

        auto& gBufferTargets = backendState->gBufferTargets;
        auto& depthTarget    = backendState->depthTarget;
        auto& depthPyramid   = backendState->depthPyramid;

        // Our GBuffer and depth target need to be in ATTACHMENT OPTIMAL layout before we can start rendering
        VkImageMemoryBarrier2 renderBeginBarriers[GBUFFER_COUNT + 1] = {
            VkUtils::ImageBarrier(depthTarget.GetImage(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT),
        };

        for (u32 i = 0; i < GBUFFER_COUNT; ++i)
        {
            renderBeginBarriers[i + 1] =
                VkUtils::ImageBarrier(gBufferTargets[i].GetImage(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
        }

        VkUtils::PipelineBarrier(commandBuffer, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, ARRAY_SIZE(renderBeginBarriers), renderBeginBarriers);

        // TODO: Refactor this to be false when m_taskShadidingEnabled is false
        auto taskSubmit    = m_meshShadingEnabled;
        auto clusterSubmit = m_meshShadingEnabled && !m_taskShadingEnabled;

        // Early cull: frustum cull & fill objects that *were* visible last frame
        CullStep(commandBuffer, taskSubmit ? m_taskCullShader : m_drawCullShader, depthPyramid, cullData, 2, taskSubmit, /* late = */ false);

        // Early render: render objects that were visible last frame
        RenderStep(commandBuffer, gBufferTargets, depthTarget, depthPyramid, globals, window, 0, 8, taskSubmit, clusterSubmit, /* late = */ false);

        // Depth pyramid generation
        DepthPyramidStep(commandBuffer, depthTarget, depthPyramid);

        // Late cull: frustum + occlusion cull & fill object that were *not* visible last frame
        CullStep(commandBuffer, taskSubmit ? m_taskCullLateShader : m_drawCullLateShader, depthPyramid, cullData, 6, taskSubmit, /* late = */ true);

        // Late render: Render opaque objects that are visible this frame but weren't drawn in the early pass
        RenderStep(commandBuffer, gBufferTargets, depthTarget, depthPyramid, globals, window, 1, 10, taskSubmit, clusterSubmit, /* late = */ true);

        // Post cull: frustum + occlusion & fill extra objects
        CullStep(commandBuffer, taskSubmit ? m_taskCullLateShader : m_drawCullLateShader, depthPyramid, cullData, 12, taskSubmit, /* late = */ true,
                 /* postPass = */ 1);

        // Post render: Render transparent objects that are visible this frame but weren't draw in the early pass
        RenderStep(commandBuffer, gBufferTargets, depthTarget, depthPyramid, globals, window, 2, 14, taskSubmit, clusterSubmit, /* late = */ true,
                   /* postPass = */ 1);

        return true;
    }  // namespace C3D

    bool VulkanRendererPlugin::End(Window& window)
    {
        auto backendState       = window.rendererState->backendState;
        auto commandBuffer      = backendState->GetCommandBuffer();
        auto swapchainImage     = backendState->swapchain.GetImage(backendState->imageIndex);
        auto swapchainImageView = backendState->swapchain.GetView(backendState->imageIndex);

        auto& gBufferTargets = backendState->gBufferTargets;
        auto& depthTarget    = backendState->depthTarget;
        auto& depthPyramid   = backendState->depthPyramid;

        // Setup out blit barriers
        VkImageMemoryBarrier2 blitBarriers[GBUFFER_COUNT + 2] = {
            // NOTE: The source image has previous state of undefined, we however still have to specify COMPUTE_SHADER to synchronize with the submitStageMask in Submit()
            VkUtils::ImageBarrier(swapchainImage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                  VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL),
            VkUtils::ImageBarrier(depthTarget.GetImage(), VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                  VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_IMAGE_ASPECT_DEPTH_BIT),
        };

        for (u32 i = 0; i < GBUFFER_COUNT; ++i)
        {
            blitBarriers[i + 2] = VkUtils::ImageBarrier(gBufferTargets[i].GetImage(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                        VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        // Wait for GBuffer targets to be in READ_ONLY_OPTIMAL, our swapchain image to be in GENERAL and our depth target to be in READ_ONLY_OPTIMAL
        VkUtils::PipelineBarrier(commandBuffer, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, ARRAY_SIZE(blitBarriers), blitBarriers);

        {
            u32 timeStamp = 16;
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamps, timeStamp + 0);

            if (m_shadingEnabled)
            {
                m_shadeShader.Bind(commandBuffer);

                DescriptorInfo descriptors[] = {
                    { swapchainImageView, VK_IMAGE_LAYOUT_GENERAL },
                    { m_readSampler, gBufferTargets[0].GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
                    { m_readSampler, gBufferTargets[1].GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
                    { m_readSampler, depthTarget.GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
                    m_tlas,
                };

                m_shadeShader.PushDescriptorSet(commandBuffer, descriptors);

                ShadeData shadeData             = {};
                shadeData.sunDirection          = m_sunDirection;
                shadeData.inverseViewProjection = inverse(m_projection * m_view);
                shadeData.imageSize             = vec2(static_cast<f32>(window.width), static_cast<f32>(window.height));

                m_shadeShader.PushConstants(commandBuffer, &shadeData, sizeof(shadeData));

                m_shadeShader.Dispatch(commandBuffer, window.width, window.height, 1);
            }
            else
            {
                m_blitShader.Bind(commandBuffer);

                DescriptorInfo descriptors[] = {
                    { swapchainImageView, VK_IMAGE_LAYOUT_GENERAL },
                    { m_readSampler, gBufferTargets[0].GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
                };

                m_blitShader.PushDescriptorSet(commandBuffer, descriptors);

                vec4 blitData = vec4(static_cast<f32>(window.width), static_cast<f32>(window.height), 0, 0);
                m_blitShader.PushConstants(commandBuffer, &blitData, sizeof(blitData));

                m_blitShader.Dispatch(commandBuffer, window.width, window.height, 1);
            }

            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamps, timeStamp + 1);
        }

        // Setup a present barrier
        auto presentBarrier =
            VkUtils::ImageBarrier(swapchainImage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, 0, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        // Wait for the swapchain image to go from TRANSFER_DST_OPTIMAL to PRESENT_SRC_KHR
        VkUtils::PipelineBarrier(commandBuffer, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &presentBarrier);

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

        VkPipelineStageFlags submitStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

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

        u64 timestampResults[18] = {};
        VK_CHECK(vkGetQueryPoolResults(device, m_queryPoolTimestamps, 0, ARRAY_SIZE(timestampResults), sizeof(timestampResults), timestampResults, sizeof(timestampResults[0]),
                                       VK_QUERY_RESULT_64_BIT));

        u32 statResults[3] = {};
        VK_CHECK(vkGetQueryPoolResults(device, m_queryPoolStatistics, 0, ARRAY_SIZE(statResults), sizeof(statResults), statResults, sizeof(statResults[0]), 0));

        auto props = m_context.device.GetProperties();

        f64 frameGpuBegin   = static_cast<f64>(timestampResults[0]) * props.limits.timestampPeriod * 1e-6;
        f64 frameGpuEnd     = static_cast<f64>(timestampResults[1]) * props.limits.timestampPeriod * 1e-6;
        f64 cullGpuTime     = static_cast<f64>(timestampResults[3] - timestampResults[2]) * props.limits.timestampPeriod * 1e-6;
        f64 pyramidGpuTime  = static_cast<f64>(timestampResults[5] - timestampResults[4]) * props.limits.timestampPeriod * 1e-6;
        f64 cullLateGpuTime = static_cast<f64>(timestampResults[7] - timestampResults[6]) * props.limits.timestampPeriod * 1e-6;

        f64 renderGpuTime     = static_cast<f64>(timestampResults[9] - timestampResults[8]) * props.limits.timestampPeriod * 1e-6;
        f64 renderLateGpuTime = static_cast<f64>(timestampResults[11] - timestampResults[10]) * props.limits.timestampPeriod * 1e-6;

        f64 cullPostGpuTime   = static_cast<f64>(timestampResults[13] - timestampResults[12]) * props.limits.timestampPeriod * 1e-6;
        f64 renderPostGpuTime = static_cast<f64>(timestampResults[15] - timestampResults[14]) * props.limits.timestampPeriod * 1e-6;

        f64 finalGpuTime = static_cast<f64>(timestampResults[17] - timestampResults[16]) * props.limits.timestampPeriod * 1e-6;

        f64 frameCpuEnd = Platform::GetAbsoluteTime() * 1000;

        m_frameCpuAvg = m_frameCpuAvg * 0.95 + (frameCpuEnd - m_frameCpuBegin) * 0.05;
        m_frameGpuAvg = m_frameGpuAvg * 0.95 + (frameGpuEnd - frameGpuBegin) * 0.05;

        f64 triangleCount = static_cast<f64>(statResults[0] + statResults[1] + statResults[2]);

        f64 trianglesPerSecond = triangleCount / (m_frameGpuAvg * 1e-3);
        f64 drawsPerSecond     = static_cast<f64>(m_draws.Size()) / (m_frameGpuAvg * 1e-3);

        auto meshShadingEnabledAndSupported = m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING) && m_meshShadingEnabled;

        auto cullTotalTime   = cullGpuTime + cullLateGpuTime + cullPostGpuTime;
        auto renderTotalTime = renderGpuTime + renderLateGpuTime + renderPostGpuTime;

        titleText.Clear();
        titleText.Format(
            "Mesh Shading: {}; Task Shading: {}; Cull: {}; Occlusion: {}; Cluster Occlusion: {}; LOD: {}; cpu: {:.2f} ms; gpu: {:.2f} ms; (cull {:.2f} ms; pyramid {:.2f} ms;"
            "render {:.2f}; final: {:.2f} ms); triangles {:.2f}M; {:.1f}B tri/sec; {:.1f}M draws/sec;",
            m_meshShadingEnabled ? "ON" : "OFF", m_taskShadingEnabled ? "ON" : "OFF", m_cullingEnabled ? "ON" : "OFF", m_occlusionCullingEnabled ? "ON" : "OFF",
            m_clusterOcclusionCullingEnabled ? "ON" : "OFF", m_lodEnabled ? "ON" : "OFF", m_frameCpuAvg, m_frameGpuAvg, cullTotalTime, pyramidGpuTime, renderTotalTime,
            finalGpuTime, triangleCount * 1e-6, trianglesPerSecond * 1e-9, drawsPerSecond * 1e-6);

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
        createInfo.context = &m_context;
        createInfo.width   = window.width;
        createInfo.height  = window.height;
        createInfo.usage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        // Create gBuffer and depth target for this window
        for (u32 i = 0; i < GBUFFER_COUNT; ++i)
        {
            createInfo.name   = String::FromFormat("GBUFFER_TARGET_{}", i);
            createInfo.format = GBUFFER_FORMATS[i];

            if (!backend->gBufferTargets[i].Create(createInfo))
            {
                ERROR_LOG("Failed to create GBuffer target {} for window: '{}'.", i, window.name);
                return false;
            }
        }

        createInfo.name   = "DEPTH_TARGET";
        createInfo.format = VK_FORMAT_D32_SFLOAT;
        createInfo.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        if (!backend->depthTarget.Create(createInfo))
        {
            ERROR_LOG("Failed to create Depth target for window: '{}'.", window.name);
            return false;
        }

        // Using the previous power of 2 ensures our reductions are at most 2x2 which makes them conservative
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
            backend->acquireSemaphores[i] = VkUtils::CreateSemaphore(&m_context, String::FromFormat("VULKAN_ACQUIRE_SEMAPHORE_{}", i));
            if (!backend->acquireSemaphores[i])
            {
                ERROR_LOG("Failed to create acquire Semaphores.");
                return false;
            }
        }

        // Get the number of swapchain images
        auto swapchainImageCount = backend->swapchain.GetImageCount();
        // Resize our present semaphores array to that size
        backend->presentSemaphores.Resize(swapchainImageCount);
        // Then create the semaphores
        for (u32 i = 0; i < swapchainImageCount; ++i)
        {
            backend->presentSemaphores[i] = VkUtils::CreateSemaphore(&m_context, String::FromFormat("VULKAN_PRESENT_SEMAPHORE_{}", i));
            if (!backend->presentSemaphores[i])
            {
                ERROR_LOG("Failed to create present Semaphores.");
                return false;
            }
        }

        INFO_LOG("Creating fences.");
        for (u32 i = 0; i < MAX_FRAMES; ++i)
        {
            backend->fences[i] = VkUtils::CreateFence(&m_context, String::FromFormat("VULKAN_FENCE_{}", i));
            if (!backend->fences[i])
            {
                ERROR_LOG("Failed to create Fences.");
                return false;
            }
        }

        INFO_LOG("Creating CommandPools and Buffers.");
        for (u32 i = 0; i < MAX_FRAMES; ++i)
        {
            backend->commandPools[i] = VkUtils::CreateCommandPool(&m_context, String::FromFormat("VK_COMMAND_POOL_{}_{}", window.name, i));
            if (!backend->commandPools[i])
            {
                ERROR_LOG("Failed to create CommandPool.");
                return false;
            }

            backend->commandBuffers[i] = VkUtils::AllocateCommandBuffer(&m_context, String::FromFormat("VK_COMMAND_BUFFER_{}_{}", window.name, i), backend->commandPools[i],
                                                                        VK_COMMAND_BUFFER_LEVEL_PRIMARY);
            if (!backend->commandBuffers[i])
            {
                ERROR_LOG("Failed to allocate CommandBuffer.");
                return false;
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
            for (u32 i = 0; i < GBUFFER_COUNT; ++i)
            {
                if (!backend->gBufferTargets[i].Resize(window.width, window.height))
                {
                    ERROR_LOG("Failed to resize GBuffer {} target.", i);
                    return false;
                }
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

            // Destroy the GBuffer and depth target
            for (u32 i = 0; i < GBUFFER_COUNT; ++i)
            {
                backend->gBufferTargets[i].Destroy();
            }
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

    bool VulkanRendererPlugin::UploadGeometry(const Geometry& geometry)
    {
        Clock clock(ClockFlags::StartOnCreate);

        auto commandPool   = m_context.commandPool;
        auto commandBuffer = m_context.commandBuffer;

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

        clock.End();

        auto vbSize      = BytesToMebiBytes(geometry.vertices.Size() * sizeof(Vertex));
        auto ibSize      = BytesToMebiBytes(geometry.indices.Size() * sizeof(u32));
        auto meshletSize = BytesToMebiBytes(geometry.meshlets.Size() * sizeof(Meshlet) + geometry.meshletData.Size() * sizeof(u32));

        INFO_LOG("Finished uploading Geometry VB: {:.2f} MB, IBL {:.2f} MB, Meshlets: {:.2f} MB (took {:.2f} ms).", vbSize, ibSize, meshletSize, clock.GetElapsedMs());

        return true;
    }

    bool VulkanRendererPlugin::UploadTexture(const TextureAsset& texture)
    {
        VulkanTexture vulkanTexture;

        VulkanTextureCreateInfo createInfo;
        createInfo.context   = &m_context;
        createInfo.name      = texture.name;
        createInfo.width     = texture.width;
        createInfo.height    = texture.height;
        createInfo.mipLevels = texture.mipLevelCount;
        createInfo.format    = VkUtils::ConvertTextureFormatToVkFormat(texture.format);
        createInfo.usage     = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        if (!vulkanTexture.Create(createInfo))
        {
            ERROR_LOG("Failed to create Vulkan Texture.");
            return false;
        }

        auto commandPool   = m_context.commandPool;
        auto commandBuffer = m_context.commandBuffer;

        auto& device       = m_context.device;
        auto logicalDevice = device.GetLogical();

        VK_CHECK(vkResetCommandPool(logicalDevice, commandPool, 0));

        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        auto preBarrier = VkUtils::ImageBarrier(vulkanTexture.GetImage(), 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkUtils::PipelineBarrier(commandBuffer, 0, 0, nullptr, 1, &preBarrier);

        // Buffer starts at 0
        u64 bufferOffset = 0;
        // First mip width and height is the width and height of the full texture
        u32 mipWidth  = texture.width;
        u32 mipHeight = texture.height;

        for (u32 i = 0; i < texture.mipLevelCount; ++i)
        {
            // Define the region to copy
            VkBufferImageCopy region               = {};
            region.bufferOffset                    = bufferOffset;
            region.bufferRowLength                 = 0;
            region.bufferImageHeight               = 0;
            region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel       = i;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount     = 1;
            region.imageOffset                     = { 0, 0, 0 };
            region.imageExtent                     = { mipWidth, mipHeight, 1 };

            vkCmdCopyBufferToImage(commandBuffer, m_context.stagingBuffer.GetHandle(), vulkanTexture.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            bufferOffset += ((mipWidth + 3) / 4) * ((mipHeight + 3) / 4) * texture.blockSize;

            mipWidth  = mipWidth > 1 ? mipWidth / 2 : 1;
            mipHeight = mipHeight > 1 ? mipHeight / 2 : 1;
        }

        if (bufferOffset != texture.size)
        {
            ERROR_LOG("The final bufferOffset does not equal the total texture size!");
            return false;
        }

        auto postBarrier = VkUtils::ImageBarrier(vulkanTexture.GetImage(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                 VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VkUtils::PipelineBarrier(commandBuffer, 0, 0, nullptr, 1, &postBarrier);

        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo       = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &commandBuffer;

        VK_CHECK(vkQueueSubmit(device.GetDeviceQueue(), 1, &submitInfo, VK_NULL_HANDLE));

        VK_CHECK(device.WaitIdle());

        VkDescriptorImageInfo imageInfo = {};
        imageInfo.sampler               = m_textureSampler;
        imageInfo.imageView             = vulkanTexture.GetView();
        imageInfo.imageLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet               = m_textureDescriptorSet;
        write.dstBinding           = 0;
        // Ensure we start indexing at 1 since 0 will be our "missing" texture
        write.dstArrayElement = m_textures.Size() + 1;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(logicalDevice, 1, &write, 0, nullptr);

        // Finally add the texture to our array so we can clean it up later
        m_textures.PushBack(vulkanTexture);

        return true;
    }

    bool VulkanRendererPlugin::GenerateDrawCommands(const Geometry& geometry)
    {
        constexpr auto drawCount = 1000000;
        m_draws.Resize(drawCount);

        u32 meshletVisibilityCount = 0;
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
            draw.orientation = glm::rotate(glm::quat(0, 0, 0, 1), angle, axis);

            draw.meshIndex               = static_cast<u32>(meshIndex);
            draw.vertexOffset            = mesh.vertexOffset;
            draw.meshletVisibilityOffset = meshletVisibilityCount;

            u32 meshletCount = 0;
            for (u32 i = 0; i < mesh.lodCount; ++i)
            {
                meshletCount = Max(meshletCount, mesh.lods[i].meshletCount);
            }

            meshletVisibilityCount += meshletCount;
        }

        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
        {
            m_meshletVisibilityBytes = (meshletVisibilityCount + 31) / 32 * sizeof(u32);

            INFO_LOG("Total meshlet visiblity count: {}; Size is: {:.2f} KB.", meshletVisibilityCount, BytesToKibiBytes(m_meshletVisibilityBytes));

            if (!m_meshletVisibilityBuffer.Create(&m_context, "MESHLET_VISIBILITY", m_meshletVisibilityBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            {
                ERROR_LOG("Failed to create meshlet visibility buffer.");
                return false;
            }
        }

        return m_drawBuffer.Upload(m_context.commandBuffer, m_context.commandPool, m_draws.GetData(), sizeof(MeshDraw) * m_draws.Size());
    }

    bool VulkanRendererPlugin::UploadDrawCommands(const Geometry& geometry, const DynamicArray<MeshDraw>& draws)
    {
        ScopedTimer timer("UploadDrawCommands");

        // Copy over the draws
        m_draws = draws;

        u32 meshletVisibilityCount = 0;
        for (auto& draw : m_draws)
        {
            const auto& mesh = geometry.meshes[draw.meshIndex];

            draw.vertexOffset            = mesh.vertexOffset;
            draw.meshletVisibilityOffset = meshletVisibilityCount;

            u32 meshletCount = 0;
            for (u32 i = 0; i < mesh.lodCount; ++i)
            {
                meshletCount = Max(meshletCount, mesh.lods[i].meshletCount);
            }

            meshletVisibilityCount += meshletCount;
        }

        if (m_context.device.IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
        {
            m_meshletVisibilityBytes = (meshletVisibilityCount + 31) / 32 * sizeof(u32);

            INFO_LOG("Total meshlet visiblity count: {}; Size is: {:.2f} KB.", meshletVisibilityCount, BytesToKibiBytes(m_meshletVisibilityBytes));

            if (!m_meshletVisibilityBuffer.Create(&m_context, "MESHLET_VISIBILITY", m_meshletVisibilityBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            {
                ERROR_LOG("Failed to create meshlet visibility buffer.");
                return false;
            }
        }

        return m_drawBuffer.Upload(m_context.commandBuffer, m_context.commandPool, m_draws.GetData(), sizeof(MeshDraw) * m_draws.Size());
    }

    void VulkanRendererPlugin::SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth, f32 maxDepth) { m_viewport = { x, y, width, height, minDepth, maxDepth }; }

    void VulkanRendererPlugin::SetScissor(i32 offsetX, i32 offsetY, u32 width, u32 height)
    {
        VkOffset2D offset = { offsetX, offsetY };
        VkExtent2D extent = { width, height };
        m_scissor         = { offset, extent };
    }

    void VulkanRendererPlugin::SetCamera(const Camera& camera) { m_camera = camera; }

    void VulkanRendererPlugin::SetSunDirection(const vec3& sunDirection) { m_sunDirection = sunDirection; }

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
