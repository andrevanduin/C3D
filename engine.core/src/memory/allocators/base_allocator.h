
#pragma once
#include <new>

#include "defines.h"
#include "metrics/metrics.h"
#include "metrics/types.h"

namespace C3D
{
    template <class Derived>
    class C3D_API BaseAllocator
    {
    public:
        explicit BaseAllocator(const u8 type) : m_type(type) {}

        BaseAllocator(const BaseAllocator&) = delete;
        BaseAllocator(BaseAllocator&&)      = delete;

        BaseAllocator& operator=(const BaseAllocator&) = delete;
        BaseAllocator& operator=(BaseAllocator&&)      = delete;

        virtual ~BaseAllocator() = default;

        virtual void* AllocateBlock(MemoryType type, u64 size, u16 alignment = 1) const = 0;
        virtual void Free(void* block) const                                            = 0;

        virtual bool GetSizeAlignment(void* block, u64* outSize, u16* outAlignment) const { return true; }

        virtual bool GetAlignment(void* block, u16* outAlignment) const { return true; }
        virtual bool GetAlignment(const void* block, u16* outAlignment) const { return true; }

        template <typename T>
        C3D_INLINE T* Allocate(const MemoryType type, const u64 count = 1) const
        {
            return static_cast<T*>(AllocateBlock(type, sizeof(T) * count, alignof(T)));
        }

        template <class T>
        C3D_INLINE T* New(const MemoryType type) const
        {
            auto block = AllocateBlock(type, sizeof(T), alignof(T));
            return new (block) T();
        }

        template <class T, class... Args>
        C3D_INLINE T* New(const MemoryType type, Args&&... args) const
        {
            auto block = AllocateBlock(type, sizeof(T), alignof(T));
            return new (block) T(std::forward<Args>(args)...);
        }

        template <class T>
        void Delete(T* instance) const
        {
            // Call our destructor
            instance->~T();
            // Free our memory
            Free(instance);
        }

        template <class T>
        C3D_INLINE T* NewArray(const MemoryType type, const u64 count) const
        {
            T* elements = static_cast<T*>(AllocateBlock(type, sizeof(T) * count, alignof(T)));
            std::uninitialized_default_construct_n(elements, count);
            return elements;
        }

        template <class T>
        void DeleteArray(T* array, const u64 count) const
        {
            std::destroy_n(array, count);
            Free(array);
        }

        [[nodiscard]] void* GetMemory() const { return m_memoryBlock; }

        [[nodiscard]] u8 GetId() const { return m_id; }

        static Derived* GetDefault() { return Derived::GetDefault(); }

    protected:
        /** @brief The id for this allocator */
        u8 m_id = INVALID_ID_U8;
        /** @brief The type of this allocator */
        u8 m_type = INVALID_ID_U8;
        /** @brief The block of memory */
        char* m_memoryBlock = nullptr;
    };
}  // namespace C3D
