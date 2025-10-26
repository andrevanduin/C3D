
#include "vulkan_buffer.h"

#include "time/scoped_timer.h"
#include "vulkan_context.h"
#include "vulkan_utils.h"

namespace C3D
{
    bool VulkanBuffer::Create(VulkanContext* context, const char* name, u64 size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags)
    {
        m_context     = context;
        m_size        = size;
        m_memoryFlags = memoryFlags;
        m_name        = name;

        VkBufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        createInfo.size               = size;
        createInfo.usage              = usage;

        auto device = m_context->device.GetLogical();

        VK_CHECK(vkCreateBuffer(device, &createInfo, m_context->allocator, &m_handle));

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_BUFFER, m_handle, String::FromFormat("VULKAN_BUFFER_{}", name));

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device, m_handle, &memoryRequirements);
        m_requiredSize = memoryRequirements.size;

        VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocateInfo.allocationSize       = memoryRequirements.size;
        allocateInfo.memoryTypeIndex      = m_context->device.SelectMemoryType(memoryRequirements.memoryTypeBits, memoryFlags);

        VK_CHECK(vkAllocateMemory(device, &allocateInfo, m_context->allocator, &m_memory));

        MetricsAllocate(Memory.GetId(), MemoryType::Vulkan, size, memoryRequirements.size, m_memory);

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_BUFFER, m_handle, String::FromFormat("VULKAN_BUFFER_MEMORY_{}", name));

        VK_CHECK(vkBindBufferMemory(device, m_handle, m_memory, 0));

        if (memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            // If our buffer memory is visible to the CPU we memory map it to our data pointer
            VK_CHECK(vkMapMemory(device, m_memory, 0, size, 0, &m_data));
        }

        return true;
    }

    bool VulkanBuffer::Upload(VkCommandBuffer commandBuffer, VkCommandPool commandPool, void* data, u64 size)
    {
        ScopedTimer timer(String::FromFormat("Uploading {} MiB to buffer: '{}'", BytesToMebiBytes(size), m_name));

        if (m_memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            ERROR_LOG("The memory in the '{}' buffer is accessible from the CPU so there is no need to upload. Call CopyInto() instead.");
            return false;
        }

        if (size >= m_size)
        {
            ERROR_LOG("Tried to upload data of size: {}b to buffer: '{}' which has a of size = {}b.", size, m_name, m_size);
            return false;
        }

        // Copy the data into our staging buffer
        m_context->stagingBuffer.CopyInto(data, size);

        auto device = m_context->device.GetLogical();
        VK_CHECK(vkResetCommandPool(device, commandPool, 0));

        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        // Define the region to copy
        VkBufferCopy region = {};
        region.srcOffset    = 0;
        region.dstOffset    = 0;
        region.size         = size;

        // Copy the data from our staging buffer into this buffer
        vkCmdCopyBuffer(commandBuffer, m_context->stagingBuffer.GetHandle(), m_handle, 1, &region);

        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo       = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &commandBuffer;

        auto queue = m_context->device.GetDeviceQueue();
        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

        VK_CHECK(m_context->device.WaitIdle());

        return true;
    }

    bool VulkanBuffer::CopyInto(void* source, u64 size)
    {
        if (m_memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            if (size >= m_size)
            {
                ERROR_LOG("Provided size is >= buffer size.");
                return false;
            }

            std::memcpy(m_data, source, size);

            return true;
        }

        ERROR_LOG("The memory in the '{}' buffer is not accessible from the CPU.");
        return false;
    }

    void VulkanBuffer::Fill(VkCommandBuffer commandBuffer, u64 offset, u64 size, u32 data) const
    {
        vkCmdFillBuffer(commandBuffer, m_handle, offset, size, data);
    }

    VkBufferMemoryBarrier2 VulkanBuffer::Barrier(VkPipelineStageFlags2 srcStageMask, VkAccessFlags srcAccessMask, VkPipelineStageFlags2 dstStageMask,
                                                 VkAccessFlags dstAccessMask) const
    {
        return VkUtils::BufferBarrier(m_handle, srcStageMask, srcAccessMask, dstStageMask, dstAccessMask);
    }

    void VulkanBuffer::Destroy()
    {
        if (m_context)
        {
            auto device = m_context->device.GetLogical();

            MetricsFree(Memory.GetId(), MemoryType::Vulkan, m_size, m_requiredSize, m_memory);

            vkDestroyBuffer(device, m_handle, m_context->allocator);
            vkFreeMemory(device, m_memory, m_context->allocator);
        }

        m_name.Destroy();
    }

}  // namespace C3D