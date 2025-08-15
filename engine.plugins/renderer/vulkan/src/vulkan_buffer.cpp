
#include "vulkan_buffer.h"

#include "vulkan_context.h"
#include "vulkan_utils.h"

namespace C3D
{
    bool VulkanBuffer::Create(VulkanContext* context, const char* name, u64 size, VkBufferUsageFlags usage)
    {
        m_context = context;
        m_size    = size;

        VkBufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        createInfo.size               = size;
        createInfo.usage              = usage;

        auto device = m_context->device.GetLogical();

        VK_CHECK(vkCreateBuffer(device, &createInfo, m_context->allocator, &m_handle));

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_BUFFER, m_handle, String::FromFormat("VULKAN_BUFFER_{}", name));

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device, m_handle, &memoryRequirements);

        VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocateInfo.allocationSize       = memoryRequirements.size;
        allocateInfo.memoryTypeIndex =
            m_context->device.SelectMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VK_CHECK(vkAllocateMemory(device, &allocateInfo, m_context->allocator, &m_memory));

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_BUFFER, m_handle, String::FromFormat("VULKAN_BUFFER_MEMORY_{}", name));

        VK_CHECK(vkBindBufferMemory(device, m_handle, m_memory, 0));

        VK_CHECK(vkMapMemory(device, m_memory, 0, size, 0, &m_data));

        return true;
    }

    bool VulkanBuffer::CopyInto(void* source, u64 size)
    {
        if (size >= m_size)
        {
            ERROR_LOG("Provided size is >= buffer size.");
            return false;
        }

        std::memcpy(m_data, source, size);

        return true;
    }

    void VulkanBuffer::Destroy()
    {
        auto device = m_context->device.GetLogical();

        vkFreeMemory(device, m_memory, m_context->allocator);
        vkDestroyBuffer(device, m_handle, m_context->allocator);
    }

}  // namespace C3D