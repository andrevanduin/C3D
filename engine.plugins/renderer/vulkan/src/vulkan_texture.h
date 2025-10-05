
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
        /** @brief The number of mip levels for this Texture. */
        u32 mipLevels = 1;
        /** @brief A pointer to our Vulkan context. */
        VulkanContext* context = nullptr;
    };

    class VulkanTexture
    {
    public:
        bool Create(const VulkanTextureCreateInfo& info);

        /**
         * @brief Resize the texture to the size of the provided width and height.
         *
         * @param width The width that the texture should have after resizing
         * @param height The height that the texture should have after resizing
         * @param mips The number of mips required after resizing
         * @return True if successful; false otherwise
         */
        bool Resize(u32 width, u32 height, u32 mips = 1);

        /**
         * @brief Create a Image barrier for this image with the provided arguments.
         * This can later be used in a vkCmdPipelineBarrier to wait for the image to be in the newLayout before proceeding.
         *
         * @param srcAccessMask The access mask for the source
         * @param dstAccessMask The access mask for the destination
         * @param newLayout The expected new layout of the image
         * @return VkImageMemoryBarrier that can be used in the vkCmdPipelineBarrier call
         */
        VkImageMemoryBarrier CreateBarrier(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout newLayout);

        /**
         * @brief Copy the contents of this image to the provided target.
         *
         * @param commandBuffer The command buffer to which we should record the copy command
         * @param target The target image
         * @param targetAspectMask The target image's aspect mask
         * @param targetLayout The target image's layout
         */
        void CopyTo(VkCommandBuffer commandBuffer, VkImage target, VkImageAspectFlags targetAspectMask, VkImageLayout targetLayout);

        /** @brief Destroy the vulkan texture and it's internal resources. */
        void Destroy();

        VkImageView GetView() const { return m_imageView; }

        const DynamicArray<VkImageView>& GetMips() const { return m_mipViews; }

    private:
        bool CreateInternal(u32 width, u32 height);

        bool AllocateAndBind();

        /** @brief An optional name for debugging purposes. */
        String m_name;
        /** @brief The width and the height of this Texture. */
        u32 m_width = 0, m_height = 0;
        /** @brief The number of mip levels in this Texture. */
        u32 m_mipLevels = 1;
        /** @brief The format used by this Texture. */
        VkFormat m_format;
        /** @brief The image usage flags for this Texture. */
        VkImageUsageFlags m_usage;
        /** @brief The image aspect mask for this Texture. */
        VkImageAspectFlags m_aspectMask;
        /** @brief The current layout of the underlying Vulkan image. */
        VkImageLayout m_currentLayout;
        /** @brief A handle to the Vulkan image. */
        VkImage m_image = nullptr;
        /** @brief A handle to the Vulkan image view base level (of all mips). */
        VkImageView m_imageView = nullptr;
        /** @brief Handles to the Vulkan image views for each mip. */
        DynamicArray<VkImageView> m_mipViews;
        /** @brief A handle to the underlying memory used for this Vulkan Texture. */
        VkDeviceMemory m_memory = nullptr;
        /** @brief A pointer to our Vulkan context. */
        VulkanContext* m_context = nullptr;
    };
}  // namespace C3D