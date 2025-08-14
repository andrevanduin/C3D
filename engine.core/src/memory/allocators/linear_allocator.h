
#pragma once
#include "base_allocator.h"
#include "defines.h"

namespace C3D
{
    class C3D_API LinearAllocator final : public BaseAllocator<LinearAllocator>
    {
    public:
        LinearAllocator();

        void Create(const char* name, u64 totalSize, void* memory = nullptr);
        void Destroy();

        void* AllocateBlock(MemoryType type, u64 size, u16 alignment = 1) const override;
        void Free(void* block) const override;

        void FreeAll();

        [[nodiscard]] u64 GetTotalSize() const { return m_totalSize; }
        [[nodiscard]] u64 GetAllocated() const { return m_allocated; }

        [[nodiscard]] bool OwnsMemory() const { return m_ownsMemory; }

        static LinearAllocator* GetDefault();

    private:
        u64 m_totalSize         = 0;
        mutable u64 m_allocated = 0;

        bool m_ownsMemory = false;
    };
}  // namespace C3D
