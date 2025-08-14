
#pragma once
#include "base_allocator.h"
#include "defines.h"
#include "logger/logger.h"
#include "memory/free_list.h"

namespace C3D
{
    typedef u32 AllocSizeMarker;

    /** @brief The allocation footer that gets added to every allocation. */
    struct AllocFooter
    {
        /** @brief A pointer to where this allocations starts.
         * We can use this to calculate the offset into the memory block that is managed by the dynamic allocator.
         */
        void* start = nullptr;
        /** @brief The alignment that needs to be used for the user's memory. */
        u16 alignment = 0;
        /** @brief The memory type of the user's memory. (used to keep track of where allocations are coming from) */
        MemoryType type = MemoryType::Unknown;
    };

    constexpr auto MAX_SINGLE_ALLOC_SIZE        = GibiBytes(4);
    constexpr auto SMALLEST_POSSIBLE_ALLOCATION = sizeof(AllocFooter) + sizeof(AllocSizeMarker) + 1 + 1;

    class C3D_API DynamicAllocator final : public BaseAllocator<DynamicAllocator>
    {
    public:
        DynamicAllocator(AllocatorType type = AllocatorType::Dynamic);

        bool Create(void* memory, u64 totalMemory, u64 usableMemory);

        bool Destroy();

        void* AllocateBlock(MemoryType type, u64 size, u16 alignment = 1) const override;

        void Free(void* block) const override;

        /** @brief Obtains the size and alignment of a given block of memory.
         * Will fail if the provided block of memory is invalid.
         */
        bool GetSizeAlignment(void* block, u64* outSize, u16* outAlignment) const override;

        /** @brief Obtains the alignment of a given block of memory.
         * Will fail if the provided block of memory is invalid.
         */
        bool GetAlignment(void* block, u16* outAlignment) const override;
        bool GetAlignment(const void* block, u16* outAlignment) const override;

        u64 FreeSpace() const;
        u64 GetTotalUsableSize() const;

        static constexpr u64 GetMemoryRequirements(u64 usableSize);

        static DynamicAllocator* GetDefault();

    private:
        bool m_initialized = false;
        // The total size including our freelist
        u64 m_totalSize = 0;
        // The size of usable memory
        u64 m_memorySize = 0;
        // The freelist to keep track of all the free blocks of memory
        FreeList m_freeList;
        // A pointer to the actual block of memory that this allocator manages
        char* m_memory = nullptr;
        // A mutex to ensure allocations happen in a thread-safe manor
        mutable std::mutex m_mutex;
    };

    constexpr u64 DynamicAllocator::GetMemoryRequirements(const u64 usableSize)
    {
        return FreeList::GetMemoryRequirement(usableSize, SMALLEST_POSSIBLE_ALLOCATION) + usableSize;
    }
}  // namespace C3D
