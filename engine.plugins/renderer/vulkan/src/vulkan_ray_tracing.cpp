
#include "vulkan_ray_tracing.h"

#include "asserts/asserts.h"
#include "containers/dynamic_array.h"
#include "defines.h"
#include "logger/logger.h"
#include "math/math_types.h"
#include "renderer/vertex.h"
#include "time/clock.h"
#include "vulkan_buffer.h"
#include "vulkan_utils.h"

namespace C3D
{
    // Required by the spec for acceleration structures, it could be smaller for the scratch buffer but we don't care about that for now
    const u32 ALIGNMENT = 256;
    // Default size of the scratch buffer
    const u64 DEFAULT_SCRATCH_SIZE = MebiBytes(16);
    // Default LOD to use for BLAS creation
    const u32 DEFAULT_LOD_INDEX = 0;

    bool VulkanRayTracing::BuildBLAS(VulkanContext* context, const DynamicArray<Mesh>& meshes, const VulkanBuffer& vb, const VulkanBuffer& ib,
                                     DynamicArray<VkAccelerationStructureKHR>& blas, VulkanBuffer& blasBuffer)
    {
        Clock clock(ClockFlags::StartOnCreate);

        u32 numberOfMeshes = meshes.Size();

        DynamicArray<u32> primitiveCounts(numberOfMeshes);
        DynamicArray<VkAccelerationStructureGeometryKHR> geometries(numberOfMeshes);
        DynamicArray<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos(numberOfMeshes);

        u64 totalAccelerationSize = 0;
        u64 totalPrimitiveCount   = 0;
        u64 maxScratchBufferSize  = 0;

        DynamicArray<u64> accelerationOffsets(numberOfMeshes);
        DynamicArray<u64> accelerationSizes(numberOfMeshes);
        DynamicArray<u64> scratchSizes(numberOfMeshes);

        VkDeviceAddress vbAddress = vb.GetDeviceAddress();
        VkDeviceAddress ibAddress = ib.GetDeviceAddress();

        auto device = context->device.GetLogical();

        for (u32 i = 0; i < numberOfMeshes; ++i)
        {
            const auto& mesh = meshes[i];
            auto& geo        = geometries[i];
            auto& buildInfo  = buildInfos[i];

            primitiveCounts[i] = mesh.lods[DEFAULT_LOD_INDEX].indexCount / 3;

            geo.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geo.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;

            C3D_ASSERT_MSG(offsetof(Vertex, pos.z) == offsetof(Vertex, pos.x) + sizeof(float) * 2, "Vertex layout mismatch!");

            geo.geometry.triangles.sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            geo.geometry.triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;
            geo.geometry.triangles.vertexData.deviceAddress = vbAddress + mesh.vertexOffset * sizeof(Vertex);
            geo.geometry.triangles.vertexStride             = sizeof(Vertex);
            geo.geometry.triangles.maxVertex                = mesh.vertexCount - 1;
            geo.geometry.triangles.indexType                = VK_INDEX_TYPE_UINT32;
            geo.geometry.triangles.indexData.deviceAddress  = ibAddress + mesh.lods[DEFAULT_LOD_INDEX].indexOffset * sizeof(u32);

            buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
            buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            buildInfo.geometryCount = 1;
            buildInfo.pGeometries   = &geo;

            VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
            vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCounts[i], &sizeInfo);

            accelerationOffsets[i] = totalAccelerationSize;
            accelerationSizes[i]   = sizeInfo.accelerationStructureSize;
            scratchSizes[i]        = sizeInfo.buildScratchSize;

            totalAccelerationSize = (totalAccelerationSize + sizeInfo.accelerationStructureSize + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
            totalPrimitiveCount += primitiveCounts[i];
            maxScratchBufferSize = Max(maxScratchBufferSize, sizeInfo.buildScratchSize);
        }

        // Create our buffer to hold the BLAS
        if (!blasBuffer.Create(context, "BLAS_BUFFER", totalAccelerationSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create BLAS buffer.");
            return false;
        }

        // Create our scratch buffer for the BLAS creation
        VulkanBuffer scratch;
        if (!scratch.Create(context, "BLAS_SCRATCH_BUFFER", Max(DEFAULT_SCRATCH_SIZE, maxScratchBufferSize),
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
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

            auto result = vkCreateAccelerationStructureKHR(device, &accelerationInfo, context->allocator, &blas[i]);
            if (!VkUtils::IsSuccess(result))
            {
                ERROR_LOG("Failed to create vk Acceleration Structure with error: '{}'.", VkUtils::ResultString(result));
                return false;
            }
        }

        auto commandPool   = context->commandPool;
        auto commandBuffer = context->commandBuffer;

        // Reset command pool
        VK_CHECK(vkResetCommandPool(device, commandPool, 0));

        // Begin the command buffer
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        auto scratchBarrier = VkUtils::BufferBarrier(scratch.GetHandle(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                                                     VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR);

        for (u32 start = 0; start < numberOfMeshes;)
        {
            u64 scratchOffset = 0;

            // Aggregate the range that fits into our allocated scratch buffer
            u64 i = start;
            while (i < numberOfMeshes && scratchOffset + scratchSizes[i] <= scratch.GetSize())
            {
                buildInfos[i].scratchData.deviceAddress = scratchAddress + scratchOffset;
                buildInfos[i].dstAccelerationStructure  = blas[i];
                buildRanges[i].primitiveCount           = primitiveCounts[i];
                buildRangePtrs[i]                       = &buildRanges[i];

                scratchOffset = (scratchOffset + scratchSizes[i] + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
                ++i;
            }

            C3D_ASSERT(i > start);

            vkCmdBuildAccelerationStructuresKHR(commandBuffer, i - start, &buildInfos[start], &buildRangePtrs[start]);
            start = i;

            VkUtils::PipelineBarrier(commandBuffer, 0, 1, &scratchBarrier, 0, nullptr);
        }

        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo       = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &commandBuffer;

        VK_CHECK(vkQueueSubmit(context->device.GetDeviceQueue(), 1, &submitInfo, VK_NULL_HANDLE));

        context->device.WaitIdle();

        // Finally destroy our scratch buffer since we are done with it
        scratch.Destroy();

        clock.End();

        INFO_LOG("BLAS AccelerationStructureSize: {:.2f} MB, BLAS BuildScratchSize: {:.2f} MB (max {:.2f} MB), Triangles: {:.3f} M.", BytesToMebiBytes(totalAccelerationSize),
                 BytesToMebiBytes(scratch.GetSize()), BytesToMebiBytes(maxScratchBufferSize), static_cast<f64>(totalPrimitiveCount) / 1e6);
        INFO_LOG("Building BLAS took: {:.2f} ms.", clock.GetElapsedMs());

        return true;
    }

    bool VulkanRayTracing::BuildTLAS(VulkanContext* context, const DynamicArray<MeshDraw>& draws, const DynamicArray<VkAccelerationStructureKHR>& blas,
                                     VkAccelerationStructureKHR& tlas, VulkanBuffer& tlasBuffer)
    {
        Clock clock(ClockFlags::StartOnCreate);

        auto device = context->device.GetLogical();

        VulkanBuffer instances;
        if (!instances.Create(context, "TLAS_INSTANCES", sizeof(VkAccelerationStructureInstanceKHR) * draws.Size(),
                              VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            ERROR_LOG("Failed to create instance buffer.");
            return false;
        }

        u32 numberOfBlas  = blas.Size();
        u32 numberOfDraws = draws.Size();

        DynamicArray<VkDeviceAddress> blasAddresses(numberOfBlas);

        for (u32 i = 0; i < numberOfBlas; ++i)
        {
            VkAccelerationStructureDeviceAddressInfoKHR info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
            info.accelerationStructure                       = blas[i];

            blasAddresses[i] = vkGetAccelerationStructureDeviceAddressKHR(device, &info);
        }

        for (size_t i = 0; i < numberOfDraws; ++i)
        {
            const MeshDraw& draw = draws[i];
            C3D_ASSERT(draw.meshIndex < numberOfBlas);

            mat3 xform = transpose(glm::mat3_cast(draw.orientation)) * draw.scale;

            VkAccelerationStructureInstanceKHR instance = {};
            memcpy(instance.transform.matrix[0], &xform[0], sizeof(f32) * 3);
            memcpy(instance.transform.matrix[1], &xform[1], sizeof(f32) * 3);
            memcpy(instance.transform.matrix[2], &xform[2], sizeof(f32) * 3);

            instance.transform.matrix[0][3] = draw.position.x;
            instance.transform.matrix[1][3] = draw.position.y;
            instance.transform.matrix[2][3] = draw.position.z;

            instance.instanceCustomIndex            = i;
            instance.mask                           = 1 << draw.postPass;
            instance.flags                          = draw.postPass ? VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR : 0;
            instance.accelerationStructureReference = blasAddresses[draw.meshIndex];

            memcpy(static_cast<VkAccelerationStructureInstanceKHR*>(instances.GetData()) + i, &instance, sizeof(VkAccelerationStructureInstanceKHR));
        }

        VkAccelerationStructureGeometryKHR geometry    = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
        geometry.geometryType                          = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.geometry.instances.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry.geometry.instances.data.deviceAddress = instances.GetDeviceAddress();

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };

        buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;  // TODO: fast build?
        buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries   = &geometry;

        uint32_t primitiveCount = uint32_t(numberOfDraws);

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &sizeInfo);

        INFO_LOG("TLAS AccelerationStructureSize: {:.2f} MB, ScratchSize: {:.2f} MB.", BytesToMebiBytes(sizeInfo.accelerationStructureSize),
                 BytesToMebiBytes(sizeInfo.buildScratchSize));

        if (!tlasBuffer.Create(context, "TLAS_BUFFER", sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create TLAS Buffer.");
            return false;
        }

        VulkanBuffer scratch;
        if (!scratch.Create(context, "TLAS_SCRATCH_BUFFER", sizeInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            ERROR_LOG("Failed to create TLAS Scratch Buffer.");
            return false;
        }

        VkAccelerationStructureCreateInfoKHR accelerationInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };

        accelerationInfo.buffer = tlasBuffer.GetHandle();
        accelerationInfo.size   = sizeInfo.accelerationStructureSize;
        accelerationInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

        auto result = vkCreateAccelerationStructureKHR(device, &accelerationInfo, context->allocator, &tlas);
        if (!VkUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create Acceleration Structure with error: '{}'.", VkUtils::ResultString(result));
            return false;
        }

        buildInfo.dstAccelerationStructure  = tlas;
        buildInfo.scratchData.deviceAddress = scratch.GetDeviceAddress();

        VkAccelerationStructureBuildRangeInfoKHR buildRange           = {};
        buildRange.primitiveCount                                     = primitiveCount;
        const VkAccelerationStructureBuildRangeInfoKHR* buildRangePtr = &buildRange;

        auto commandPool   = context->commandPool;
        auto commandBuffer = context->commandBuffer;

        VK_CHECK(vkResetCommandPool(device, commandPool, 0));

        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &buildRangePtr);

        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo       = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &commandBuffer;

        VK_CHECK(vkQueueSubmit(context->device.GetDeviceQueue(), 1, &submitInfo, VK_NULL_HANDLE));

        context->device.WaitIdle();

        // Finally destroy our temporary buffers
        scratch.Destroy();
        instances.Destroy();

        clock.End();
        INFO_LOG("Building TLAS took: {:.2f} ms.", clock.GetElapsedMs());

        return true;
    }

}  // namespace C3D