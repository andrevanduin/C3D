
#include "vulkan_device.h"

#include <logger/logger.h>

#include "platform/vulkan_platform.h"
#include "vulkan_context.h"
#include "vulkan_utils.h"

namespace C3D
{
    bool VulkanDevice::Create(VulkanContext* context)
    {
        m_context = context;

        // First we select the ideal phyiscal device
        if (!SelectPhyiscalDevice())
        {
            ERROR_LOG("No valid physical device could be selected.");
            return false;
        }

        float queuePriorities[] = { 1.0f };

        DynamicArray<const char*> requestedExtensions(5);
        // We always require the swapchain extension
        requestedExtensions.PushBack(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        requestedExtensions.PushBack(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

        VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueInfo.queueFamilyIndex        = m_physical.graphicsQueueFamilyIndex;
        queueInfo.queueCount              = 1;
        queueInfo.pQueuePriorities        = queuePriorities;

        VkDeviceCreateInfo deviceCreateInfo      = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        deviceCreateInfo.queueCreateInfoCount    = 1;
        deviceCreateInfo.pQueueCreateInfos       = &queueInfo;
        deviceCreateInfo.enabledExtensionCount   = requestedExtensions.Size();
        deviceCreateInfo.ppEnabledExtensionNames = requestedExtensions.GetData();

        // Fill in all the structures for our extensions

        // Dynamic rendering
        VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
        dynamicRendering.dynamicRendering                         = VK_TRUE;

        // Finally attach to the pnext pointer
        deviceCreateInfo.pNext = &dynamicRendering;

        auto result = vkCreateDevice(m_physical.handle, &deviceCreateInfo, m_context->allocator, &m_logical.handle);
        if (!VulkanUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create device: '{}.'", VulkanUtils::ResultString(result));
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
        if (!VulkanUtils::IsSuccess(result))
        {
            FATAL_LOG("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed with the following error: '{}'.", VulkanUtils::ResultString(result));
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

    bool VulkanDevice::SelectPhyiscalDevice()
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

            // Ensure we aren't trying to use a CPU
            if (m_physical.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
            {
                INFO_LOG("Device is a CPU. Skipping it.");
                continue;
            }

            // Ensure that we have a graphics queue family
            m_physical.graphicsQueueFamilyIndex = SelectGraphicsFamilyIndex(current);
            if (m_physical.graphicsQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
            {
                INFO_LOG("Device does not have a valid graphics queue family.");
                continue;
            }

            // Ensure that we support presentation on that same queue
            if (!VulkanPlatform::GetPresentationSupport(current, m_physical.graphicsQueueFamilyIndex))
            {
                INFO_LOG("Device does not support presentation on graphics queue family.");
                continue;
            }

            // Get the physical memory properties of this device
            vkGetPhysicalDeviceMemoryProperties(current, &m_physical.memory);

            // Calculate the available memory on the GPU in MB
            u32 gpuMemory = VulkanUtils::GetAvailableGPUMemoryInMB(m_physical.memory);

            Metrics.SetAllocatorAvailableSpace(GPU_ALLOCATOR_ID, MebiBytes(gpuMemory));

            auto& props = m_physical.properties;
            INFO_LOG("GPU            - {}", props.deviceName);
            INFO_LOG("Type           - {}", VkPhysicalDeviceTypeToString(props.deviceType));
            INFO_LOG("GPU Memory     - {}GiB", gpuMemory / 1024);
            INFO_LOG("Driver Version - {}.{}.{}", VK_VERSION_MAJOR(props.driverVersion), VK_VERSION_MINOR(props.driverVersion),
                     VK_VERSION_PATCH(props.driverVersion));
            INFO_LOG("API Version    - {}.{}.{}", VK_API_VERSION_MAJOR(props.apiVersion), VK_API_VERSION_MINOR(props.apiVersion),
                     VK_API_VERSION_PATCH(props.apiVersion));

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