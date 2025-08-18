
#include "metrics.h"

#include "logger/logger.h"
#include "math/c3d_math.h"
#include "time/clock.h"

namespace C3D
{
    static const char* MEMORY_TYPE_STRINGS[static_cast<u8>(MemoryType::MaxType)] = { "Unknown",
                                                                                     "Dynamic_Allocator",
                                                                                     "Linear_Allocator",
                                                                                     "FreeList",
                                                                                     "Array",
                                                                                     "DynamicArray",
                                                                                     "Stack",
                                                                                     "HashTable",
                                                                                     "HashMap",
                                                                                     "HashSet",
                                                                                     "Queue",
                                                                                     "RingQueue",
                                                                                     "Bst",
                                                                                     "String",
                                                                                     "C3DString",
                                                                                     "Engine",
                                                                                     "Platform",
                                                                                     "AssetManager",
                                                                                     "Event",
                                                                                     "Job",
                                                                                     "Texture",
                                                                                     "MaterialInstance",
                                                                                     "Mesh",
                                                                                     "CoreSystem",
                                                                                     "RenderSystem",
                                                                                     "RenderData",
                                                                                     "RenderView",
                                                                                     "Game",
                                                                                     "Transform",
                                                                                     "Entity",
                                                                                     "EntityNode",
                                                                                     "Scene",
                                                                                     "CVar",
                                                                                     "Shader",
                                                                                     "Asset",
                                                                                     "Vulkan",
                                                                                     "VulkanExternal",
                                                                                     "Direct3D",
                                                                                     "OpenGL",
                                                                                     "Audio",
                                                                                     "BitmapFont",
                                                                                     "SystemFont",
                                                                                     "Terrain",
                                                                                     "Test",
                                                                                     "DebugConsole",
                                                                                     "DebugRendering",
                                                                                     "Command",
                                                                                     "UI2D",
                                                                                     "ECS" };

    MetricSystem* MetricSystem::s_instance;

    MetricSystem::MetricSystem() = default;

    void MetricSystem::Init()
    {
        // Invalidate all stats
        for (auto& stats : m_memoryStats)
        {
            stats.type = AllocatorType::None;
        }

        m_memoryStats[GPU_ALLOCATOR_ID].type     = AllocatorType::GpuLocal;
        m_memoryStats[GPU_ALLOCATOR_ID].name     = "GPU_ALLOCATOR";
        m_memoryStats[DYNAMIC_ALLOCATOR_ID].type = AllocatorType::Dynamic;

        // NOTE: We can't log at this stage yet using our logger since it's too early in the startup process
    }

    void MetricSystem::Update(FrameData& frameData, Clocks& clocks)
    {
        // Calculate ms per frame average
        m_accumulatedTime += frameData.timeData.delta;
        if (m_accumulatedTime >= 1.0)
        {
            // At least 1 second has passed
            frameData.timeData.avgPrepareFrameTimeMs = (clocks.prepareFrame.GetTotalElapsed() / m_counter) * C3D::SEC_TO_MS_MULTIPLIER;
            frameData.timeData.avgPresentTimeMs      = (clocks.present.GetTotalElapsed() / m_counter) * C3D::SEC_TO_MS_MULTIPLIER;
            frameData.timeData.avgRenderTimeMs       = (clocks.onRender.GetTotalElapsed() / m_counter) * C3D::SEC_TO_MS_MULTIPLIER;
            frameData.timeData.avgUpdateTimeMs       = (clocks.onUpdate.GetTotalElapsed() / m_counter) * C3D::SEC_TO_MS_MULTIPLIER;
            frameData.timeData.avgRunTimeMs          = (clocks.total.GetTotalElapsed() / m_counter) * C3D::SEC_TO_MS_MULTIPLIER;

            clocks.prepareFrame.ResetTotal();
            clocks.present.ResetTotal();
            clocks.onRender.ResetTotal();
            clocks.onUpdate.ResetTotal();
            clocks.total.ResetTotal();

            m_fps             = m_counter;
            m_counter         = 0;
            m_accumulatedTime = 0;
        }

        m_counter++;
    }

    u8 SetMemoryStats(const char* name, MemoryStats& stats, const AllocatorType type, const u64 availableSpace, const u8 i)
    {
        stats.type                = type;
        stats.totalAvailableSpace = availableSpace;
        stats.name                = name;
        // Return the index into our array as an id
        return i;
    }

    u8 MetricSystem::CreateAllocator(const char* name, const AllocatorType type, const u64 availableSpace)
    {
        if (std::strlen(name) > ALLOCATOR_NAME_MAX_LENGTH)
        {
            FATAL_LOG("Allocator name: '{}' should <= {} characters", name, ALLOCATOR_NAME_MAX_LENGTH);
        }

        if (type == AllocatorType::GlobalDynamic)
        {
            return SetMemoryStats(name, m_memoryStats[DYNAMIC_ALLOCATOR_ID], type, availableSpace, DYNAMIC_ALLOCATOR_ID);
        }
        if (type == AllocatorType::GpuLocal)
        {
            return SetMemoryStats(name, m_memoryStats[GPU_ALLOCATOR_ID], type, availableSpace, GPU_ALLOCATOR_ID);
        }

        for (u8 i = 0; i < METRICS_COUNT; i++)
        {
            auto& stats = m_memoryStats[i];
            if (stats.type == AllocatorType::None)
            {
                // Return the index into our array as an id
                return SetMemoryStats(name, stats, type, availableSpace, i);
            }
        }

        // If we got to this point we have no more space for metrics so we return an error
        FATAL_LOG("Not enough space for Allocator metrics");
        return INVALID_ID_U8;
    }

    void MetricSystem::DestroyAllocator(const u8 allocatorId, bool printMissedAllocs)
    {
        // Print the memory usage for this allocator
        PrintMemoryUsage(allocatorId, true);
        // Clear out the metrics we have on this allocator
        m_memoryStats[allocatorId] = MemoryStats();
    }

    void MetricSystem::Allocate(const u8 allocatorId, const Allocation& a)
    {
        auto& stats     = m_memoryStats[allocatorId];
        const auto type = ToUnderlying(a.type);

        stats.allocCount++;
        stats.totalRequested += a.requestedSize;
        stats.totalRequired += a.requiredSize;

#ifdef C3D_MEMORY_METRICS_POINTERS
        auto& tagged = stats.taggedAllocations[type];
        if (!a.ptr)
        {
            tagged.count++;
            tagged.requestedSize += a.requestedSize;
            tagged.requiredSize += a.requiredSize;
        }
        else
        {
            auto current = std::stacktrace::current(1, 10);
            tagged.allocations.emplace_back(a.ptr, std::to_string(current), a.requestedSize, a.requiredSize);
        }
#else
        stats.taggedAllocations[type].requestedSize += a.requestedSize;
        stats.taggedAllocations[type].requiredSize += a.requiredSize;
        stats.taggedAllocations[type].count++;
#endif
    }

    void MetricSystem::AllocateExternal(const u64 size)
    {
        m_externalAllocations.count++;
        m_externalAllocations.size += size;
    }

    void MetricSystem::Free(const u8 allocatorId, const DeAllocation& a)
    {
        auto& stats     = m_memoryStats[allocatorId];
        const auto type = ToUnderlying(a.type);

        stats.allocCount--;

#ifdef C3D_MEMORY_METRICS_POINTERS
        auto& tagged = stats.taggedAllocations[type];

        const auto allocIt = std::find_if(tagged.allocations.begin(), tagged.allocations.end(), [a](const TrackedAllocation& t) { return t.ptr == a.ptr; });
        if (allocIt != tagged.allocations.end())
        {
            const auto alloc = *allocIt;

            stats.totalRequested -= alloc.requestedSize;
            stats.totalRequired -= alloc.requiredSize;

            tagged.allocations.erase(allocIt);
        }
        else
        {
            throw std::bad_alloc();
        }
#else
        stats.totalRequested -= a.requestedSize;
        stats.totalRequired -= a.requiredSize;

        stats.taggedAllocations[type].requestedSize -= a.requestedSize;
        stats.taggedAllocations[type].requiredSize -= a.requiredSize;
        stats.taggedAllocations[type].count--;
#endif
    }

    void MetricSystem::FreeExternal(const u64 size)
    {
        m_externalAllocations.count--;
        m_externalAllocations.size -= size;
    }

    void MetricSystem::FreeAll(const u8 allocatorId)
    {
        auto& stats          = m_memoryStats[allocatorId];
        stats.allocCount     = 0;
        stats.totalRequested = 0;
        stats.totalRequired  = 0;

        for (auto& taggedAllocation : stats.taggedAllocations)
        {
#ifdef C3D_MEMORY_METRICS_POINTERS
            taggedAllocation.allocations.clear();
#else
            taggedAllocation.requestedSize = 0;
            taggedAllocation.requiredSize  = 0;
            taggedAllocation.count         = 0;
#endif
        }
    }

    void MetricSystem::SetAllocatorAvailableSpace(const u8 allocatorId, const u64 availableSpace)
    {
        m_memoryStats[allocatorId].totalAvailableSpace = availableSpace;
    }

    u64 MetricSystem::GetAllocCount(const u8 allocatorId) const { return m_memoryStats[allocatorId].allocCount; }

    u64 MetricSystem::GetAllocCount(MemoryType memoryType, u8 allocatorId) const
    {
        return m_memoryStats[allocatorId].taggedAllocations[ToUnderlying(memoryType)].count;
    }

    u64 MetricSystem::GetMemoryUsage(const MemoryType memoryType, const u8 allocatorId) const
    {
#ifdef C3D_MEMORY_METRICS_POINTERS
        u64 requiredSize = 0;
        for (const auto& alloc : m_memoryStats[allocatorId].taggedAllocations[ToUnderlying(memoryType)].allocations)
        {
            requiredSize += alloc.requiredSize;
        }
        return requiredSize;
#else
        return m_memoryStats[allocatorId].taggedAllocations[ToUnderlying(memoryType)].requiredSize;
#endif
    }

    u64 MetricSystem::GetRequestedMemoryUsage(const MemoryType memoryType, const u8 allocatorId) const
    {
#ifdef C3D_MEMORY_METRICS_POINTERS
        const auto tagged = m_memoryStats[allocatorId].taggedAllocations[ToUnderlying(memoryType)];

        u64 requestedSize = 0;
        for (const auto& alloc : tagged.allocations)
        {
            requestedSize += alloc.requestedSize;
        }
        // Return the requested size + the gpu allocator's requested size
        return requestedSize + tagged.requestedSize;
#else
        return m_memoryStats[allocatorId].taggedAllocations[ToUnderlying(memoryType)].requestedSize;
#endif
    }

    void MetricSystem::PrintMemoryUsage(const u8 allocatorId, const bool debugLines)
    {
        char buffer[4096] = {};

        const auto& memStats = m_memoryStats[allocatorId];
        if (memStats.type != AllocatorType::None && memStats.allocCount > 0)
        {
            auto offset = 0;
            i32 bytesWritten =
                snprintf(buffer + offset, 8192, "%s with id: '%d' and type: '%d'\n", memStats.name.Data(), allocatorId, ToUnderlying(memStats.type));
            if (bytesWritten == -1)
            {
                FATAL_LOG("Sprintf_s() failed with an error.");
            }

            offset += bytesWritten;

            for (u8 j = 0; j < ToUnderlying(MemoryType::MaxType); j++)
            {
                auto& allocation = memStats.taggedAllocations[j];
                SprintfAllocation(allocation, j, buffer, bytesWritten, offset, debugLines);
                offset += bytesWritten;
            }

            const auto required   = memStats.totalRequired;
            const auto total      = memStats.totalAvailableSpace;
            const auto percentage = static_cast<f64>(required) / static_cast<f64>(total) * 100.0;

            f64 requiredAmount, totalAmount;
            const char* requiredUnit = SizeToText(required, &requiredAmount);
            const char* totalUnit    = SizeToText(total, &totalAmount);

            bytesWritten = snprintf(buffer + offset, 8192, "  %d total allocations using: %.2f %-3s of total: %.2f %-3s (%.2f%%)\n",
                                    static_cast<int>(memStats.allocCount), requiredAmount, requiredUnit, totalAmount, totalUnit, percentage);

            Logger::Info(buffer);
        }
    }

    void MetricSystem::PrintMemoryUsage(const bool debugLines)
    {
        Logger::Info("--------- MEMORY USAGE ---------");
        for (u8 i = 0; i < METRICS_COUNT; i++)
        {
            PrintMemoryUsage(i, debugLines);
        }
        Logger::Info("--------- MEMORY USAGE ---------");
    }

    MetricSystem& MetricSystem::GetInstance()
    {
        if (!s_instance)
        {
            s_instance = new MetricSystem();
            s_instance->Init();
        }
        return *s_instance;
    }

    const char* MetricSystem::SizeToText(const u64 size, f64* outAmount)
    {
        if (size >= GibiBytes(1))
        {
            *outAmount = static_cast<f64>(size) / GibiBytes(1);
            return "GiB";
        }

        if (size >= MebiBytes(1))
        {
            *outAmount = static_cast<f64>(size) / MebiBytes(1);
            return "MiB";
        }

        if (size >= KibiBytes(1))
        {
            *outAmount = static_cast<f64>(size) / KibiBytes(1);
            return "KiB";
        }

        *outAmount = static_cast<f64>(size);
        return "B";
    }

    void MetricSystem::SprintfAllocation(const MemoryAllocations& allocation, const int index, char* buffer, int& bytesWritten, const int offset,
                                         const bool debugLines)
    {
        f64 requestedAmount, requiredAmount;
        const char* requestedUnit;
        const char* requiredUnit;
        u32 count;

#ifdef C3D_MEMORY_METRICS_POINTERS
        u64 requestedSize = 0;
        u64 requiredSize  = 0;

        for (const auto& alloc : allocation.allocations)
        {
            requestedSize += alloc.requestedSize;
            requiredSize += alloc.requiredSize;

#ifdef C3D_MEMORY_METRICS_STACKTRACE
            if (debugLines)
            {
                DEBUG_LOG("{}", alloc.stacktrace);
            }
#endif
        }

        requestedUnit = SizeToText(requestedSize, &requestedAmount);
        requiredUnit  = SizeToText(requiredSize, &requiredAmount);
        count         = static_cast<int>(allocation.allocations.size());
#else
        requestedUnit = SizeToText(allocation.requestedSize, &requestedAmount);
        requiredUnit  = SizeToText(allocation.requiredSize, &requiredAmount);
        count         = allocation.count;
#endif

        if (EpsilonEqual(requestedAmount, 0) && EpsilonEqual(requiredAmount, 0))
        {
            bytesWritten = 0;
            return;
        }

        if (!EpsilonEqual(requestedAmount, requiredAmount))
        {
            bytesWritten = snprintf(buffer + offset, 8192, "  %-20s: %4d using %6.2f %-3s | (%6.2f %-3s)\n", MEMORY_TYPE_STRINGS[index], count, requestedAmount,
                                    requestedUnit, requiredAmount, requiredUnit);
        }
        else
        {
            bytesWritten =
                snprintf(buffer + offset, 8192, "  %-20s: %4d using %6.2f %-3s\n", MEMORY_TYPE_STRINGS[index], count, requestedAmount, requestedUnit);
        }

        if (bytesWritten == -1)
        {
            FATAL_LOG("Sprintf_s() failed with an error.");
        }
    }
}  // namespace C3D
