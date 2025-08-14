
#pragma once
#include "base_allocator.h"
#include "defines.h"

namespace C3D
{
    struct MallocAllocation
    {
        MemoryType type;
        u64 size = 0;
    };

    class C3D_API MallocAllocator final : public BaseAllocator<MallocAllocator>
    {
    public:
        MallocAllocator();

        MallocAllocator(const MallocAllocator&) = delete;
        MallocAllocator(MallocAllocator&&)      = delete;

        MallocAllocator& operator=(const MallocAllocator&) = delete;
        MallocAllocator& operator=(MallocAllocator&&)      = delete;

        ~MallocAllocator() override;

        void* AllocateBlock(MemoryType type, u64 size, u16 alignment = 1) const override;
        void Free(void* block) const override;

        static MallocAllocator* GetDefault();
    };
}  // namespace C3D
