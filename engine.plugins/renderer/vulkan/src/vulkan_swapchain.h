
#pragma once
#include <containers/dynamic_array.h>
#include <defines.h>
#include <vulkan/vulkan_core.h>

namespace C3D
{
    struct VulkanContext;
    struct Window;
    struct WindowRendererBackendState;

    class VulkanSwapchain
    {
    public:
        /**
         * @brief Creates a swapchain for the provided window.
         *
         * @param context A pointer to the vulkan context
         * @param window The window that you want to create a swapchain for
         * @return True if successful; false otherwise
         */
        bool Create(VulkanContext* context, Window& window);

        /**
         * @brief Resize the existing swapchain (for example when the window size changes)
         *
         * @param window The window that has (potentially) resized
         * @return True if successful; false otherwise
         */
        bool Resize(Window& window);

        void Destroy();

        bool AcquireNextImageIndex(u64 timeoutNs, WindowRendererBackendState* backendState);
        bool Present(VkQueue presentQueue, WindowRendererBackendState* backendState);

        VkSwapchainKHR GetHandle() const { return m_handle; }

        VkImage GetImage(u32 index) const { return m_images[index]; }
        VkImageView GetImageView(u32 index) const { return m_views[index]; }

        u32 GetImageCount() const { return m_imageCount; }

        VkFormat GetImageFormat() const { return m_surfaceFormat.format; }

    private:
        /**
         * @brief Internal create method. Can be called for the first creation call or for resizing.
         * If the method is called after we already have a swapchain this method will recreate and destroy the old one
         */
        void Create(Window& window);

        VkPresentModeKHR GetPresentMode() const;
        VkSurfaceFormatKHR GetSurfaceFormat() const;

        /** @brief The number of images in the swapchain. */
        u32 m_imageCount = 0;
        /** @brief The swapchain images. */
        DynamicArray<VkImage> m_images;
        /** @brief The swapchain image views. */
        DynamicArray<VkImageView> m_views;
        /** @brief The currently used surface format. */
        VkSurfaceFormatKHR m_surfaceFormat;
        /** @brief A handle to the Vulkan swapchain. */
        VkSwapchainKHR m_handle = nullptr;
        /** @brief A pointer to our Vulkan context. */
        VulkanContext* m_context = nullptr;
    };
}  // namespace C3D