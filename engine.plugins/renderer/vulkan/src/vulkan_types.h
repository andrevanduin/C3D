
#pragma once
#include <containers/dynamic_array.h>
#include <defines.h>
#include <volk.h>

#include "vulkan_swapchain.h"
#include "vulkan_texture.h"

namespace C3D
{
    /** @brief The maximum number of frames in flight. */
    constexpr u32 MAX_FRAMES = 2;

    /** @brief The minimum number of images in flight. */
    constexpr u32 MIN_IMAGES = 3;

#define VK_CHECK_SWAPCHAIN(call, msg)                                                                                      \
    {                                                                                                                      \
        VkResult result_ = call;                                                                                           \
        C3D_ASSERT_MSG(result_ == VK_SUCCESS || result_ == VK_SUBOPTIMAL_KHR || result_ == VK_ERROR_OUT_OF_DATE_KHR, msg); \
    }

    /** @brief A structure to hold the current Vulkan API version. */
    struct VulkanAPIVersion
    {
        u32 major = 0;
        u32 minor = 0;
        u32 patch = 0;
    };

    struct WindowRendererBackendState
    {
        /** @brief The internal Vulkan surface that we should draw on. */
        VkSurfaceKHR surface;
        /** @brief The swapchain that belongs to this window. */
        VulkanSwapchain swapchain;
        /** @brief Semaphores used to wait before the command buffers for this batch begin execution. */
        VkSemaphore acquireSemaphores[MAX_FRAMES];
        /** @brief Semaphore used to wait before presenting. */
        DynamicArray<VkSemaphore> presentSemaphores;
        /** @brief A fence which gets signaled once all submitted command buffers have completed execution. */
        VkFence fences[MAX_FRAMES];
        /** @brief Command Pools used to allocate command buffers. */
        VkCommandPool commandPools[MAX_FRAMES];
        /** @brief Command buffers used to store commands to be submitted to a queue for execution. */
        VkCommandBuffer commandBuffers[MAX_FRAMES];

        /** @brief The current image index as retrieved by vkAcquireNextImageKHR. */
        u32 imageIndex = 0;
        /** @brief The current frame index. */
        u64 frameIndex = 0;

        /** @brief The color target of our window. */
        VulkanTexture colorTarget;
        /** @brief The depth target of our window. */
        VulkanTexture depthTarget;

        VkSemaphore GetAcquireSemaphore() const { return acquireSemaphores[frameIndex % MAX_FRAMES]; }
        VkSemaphore GetPresentSemaphore() const { return presentSemaphores[imageIndex]; }
        VkFence GetFence() const { return fences[frameIndex % MAX_FRAMES]; }

        VkCommandPool GetCommandPool() const { return commandPools[frameIndex % MAX_FRAMES]; }
        VkCommandBuffer GetCommandBuffer() const { return commandBuffers[frameIndex % MAX_FRAMES]; }
    };

    struct alignas(16) MeshDraw
    {
        vec3 position;
        f32 scale;
        quat orientation;

        union {
            u32 commandData[8];

            struct
            {
                VkDrawIndexedIndirectCommand commandIndirect;         // 5 u32's
                VkDrawMeshTasksIndirectCommandEXT commandIndirectMS;  // 3 u32's
            };
        };
    };
}  // namespace C3D