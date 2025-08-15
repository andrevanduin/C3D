
#pragma once
#include <defines.h>
#include <vulkan/vulkan_core.h>

namespace C3D
{
    struct VulkanContext;

    class VulkanBuffer
    {
    public:
        bool Create(VulkanContext* context, const char* name, u64 size, VkBufferUsageFlags flags);

        bool CopyInto(void* source, u64 size);

        void Destroy();

        VkBuffer GetHandle() const { return m_handle; }

    private:
        /** @brief A handle to our Vulkan buffer. */
        VkBuffer m_handle = nullptr;
        /** @brief The memory that our Vulkan buffer resides in */
        VkDeviceMemory m_memory = nullptr;
        /** @brief A pointer to the data in our buffer. */
        void* m_data = nullptr;
        /** @brief The size (in bytes) of our buffer. */
        u64 m_size = 0;
        /** @brief A pointer to our Vulkan context. */
        VulkanContext* m_context = nullptr;
    };
}  // namespace C3D