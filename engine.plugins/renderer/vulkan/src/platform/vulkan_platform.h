#pragma once
#include <containers/dynamic_array.h>
#include <defines.h>
#include <vulkan/vulkan.h>

namespace C3D
{
    struct Window;

    namespace VulkanPlatform
    {
        bool CreateSurface(VulkanContext& context, Window& window);

        DynamicArray<const char*> GetRequiredExtensionNames();

        bool GetPresentationSupport(VkPhysicalDevice physicalDevice, u32 queueFamilyIndex);
    }  // namespace VulkanPlatform

}  // namespace C3D
