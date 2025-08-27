
#pragma once
#include <defines.h>
#include <string/string.h>
#include <volk.h>

namespace C3D
{
    struct VulkanContext;
    struct Window;

    struct VulkanTextureCreateInfo
    {
        /** @brief An optional name for debugging purposes. */
        String name;
        /** @brief The width and the height of the Texture. */
        u32 width = 0, height = 0;
        /** @brief The format to be used for this Texture. */
        VkFormat format;
        /** @brief The image usage flags. */
        VkImageUsageFlags usage;
        /** @brief A pointer to our Vulkan context. */
        VulkanContext* context = nullptr;
    };

    class VulkanTexture
    {
    public:
        bool Create(const VulkanTextureCreateInfo& info);

        bool Resize(const Window& window);

        void Destroy();

    private:
        bool CreateInternal(u32 width, u32 height);

        bool CreateImage(u32 width, u32 height);
        bool AllocateAndBind();
        bool CreateImageView();

        /** @brief An optional name for debugging purposes. */
        String m_name;
        /** @brief The format used by this Texture. */
        VkFormat m_format;
        /** @brief The image usage flags for this Texture. */
        VkImageUsageFlags m_usage;
        /** @brief A handle to the Vulkan image. */
        VkImage m_image = nullptr;
        /** @brief A handle to the Vulkan image view. */
        VkImageView m_imageView = nullptr;
        /** @brief A handle to the underlying memory used for this Vulkan Texture. */
        VkDeviceMemory m_memory = nullptr;
        /** @brief A pointer to our Vulkan context. */
        VulkanContext* m_context = nullptr;
    };
}  // namespace C3D