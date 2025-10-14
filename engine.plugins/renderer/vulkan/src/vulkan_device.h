
#pragma once
#include <containers/dynamic_array.h>
#include <defines.h>
#include <volk.h>

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

    enum PhysicalDeviceSupportFlag : u8
    {
        PHYSICAL_DEVICE_SUPPORT_NONE                   = 0x0,
        PHYSICAL_DEVICE_SUPPORT_FLAG_MESH_SHADING      = 0x1,
        PHYSICAL_DEVICE_SUPPORT_FLAG_PUSH_DESCRIPTORS  = 0x2,
        PHYSICAL_DEVICE_SUPPORT_FLAG_PERFORMANCE_QUERY = 0x4,
    };
    using PhysicalDeviceSupportFlags = u8;

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
        /** @brief Flags indicating different supported features. */
        PhysicalDeviceSupportFlags supportFlags;
    };

    /** @brief A struct containing everything relevant to Vulkan's abstraction over the device. */
    struct LogicalDevice
    {
        /** @brief A handle to Vulkan's abstraction over the device. */
        VkDevice handle = nullptr;
        /** @brief A handle to the device queue. */
        VkQueue queue = nullptr;
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

        /** @brief Returns the preferred surface format. */
        VkSurfaceFormatKHR GetPreferredSurfaceFormat() const;

        /** @brief Returns the preferred image format. */
        VkFormat GetPreferredImageFormat() const;

        VkDevice GetLogical() const { return m_logical.handle; }
        VkPhysicalDevice GetPhysical() const { return m_physical.handle; }

        u32 GetGraphicsFamilyIndex() const { return m_physical.graphicsQueueFamilyIndex; }
        VkPhysicalDeviceProperties GetProperties() const { return m_physical.properties; }
        VkPhysicalDeviceMemoryProperties GetMemoryProperties() const { return m_physical.memory; }

        const DynamicArray<VkPresentModeKHR>& GetPresentModes() const { return m_physical.swapchainSupportInfo.presentModes; }
        const VkSurfaceCapabilitiesKHR& GetSurfaceCapabilities() const { return m_physical.swapchainSupportInfo.capabilities; }

        bool IsFeatureSupported(PhysicalDeviceSupportFlag flag) const { return m_physical.supportFlags & flag; }

        /** @brief Gets a handle to the device queue. */
        VkQueue GetDeviceQueue() const { return m_logical.queue; }

    private:
        /** @brief Checks that the device supports all requirements.*/
        bool DeviceSupportsMandatoryRequirements(VkPhysicalDevice device, const DynamicArray<const char*>& requiredExtensions);

        /** @brief Selects the ideal physical GPU present on the system. */
        bool SelectPhyiscalDevice(const DynamicArray<const char*>& requiredExtensions);

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