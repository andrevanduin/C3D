
#pragma once
#ifdef C3D_MEMORY_METRICS_STACKTRACE
#include <stacktrace>
#endif

#include "containers/array.h"
#include "defines.h"
#include "frame_data.h"
#include "types.h"

namespace C3D
{
#define Metrics C3D::MetricSystem::GetInstance()

#ifdef C3D_MEMORY_METRICS
#ifdef C3D_MEMORY_METRICS_POINTERS
#define MetricsAllocate(id, type, requested, required, ptr) Metrics.Allocate(id, Allocation(type, ptr, requested, required))

#define MetricsFree(id, type, requested, required, ptr) Metrics.Free(id, DeAllocation(type, ptr))
#else
#define MetricsAllocate(id, type, requested, required, ptr) Metrics.Allocate(id, Allocation(type, requested, required))

#define MetricsFree(id, type, requested, required, ptr) Metrics.Free(id, DeAllocation(type, requested, required))
#endif
#else
#define MetricsAllocate(id, type, requested, required, ptr)
#define MetricsFree(id, type, requested, required, ptr)
#endif

    constexpr auto METRICS_COUNT = 16;

    constexpr u8 DYNAMIC_ALLOCATOR_ID = 0;
    constexpr u8 GPU_ALLOCATOR_ID     = 1;

    struct Clocks;

    class C3D_API MetricSystem
    {
        static MetricSystem* s_instance;

    public:
        MetricSystem();

        void Init();

        void Update(FrameData& frameData, Clocks& clocks);

        /*
         * @brief Creates an internal metrics object used for tracking allocators
         * Returns an u8 id that is associated with this specific allocator
         */
        u8 CreateAllocator(const char* name, AllocatorType type, u64 availableSpace);
        /*
         * @brief Destroys the internal metrics object used for tracking allocators
         * that is associated with the provided allocatorId
         */
        void DestroyAllocator(u8 allocatorId, bool printMissedAllocs = true);

        void Allocate(u8 allocatorId, const Allocation& a);

        void AllocateExternal(u64 size);

        void Free(u8 allocatorId, const DeAllocation& a);

        void FreeExternal(u64 size);

        void FreeAll(u8 allocatorId);

        void SetAllocatorAvailableSpace(u8 allocatorId, u64 availableSpace);

        [[nodiscard]] u64 GetAllocCount(u8 allocatorId = 0) const;

        [[nodiscard]] u64 GetAllocCount(MemoryType memoryType, u8 allocatorId = 0) const;

        [[nodiscard]] u64 GetMemoryUsage(MemoryType memoryType, u8 allocatorId = DYNAMIC_ALLOCATOR_ID) const;

        [[nodiscard]] u64 GetRequestedMemoryUsage(MemoryType memoryType, u8 allocatorId = DYNAMIC_ALLOCATOR_ID) const;

        void PrintMemoryUsage(u8 allocatorId, bool debugLines);
        void PrintMemoryUsage(bool debugLines = false);

        static MetricSystem& GetInstance();

        [[nodiscard]] u16 GetFps() const { return m_fps; }

    private:
        static const char* SizeToText(u64 size, f64* outAmount);
        static void SprintfAllocation(const MemoryAllocations& allocation, int index, char* buffer, int& bytesWritten, int offset, bool debugLines);

        f64 m_accumulatedTime = 0.0;

        u16 m_counter = 0;
        u16 m_fps     = 0;

        // The memory stats for all our different allocators
        Array<MemoryStats, METRICS_COUNT> m_memoryStats;
        // Keep track of the external allocations that we have no control over
        ExternalAllocations m_externalAllocations;
    };
}  // namespace C3D
