
#pragma once
#include <defines.h>

#include "vulkan_context.h"

namespace C3D
{
    namespace VulkanInstance
    {
        bool Create(VulkanContext& context, const char* applicationName, u32 applicationVersion);
        void Destroy(VulkanContext& context);
    }  // namespace VulkanInstance
}  // namespace C3D