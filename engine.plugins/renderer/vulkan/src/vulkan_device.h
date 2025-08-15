
#pragma once
#include <containers/dynamic_array.h>
#include <defines.h>
#include <vulkan/vulkan_core.h>

namespace C3D
{
    struct VulkanContext;

    /** @brief Struct storing the supported swapchain/surface formats, present modes and capabilities. */
    struct VulkanSwapchainSupportInfo
    {
        VkSurfaceCapabilitiesKHR capabilities;
        DynamicArray<VkSurfaceFormatKHR> formats;
        DynamicArray<VkPresentModeKHR> presentModes;
    };

    /** @brief A struct containing everything relevant to the physical device (actual GPU) */
    struct PhysicalDevice
    {
        /** @brief A handle to the physical device. */
        VkPhysicalDevice handle = nullptr;
        /** @brief A struct holding the physical device's properties */
        VkPhysicalDeviceProperties properties;
        /** @brief A struct holding the physical device's memory properties. */
        VkPhysicalDeviceMemoryProperties memory;
        /** @brief The index of the graphics queue family. */
        u32 graphicsQueueFamilyIndex = INVALID_ID;
        /** @brief The swapchain support info. */
        VulkanSwapchainSupportInfo swapchainSupportInfo;
    };

    /** @brief A struct containing everything relevant to Vulkan's abstraction over the device. */
    struct LogicalDevice
    {
        /** @brief A handle to Vulkan's abstraction over the device. */
        VkDevice handle = nullptr;
        /** @brief A handle to the device queue. */
        VkQueue queue = nullptr;
        /** @brief A handle to the command pool. */
        VkCommandPool commandPool = nullptr;
    };

    class VulkanDevice
    {
    public:
        VulkanDevice() = default;

        /** @brief There should be only one Vulkan device so we delete the copy constructor. */
        VulkanDevice(const VulkanDevice&) = delete;

        /** @brief Creates the vulkan device. */
        bool Create(VulkanContext* context);

        /** @brief Query the device to get the supported swapchain/surface capabilities  */
        void QuerySwapchainSupport(VkSurfaceKHR surface);

        /**
         * @brief Selects the approriate memory index for the provided memory type that satisfies the provided flags
         *
         * @param memoryTypeBits The type of memory that you want to use
         * @param flags Flags corresponding to the properties that this memory needs to have
         * @return The index to the memory that was requested
         */
        u32 SelectMemoryType(u32 memoryTypeBits, VkMemoryPropertyFlags flags) const;

        /** @brief Destroys the vulkan device. */
        void Destroy();

        /** @brief Waits for the device to go idle. */
        VkResult WaitIdle() const;

        VkDevice GetLogical() const { return m_logical.handle; }
        VkPhysicalDevice GetPhysical() const { return m_physical.handle; }

        u32 GetGraphicsFamilyIndex() const { return m_physical.graphicsQueueFamilyIndex; }
        VkPhysicalDeviceMemoryProperties GetMemoryProperties() const { return m_physical.memory; }

        const DynamicArray<VkSurfaceFormatKHR>& GetSurfaceFormats() const { return m_physical.swapchainSupportInfo.formats; }
        const DynamicArray<VkPresentModeKHR>& GetPresentModes() const { return m_physical.swapchainSupportInfo.presentModes; }
        const VkSurfaceCapabilitiesKHR& GetSurfaceCapabilities() const { return m_physical.swapchainSupportInfo.capabilities; }

        /** @brief Gets a handle to the device queue. */
        VkQueue GetDeviceQueue() const { return m_logical.queue; }

        /** @brief Gets a handle to the command pool. */
        VkCommandPool GetCommandPool() const { return m_logical.commandPool; }

    private:
        /** @brief Selects the ideal physical GPU present on the system. */
        bool SelectPhyiscalDevice();

        /** @brief Finds the index of the graphics queue family. */
        u32 SelectGraphicsFamilyIndex(VkPhysicalDevice handle);

    private:
        /** @brief The physical device */
        PhysicalDevice m_physical;
        /** @brief The logical device */
        LogicalDevice m_logical;

        /** @brief A pointer to our Vulkan context. */
        VulkanContext* m_context = nullptr;
    };
}  // namespace C3D