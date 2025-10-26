
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
        /** @brief The depth pyramid of our window. */
        VulkanTexture depthPyramid;

        VkSemaphore GetAcquireSemaphore() const { return acquireSemaphores[frameIndex % MAX_FRAMES]; }
        VkSemaphore GetPresentSemaphore() const { return presentSemaphores[imageIndex]; }
        VkFence GetFence() const { return fences[frameIndex % MAX_FRAMES]; }

        VkCommandPool GetCommandPool() const { return commandPools[frameIndex % MAX_FRAMES]; }
        VkCommandBuffer GetCommandBuffer() const { return commandBuffers[frameIndex % MAX_FRAMES]; }
    };

    struct MeshDrawCommand
    {
        u32 drawId;
        VkDrawIndexedIndirectCommand indirect;
    };

    struct MeshTaskCommand
    {
        u32 drawId;
        u32 taskOffset;
        u32 taskCount;
        u32 lateDrawVisibility;
        u32 meshletVisibilityOffset;
    };

    /** @brief Data used for culling in the shaders. */
    struct alignas(16) CullData
    {
        mat4 view;

        f32 p00, p11, zNear, zFar;        // Symmertric projection parameters
        f32 frustum[4];                   // Data for left/right/top/bottom planes
        f32 lodTarget;                    // Lod target error at z=1
        f32 pyramidWidth, pyramidHeight;  // Depth pyramid size in texels

        u32 drawCount = 0;

        i32 cullingEnabled                 = 0;
        i32 occlusionCullingEnabled        = 0;
        i32 clusterOcclusionCullingEnabled = 0;
        i32 meshShadingEnabled             = 0;
        i32 lodEnabled                     = 0;
    };

    /** @brief The Globals structure that is uploaded to the GPU once per frame.  */
    struct alignas(16) Globals
    {
        mat4 projection;
        CullData cullData;
        f32 screenWidth, screenHeight;
    };

    struct alignas(16) DepthReduceData
    {
        vec2 imageSize;
    };

}  // namespace C3D