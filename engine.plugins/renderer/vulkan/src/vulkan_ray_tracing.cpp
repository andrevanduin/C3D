
#include "vulkan_ray_tracing.h"

#include <cstddef>

#include "asserts/asserts.h"
#include "containers/dynamic_array.h"
#include "logger/logger.h"
#include "renderer/vertex.h"
#include "vulkan_buffer.h"
#include "vulkan_utils.h"

namespace C3D
{
    // Required by the spec for acceleration structures, it could be smaller for the scratch buffer but we don't care about that for now
    const u32 ALIGNMENT = 256;

    bool VulkanRayTracing::BuildBLAS(VulkanContext* context, const Geometry& geometry, const VulkanBuffer& vb, const VulkanBuffer& ib,
                                     DynamicArray<VkAccelerationStructureKHR>& blas, VulkanBuffer& blasBuffer)
    {
        u32 numberOfMeshes = geometry.meshes.Size();

        DynamicArray<u32> primitiveCounts(numberOfMeshes);
        DynamicArray<VkAccelerationStructureGeometryKHR> geometries(numberOfMeshes);
        DynamicArray<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos(numberOfMeshes);

        u64 totalAccelerationSize = 0;
        u64 totalScratchSize      = 0;

        DynamicArray<u64> accelerationOffsets(numberOfMeshes);
        DynamicArray<u64> accelerationSizes(numberOfMeshes);
        DynamicArray<u64> scratchOffsets(numberOfMeshes);

        VkDeviceAddress vbAddress = vb.GetDeviceAddress();
        VkDeviceAddress ibAddress = ib.GetDeviceAddress();

        auto device = context->device.GetLogical();

        for (u32 i = 0; i < numberOfMeshes; ++i)
        {
            const auto& mesh = geometry.meshes[i];
            auto& geo        = geometries[i];
            auto& buildInfo  = buildInfos[i];

            primitiveCounts[i] = mesh.lods[0].indexCount / 3;

            geo.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geo.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;

            C3D_ASSERT_MSG(offsetof(Vertex, pos.z) == offsetof(Vertex, pos.x) + sizeof(float) * 2, "Vertex layout mismatch!");

            geo.geometry.triangles.sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            geo.geometry.triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;
            geo.geometry.triangles.vertexData.deviceAddress = vbAddress + mesh.vertexOffset * sizeof(Vertex);
            geo.geometry.triangles.vertexStride             = sizeof(Vertex);
            geo.geometry.triangles.maxVertex                = mesh.vertexCount;
            geo.geometry.triangles.indexType                = VK_INDEX_TYPE_UINT32;
            geo.geometry.triangles.indexData.deviceAddress  = ibAddress + mesh.lods[0].indexOffset * sizeof(u32);

            buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            buildInfo.geometryCount = 1;
            buildInfo.pGeometries   = &geo;

            VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
            vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCounts[i], &sizeInfo);

            accelerationOffsets[i] = totalAccelerationSize;
            accelerationSizes[i]   = sizeInfo.accelerationStructureSize;
            scratchOffsets[i]      = totalScratchSize;

            totalAccelerationSize = (totalAccelerationSize + sizeInfo.accelerationStructureSize + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
            totalScratchSize      = (totalScratchSize + sizeInfo.buildScratchSize + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
        }

        INFO_LOG("RT AccelerationStructureSize: {} RT BuildScratchSize: {}", totalAccelerationSize, totalScratchSize);

        // Create our buffer to hold the BLAS
        if (!blasBuffer.Create(context, "BLAS_BUFFER", totalAccelerationSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create BLAS buffer.");
            return false;
        }

        // Create our scratch buffer for the BLAS creation
        VulkanBuffer scratch;
        if (!scratch.Create(context, "BLAS_SCRATCH_BUFFER", totalScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create BLAS scratch buffer.");
            return false;
        }

        VkDeviceAddress scratchAddress = scratch.GetDeviceAddress();

        // Resize our BLAS array to fit the number of total meshes
        blas.Resize(numberOfMeshes);

        DynamicArray<VkAccelerationStructureBuildRangeInfoKHR> buildRanges(numberOfMeshes);
        DynamicArray<const VkAccelerationStructureBuildRangeInfoKHR*> buildRangePtrs(numberOfMeshes);

        // Create our acceleration structures
        for (u32 i = 0; i < numberOfMeshes; ++i)
        {
            VkAccelerationStructureCreateInfoKHR accelerationInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };

            accelerationInfo.buffer = blasBuffer.GetHandle();
            accelerationInfo.offset = accelerationOffsets[i];
            accelerationInfo.size   = accelerationSizes[i];
            accelerationInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

            auto result = vkCreateAccelerationStructureKHR(device, &accelerationInfo, nullptr, &blas[i]);
            if (!VkUtils::IsSuccess(result))
            {
                ERROR_LOG("Failed to create vk Acceleration Structure with error: '{}'.", VkUtils::ResultString(result));
                return false;
            }

            buildInfos[i].dstAccelerationStructure  = blas[i];
            buildInfos[i].scratchData.deviceAddress = scratchAddress + scratchOffsets[i];

            buildRanges[i].primitiveCount = primitiveCounts[i];
            buildRangePtrs[i]             = &buildRanges[i];
        }

        // Reset command pool
        VK_CHECK(vkResetCommandPool(device, context->commandPool, 0));

        // Begin the command buffer
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(context->commandBuffer, &beginInfo));

        vkCmdBuildAccelerationStructuresKHR(context->commandBuffer, numberOfMeshes, buildInfos.GetData(), buildRangePtrs.GetData());

        VK_CHECK(vkEndCommandBuffer(context->commandBuffer));

        VkSubmitInfo submitInfo       = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &context->commandBuffer;

        VK_CHECK(vkQueueSubmit(context->device.GetDeviceQueue(), 1, &submitInfo, VK_NULL_HANDLE));

        VK_CHECK(vkDeviceWaitIdle(device));

        // Finally destroy our scratch buffer since we are done with it
        scratch.Destroy();

        return true;
    }
}  // namespace C3D