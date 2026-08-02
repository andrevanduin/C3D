
#pragma once

#include "renderer/mesh.h"
#include "vulkan_buffer.h"
#include "vulkan_context.h"

namespace C3D
{
    namespace VulkanRayTracing
    {
        bool BuildBLAS(VulkanContext* context, const Geometry& geometry, const VulkanBuffer& vb, const VulkanBuffer& ib, DynamicArray<VkAccelerationStructureKHR>& blas,
                       VulkanBuffer& blasBuffer);

    }
}  // namespace C3D