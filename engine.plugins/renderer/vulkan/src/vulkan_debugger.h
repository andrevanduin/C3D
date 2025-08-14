
#pragma once

namespace C3D
{
    struct VulkanContext;

    namespace VulkanDebugger
    {
        bool Create(VulkanContext& context);

        void Destroy(VulkanContext& context);
    }  // namespace VulkanDebugger
}  // namespace C3D