
#include "malloc_allocator.h"

#include "metrics/metrics.h"

namespace C3D
{
    MallocAllocator::MallocAllocator() : BaseAllocator(ToUnderlying(AllocatorType::Malloc))
    {
        m_id = Metrics.CreateAllocator("MALLOC_ALLOCATOR", AllocatorType::Malloc, 0);
    }

    MallocAllocator::~MallocAllocator() { Metrics.DestroyAllocator(m_id); }

    void* MallocAllocator::AllocateBlock(const MemoryType type, const u64 size, u16 alignment) const
    {
        // Allocate the block with malloc
        const auto block = std::malloc(size);
        // Zero out the memory
        std::memset(block, 0, size);
        return block;
    }

    void MallocAllocator::Free(void* block) const
    {
        // Free the block
        std::free(block);
    }

    MallocAllocator* MallocAllocator::GetDefault()
    {
        static auto allocator = MallocAllocator();
        return &allocator;
    }
}  // namespace C3D
