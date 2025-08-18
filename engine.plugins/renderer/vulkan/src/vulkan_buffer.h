
#pragma once
#include <defines.h>
#include <volk.h>

namespace C3D
{
    struct VulkanContext;

    class VulkanBuffer
    {
    public:
        bool Create(VulkanContext* context, const char* name, u64 size, VkBufferUsageFlags flags, VkMemoryPropertyFlags memoryFlags);

        /** @brief Uploads the data stored in this buffer to the GPU. */
        bool Upload(VkCommandBuffer commandBuffer, VkCommandPool commandPool, void* data, u64 size);

        bool CopyInto(void* source, u64 size);

        void Destroy();

        VkBuffer GetHandle() const { return m_handle; }
        const VkBuffer* GetHandlePtr() const { return &m_handle; }

        u64 GetSize() const { return m_size; }

    private:
        /** @brief A handle to our Vulkan buffer. */
        VkBuffer m_handle = nullptr;
        /** @brief The memory that our Vulkan buffer resides in */
        VkDeviceMemory m_memory = nullptr;
        /** @brief A pointer to the data in our buffer (if the buffer is accessible from the CPU). */
        void* m_data = nullptr;
        /** @brief The size (in bytes) of our buffer. */
        u64 m_size = 0;
        /** @brief The actual required size returned by vkGetBufferMemoryRequirements (taking alignment and everything into account). */
        u64 m_requiredSize = 0;
        /** @brief The memory property flags used to create this buffer. */
        VkMemoryPropertyFlags m_memoryFlags;
        /** @brief A pointer to our Vulkan context. */
        VulkanContext* m_context = nullptr;
    };
}  // namespace C3D