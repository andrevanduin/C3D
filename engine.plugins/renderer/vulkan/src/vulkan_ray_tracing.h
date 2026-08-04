
#pragma once

#include "containers/dynamic_array.h"
#include "renderer/mesh.h"
#include "vulkan_buffer.h"
#include "vulkan_context.h"

namespace C3D
{
    namespace VulkanRayTracing
    {
        bool BuildBLAS(VulkanContext* context, const DynamicArray<Mesh>& meshes, const VulkanBuffer& vb, const VulkanBuffer& ib, DynamicArray<VkAccelerationStructureKHR>& blas,
                       VulkanBuffer& blasBuffer);

        bool BuildTLAS(VulkanContext* context, const DynamicArray<MeshDraw>& draws, const DynamicArray<VkAccelerationStructureKHR>& blas, VkAccelerationStructureKHR& tlas,
                       VulkanBuffer& tlasBuffer);

    }  // namespace VulkanRayTracing
}  // namespace C3D