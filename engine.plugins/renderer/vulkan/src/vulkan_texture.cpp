
#include "vulkan_texture.h"

#include <logger/logger.h>
#include <platform/platform_types.h>

#include "vulkan_context.h"
#include "vulkan_utils.h"

namespace C3D
{
    static VkImageAspectFlags FormatToAspectMask(VkFormat format)
    {
        switch (format)
        {
            case VK_FORMAT_D32_SFLOAT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    bool VulkanTexture::Create(const VulkanTextureCreateInfo& createInfo)
    {
        m_context    = createInfo.context;
        m_name       = createInfo.name;
        m_format     = createInfo.format;
        m_usage      = createInfo.usage;
        m_aspectMask = FormatToAspectMask(createInfo.format);

        INFO_LOG("Creating: '{}'", m_name);

        if (!m_context)
        {
            ERROR_LOG("Failed to create VulkanTexture: '{}', invalid context provided.", createInfo.name);
            return false;
        }

        return CreateInternal(createInfo.width, createInfo.height);
    }

    bool VulkanTexture::Resize(u32 width, u32 height)
    {
        INFO_LOG("Resizing: '{}'.", m_name);

        if (m_image)
        {
            Destroy();
        }

        return CreateInternal(width, height);
    }

    VkImageMemoryBarrier VulkanTexture::CreateBarrier(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout newLayout)
    {
        // Create our barrier
        auto barrier = VulkanUtils::CreateImageBarrier(m_image, srcAccessMask, dstAccessMask, m_currentLayout, newLayout, m_aspectMask);
        // Our new layout will be the current one after the barrier command has executed
        m_currentLayout = newLayout;

        return barrier;
    }

    void VulkanTexture::CopyTo(VkCommandBuffer commandBuffer, VkImage target, VkImageAspectFlags targetAspectMask, VkImageLayout targetLayout)
    {
        VkImageCopy copyRegion               = {};
        copyRegion.srcSubresource.aspectMask = m_aspectMask;
        copyRegion.srcSubresource.layerCount = 1;
        copyRegion.dstSubresource.aspectMask = targetAspectMask;
        copyRegion.dstSubresource.layerCount = 1;
        copyRegion.extent                    = { m_width, m_height, 1 };

        vkCmdCopyImage(commandBuffer, m_image, m_currentLayout, target, targetLayout, 1, &copyRegion);
    }

    bool VulkanTexture::CreateInternal(u32 width, u32 height)
    {
        INFO_LOG("Creating: '{}'.", m_name);

        // After creation we start the image as VK_IMAGE_LAYOUT_UNDEFINED
        m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // Keep track of the width and height of the image
        m_width  = width;
        m_height = height;

        if (!CreateImage())
        {
            ERROR_LOG("Failed to create VulkanTexture: '{}'.", m_name);
            return false;
        }

        if (!AllocateAndBind())
        {
            ERROR_LOG("Failed to create VulkanTexture: '{}'.", m_name);
            return false;
        }

        if (!CreateImageView())
        {
            ERROR_LOG("Failed to create VulkanTexture: '{}'.", m_name);
            return false;
        }

        return true;
    }

    bool VulkanTexture::CreateImage()
    {
        VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };

        // TODO: Quite some assumptions here for now
        createInfo.imageType     = VK_IMAGE_TYPE_2D;
        createInfo.format        = m_format;
        createInfo.extent        = { m_width, m_height, 1 };
        createInfo.mipLevels     = 1;
        createInfo.arrayLayers   = 1;
        createInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        createInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        createInfo.usage         = m_usage;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        auto result = vkCreateImage(m_context->device.GetLogical(), &createInfo, m_context->allocator, &m_image);
        if (!VulkanUtils::IsSuccess(result))
        {
            ERROR_LOG("vkCreateImage failed with error: '{}'.", VulkanUtils::ResultString(result));
            return false;
        }

        return true;
    }

    bool VulkanTexture::CreateImageView()
    {
        VkImageViewCreateInfo createInfo       = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        createInfo.image                       = m_image;
        createInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format                      = m_format;
        createInfo.subresourceRange.aspectMask = m_aspectMask;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.layerCount = 1;

        auto result = vkCreateImageView(m_context->device.GetLogical(), &createInfo, m_context->allocator, &m_imageView);
        if (!VulkanUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create Image view with error: '{}'.", VulkanUtils::ResultString(result));
            return false;
        }

        return true;
    }

    bool VulkanTexture::AllocateAndBind()
    {
        auto device = m_context->device.GetLogical();

        VkMemoryRequirements memoryRequirements;
        vkGetImageMemoryRequirements(device, m_image, &memoryRequirements);

        u32 memoryTypeIndex = m_context->device.SelectMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memoryTypeIndex == INVALID_ID)
        {
            ERROR_LOG("Failed to get Device memory type index.");
            return false;
        }

        VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocateInfo.allocationSize       = memoryRequirements.size;
        allocateInfo.memoryTypeIndex      = memoryTypeIndex;

        auto result = vkAllocateMemory(device, &allocateInfo, m_context->allocator, &m_memory);
        if (!VulkanUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to allocate memory with error: '{}'.", VulkanUtils::ResultString(result));
            return false;
        }

        result = vkBindImageMemory(device, m_image, m_memory, 0);
        if (!VulkanUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to bind memory with error: '{}'.", VulkanUtils::ResultString(result));
            return false;
        }

        return true;
    }

    void VulkanTexture::Destroy()
    {
        INFO_LOG("Destroying: '{}'.", m_name);

        auto device = m_context->device.GetLogical();

        if (m_imageView)
        {
            vkDestroyImageView(device, m_imageView, m_context->allocator);
            m_imageView = nullptr;
        }

        if (m_image)
        {
            vkDestroyImage(device, m_image, m_context->allocator);
            m_image = nullptr;
        }

        if (m_memory)
        {
            vkFreeMemory(device, m_memory, m_context->allocator);
            m_memory = nullptr;
        }
    }
}  // namespace C3D