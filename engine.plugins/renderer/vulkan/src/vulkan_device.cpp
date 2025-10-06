
#include "vulkan_device.h"

#include <logger/logger.h>
#include <string/string_utils.h>

#include "platform/vulkan_platform.h"
#include "vulkan_context.h"
#include "vulkan_utils.h"

namespace C3D
{
    bool VulkanDevice::Create(VulkanContext* context)
    {
        m_context = context;

        DynamicArray<const char*> requiredExtensions = {
            // We always require the swapchain extension
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            // We always want to use push descriptors
            VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        };

        // First we select the ideal phyiscal device
        if (!SelectPhyiscalDevice(requiredExtensions))
        {
            ERROR_LOG("No valid physical device could be selected.");
            return false;
        }

        if (IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
        {
            requiredExtensions.PushBack(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        }

        float queuePriorities[] = { 1.0f };

        VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };

        queueInfo.queueFamilyIndex = m_physical.graphicsQueueFamilyIndex;
        queueInfo.queueCount       = 1;
        queueInfo.pQueuePriorities = queuePriorities;

        VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };

        createInfo.queueCreateInfoCount    = 1;
        createInfo.pQueueCreateInfos       = &queueInfo;
        createInfo.enabledExtensionCount   = requiredExtensions.Size();
        createInfo.ppEnabledExtensionNames = requiredExtensions.GetData();

        // Fill in all the structures for our extensions
        VkPhysicalDeviceFeatures2 deviceFeatures2        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        deviceFeatures2.features.multiDrawIndirect       = VK_TRUE;
        deviceFeatures2.features.pipelineStatisticsQuery = VK_TRUE;

        createInfo.pNext = &deviceFeatures2;

        // 16-Bit storage
        VkPhysicalDeviceVulkan11Features device11Features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };

        device11Features.storageBuffer16BitAccess           = VK_TRUE;
        device11Features.uniformAndStorageBuffer16BitAccess = VK_TRUE;
        device11Features.shaderDrawParameters               = VK_TRUE;

        deviceFeatures2.pNext = &device11Features;

        // Enable Vulkan 1.2 features
        VkPhysicalDeviceVulkan12Features device12Features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };

        device12Features.drawIndirectCount                 = VK_TRUE;
        device12Features.storageBuffer8BitAccess           = VK_TRUE;
        device12Features.uniformAndStorageBuffer8BitAccess = VK_TRUE;
        device12Features.storagePushConstant8              = VK_TRUE;
        device12Features.shaderFloat16                     = VK_TRUE;
        device12Features.shaderInt8                        = VK_TRUE;
        device12Features.samplerFilterMinmax               = VK_TRUE;
        device12Features.scalarBlockLayout                 = VK_TRUE;
        device12Features.bufferDeviceAddress               = VK_TRUE;
        device11Features.pNext                             = &device12Features;

        // Dynamic rendering
        VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };

        dynamicRendering.dynamicRendering = VK_TRUE;
        device12Features.pNext            = &dynamicRendering;

        // Mesh shaders
        VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };

        meshShaderFeatures.meshShader = VK_TRUE;
        meshShaderFeatures.taskShader = VK_TRUE;

        if (IsFeatureSupported(PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING))
        {
            dynamicRendering.pNext = &meshShaderFeatures;
        }

        auto result = vkCreateDevice(m_physical.handle, &createInfo, m_context->allocator, &m_logical.handle);
        if (!VkUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create device: '{}.'", VkUtils::ResultString(result));
            return false;
        }

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_DEVICE, m_logical.handle, "VULKAN_LOGICAL_DEVICE");

        INFO_LOG("Logical device created.");

        vkGetDeviceQueue(m_logical.handle, m_physical.graphicsQueueFamilyIndex, 0, &m_logical.queue);

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_QUEUE, m_logical.queue, "VULKAN_DEVICE_QUEUE");

        INFO_LOG("Device queues obtained");

        INFO_LOG("Device created Successfully.");
        return true;
    }

    void VulkanDevice::QuerySwapchainSupport(VkSurfaceKHR surface)
    {
        auto result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical.handle, surface, &m_physical.swapchainSupportInfo.capabilities);
        if (!VkUtils::IsSuccess(result))
        {
            FATAL_LOG("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed with the following error: '{}'.", VkUtils::ResultString(result));
        }

        u32 formatCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical.handle, surface, &formatCount, nullptr))

        if (formatCount != 0)
        {
            m_physical.swapchainSupportInfo.formats.Resize(formatCount);
            VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical.handle, surface, &formatCount, m_physical.swapchainSupportInfo.formats.GetData()))
        }

        u32 presentModeCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical.handle, surface, &presentModeCount, nullptr))

        if (presentModeCount != 0)
        {
            m_physical.swapchainSupportInfo.presentModes.Resize(presentModeCount);
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical.handle, surface, &presentModeCount,
                                                               m_physical.swapchainSupportInfo.presentModes.GetData()))
        }

        INFO_LOG("Swapchain support information obtained.");
    }

    u32 VulkanDevice::SelectMemoryType(u32 memoryTypeBits, VkMemoryPropertyFlags flags) const
    {
        for (u32 i = 0; i < m_physical.memory.memoryTypeCount; ++i)
        {
            if ((memoryTypeBits & (1 << i)) != 0 && (m_physical.memory.memoryTypes[i].propertyFlags & flags) == flags)
            {
                return i;
            }
        }

        FATAL_LOG("No compatible memory type found!");
        return INVALID_ID;
    }

    void VulkanDevice::Destroy()
    {
        INFO_LOG("Destroying Logical Device.");
        vkDestroyDevice(m_logical.handle, m_context->allocator);
        m_logical.handle = nullptr;

        INFO_LOG("Releasing Physical Device Handle.");
        m_physical.handle = nullptr;

        m_physical.swapchainSupportInfo.formats.Destroy();
        m_physical.swapchainSupportInfo.presentModes.Destroy();
    }

    VkResult VulkanDevice::WaitIdle() const { return vkDeviceWaitIdle(m_logical.handle); }

    const char* VkPhysicalDeviceTypeToString(VkPhysicalDeviceType type)
    {
        switch (type)
        {
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                return "CPU";
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                return "Integrated";
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                return "Discrete";
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                return "Virtual";
            case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            default:
                return "Unknown";
        }
    }

    u32 VulkanDevice::SelectGraphicsFamilyIndex(VkPhysicalDevice handle)
    {
        u32 queueCount = 0;

        // Get the number of physical device queue families
        vkGetPhysicalDeviceQueueFamilyProperties(handle, &queueCount, nullptr);

        // Actual get the properties and populate our array
        DynamicArray<VkQueueFamilyProperties> queueFamilyProperties;
        queueFamilyProperties.Resize(queueCount);

        vkGetPhysicalDeviceQueueFamilyProperties(handle, &queueCount, queueFamilyProperties.GetData());

        for (u32 i = 0; i < queueCount; ++i)
        {
            if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                return i;
            }
        }

        return VK_QUEUE_FAMILY_IGNORED;
    }

    bool VulkanDevice::DeviceSupportsMandatoryRequirements(VkPhysicalDevice device, const DynamicArray<const char*>& requiredExtensions)
    {
        // Ensure we aren't trying to use a CPU
        if (m_physical.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
        {
            INFO_LOG("Device is a CPU. Skipping it.");
            return false;
        }

        // Ensure that we have a graphics queue family
        m_physical.graphicsQueueFamilyIndex = SelectGraphicsFamilyIndex(device);
        if (m_physical.graphicsQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
        {
            INFO_LOG("Device does not have a valid graphics queue family.");
            return false;
        }

        // Ensure that we support presentation on that same queue
        if (!VulkanPlatform::GetPresentationSupport(device, m_physical.graphicsQueueFamilyIndex))
        {
            INFO_LOG("Device does not support presentation on graphics queue family.");
            return false;
        }

        // Ensure we support timestamps during compute and graphics
        if (!m_physical.properties.limits.timestampComputeAndGraphics)
        {
            INFO_LOG("Device does not support timestamps during compute and graphics.");
            return false;
        }

        // Ensure we are capable of using Vulkan 1.2 or greater
        if (m_physical.properties.apiVersion < VK_API_VERSION_1_2)
        {
            INFO_LOG("Device does not support Vulkan 1.2");
            return false;
        }

        // Iterate over all available extensions and check if we support all required ones
        u32 extensionCount = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr));

        DynamicArray<VkExtensionProperties> supportedExtensions(extensionCount);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, supportedExtensions.GetData()));

        // For all our required extensions: check if they are part of the supportedExtensions array
        for (auto requiredExtensionName : requiredExtensions)
        {
            bool supported = false;

            for (const auto& extension : supportedExtensions)
            {
                if (StringUtils::Equals(extension.extensionName, requiredExtensionName))
                {
                    supported = true;
                    break;
                }
            }

            if (!supported)
            {
                ERROR_LOG("Device does not support required extension: '{}'.", requiredExtensionName);
                return false;
            }
        }

        // Check if mesh shading is supported
        for (const auto& extension : supportedExtensions)
        {
            if (StringUtils::Equals(extension.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME))
            {
                m_physical.supportFlags |= PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING;
                break;
            }
        }

        return true;
    }

    bool VulkanDevice::SelectPhyiscalDevice(const DynamicArray<const char*>& requiredExtensions)
    {
        // Get the number of phyiscal devices connected to the computer
        u32 physicalDeviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(m_context->instance, &physicalDeviceCount, nullptr));

        if (physicalDeviceCount == 0)
        {
            ERROR_LOG("No physical devices that support Vulkan were found.");
            return false;
        }

        // Actually get the phyiscal devices and populate our array
        DynamicArray<VkPhysicalDevice> physicalDevices;
        physicalDevices.Resize(physicalDeviceCount);

        VK_CHECK(vkEnumeratePhysicalDevices(m_context->instance, &physicalDeviceCount, physicalDevices.GetData()));

        // Iterate physical devices to find one that we can use
        VkPhysicalDevice current = nullptr;
        for (auto device : physicalDevices)
        {
            current = device;
            vkGetPhysicalDeviceProperties(current, &m_physical.properties);

            INFO_LOG("Evaluating device: '{}'", m_physical.properties.deviceName);

            if (!DeviceSupportsMandatoryRequirements(current, requiredExtensions))
            {
                INFO_LOG("Device does not support all mandatory requirements. Skipping it.");
                continue;
            }

            // Get the physical memory properties of this device
            vkGetPhysicalDeviceMemoryProperties(current, &m_physical.memory);

            // Calculate the available memory on the GPU in MB
            u32 gpuMemory = VkUtils::GetAvailableGPUMemoryInMB(m_physical.memory);

            Metrics.SetAllocatorAvailableSpace(GPU_ALLOCATOR_ID, MebiBytes(gpuMemory));

            auto& props = m_physical.properties;
            INFO_LOG("GPU            - {}", props.deviceName);
            INFO_LOG("Type           - {}", VkPhysicalDeviceTypeToString(props.deviceType));
            INFO_LOG("GPU Memory     - {}GiB", gpuMemory / 1024);
            INFO_LOG("Driver Version - {}.{}.{}", VK_VERSION_MAJOR(props.driverVersion), VK_VERSION_MINOR(props.driverVersion),
                     VK_VERSION_PATCH(props.driverVersion));
            INFO_LOG("API Version    - {}.{}.{}", VK_API_VERSION_MAJOR(props.apiVersion), VK_API_VERSION_MINOR(props.apiVersion),
                     VK_API_VERSION_PATCH(props.apiVersion));
            INFO_LOG("Max PushConstants size: {} Bytes", props.limits.maxPushConstantsSize);
            INFO_LOG("Max DrawIndirect count: {}", props.limits.maxDrawIndirectCount);

            // We have found our perfect GPU let's save off it's handle and break
            m_physical.handle = current;
            break;
        }

        if (!m_physical.handle)
        {
            ERROR_LOG("Failed to find a suitable phyiscal device. Do you have a GPU that supports Vulkan?");
            return false;
        }

        return true;
    }
}  // namespace C3D