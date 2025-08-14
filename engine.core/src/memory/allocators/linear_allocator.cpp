
#include "linear_allocator.h"

#include "logger/logger.h"
#include "memory/global_memory_system.h"
#include "metrics/metrics.h"

namespace C3D
{
    LinearAllocator::LinearAllocator() : BaseAllocator(ToUnderlying(AllocatorType::Linear)) {}

    void LinearAllocator::Create(const char* name, const u64 totalSize, void* memory)
    {
        m_totalSize  = totalSize;
        m_allocated  = 0;
        m_ownsMemory = memory == nullptr;

        // The memory already exists thus it's owned by someone else
        if (memory)
        {
            m_memoryBlock = static_cast<char*>(memory);
        }
        else
        {
            // We need to allocate the memory ourselves
            m_memoryBlock = Memory.Allocate<char>(MemoryType::LinearAllocator, totalSize);
        }

        // Create an metrics object to track the allocations this linear allocator is doing
        m_id = Metrics.CreateAllocator(name, AllocatorType::Linear, totalSize);
    }

    void LinearAllocator::Destroy()
    {
        INFO_LOG("Destroying.");

        // First we free all our memory
        FreeAll();
        // Then if we actually own the memory block we free the block
        if (m_ownsMemory && m_memoryBlock)
        {
            // We own the memory so let's free it
            Memory.Free(m_memoryBlock);
        }
        // Destroy the metrics object associated with this allocator
        Metrics.DestroyAllocator(m_id, false);

        m_memoryBlock = nullptr;
        m_totalSize   = 0;
        m_ownsMemory  = false;
    }

    void* LinearAllocator::AllocateBlock(const MemoryType type, const u64 size, u16 alignment) const
    {
        if (m_memoryBlock)
        {
            if (m_allocated + size > m_totalSize)
            {
                throw std::bad_alloc();
            }

            char* block = m_memoryBlock + m_allocated;
            m_allocated += size;

            MetricsAllocate(m_id, type, size, size, block);

            std::memset(block, 0, size);
            return block;
        }

        ERROR_LOG("Not initialized.");
        return nullptr;
    }

    void LinearAllocator::Free(void* block) const {}

    void LinearAllocator::FreeAll()
    {
        if (m_memoryBlock)
        {
            m_allocated = 0;
            std::memset(m_memoryBlock, 0, m_totalSize);

            // Ensure that the metrics keep track of the fact that we just freed all memory for this allocator
            Metrics.FreeAll(m_id);
        }
    }

    LinearAllocator* LinearAllocator::GetDefault()
    {
        static auto allocator = new LinearAllocator();
        return allocator;
    }
}  // namespace C3D
