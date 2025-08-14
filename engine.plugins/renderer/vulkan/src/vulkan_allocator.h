
#pragma once
#include <vulkan/vulkan.h>

namespace C3D
{
    namespace VulkanAllocator
    {
#ifdef C3D_VULKAN_USE_CUSTOM_ALLOCATOR
        /*
         * @brief Implementation of PFN_vkAllocationFunction
         * @link https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/PFN_vkAllocationFunction.html
         */
        void* Allocate(void* userData, const size_t size, const size_t alignment, const VkSystemAllocationScope allocationScope);

        /*
         * @brief Implementation of PFN_vkFreeFunction
         * @link https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/PFN_vkFreeFunction.html
         */
        void Free(void* userData, void* memory);

        /*
         * @brief Implementation of PFN_vkReallocationFunction
         * @link https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/PFN_vkReallocationFunction.html
         */
        void* Reallocate(void* userData, void* original, const size_t size, const size_t alignment,
                         const VkSystemAllocationScope allocationScope);

        /*
         * @brief Implementation of PFN_vkReallocationFunction
         * @link https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/PFN_vkInternalAllocationNotification.html
         */
        void InternalAllocation(void* userData, size_t size, VkInternalAllocationType allocationType,
                                VkSystemAllocationScope allocationScope);

        /*
         * @brief Implementation of PFN_vkReallocationFunction
         * @link https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/PFN_vkInternalAllocationNotification.html
         */
        void InternalFree(void* userData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);

        bool Create(VkAllocationCallbacks* callbacks);
#endif
    }  // namespace VulkanAllocator

}  // namespace C3D