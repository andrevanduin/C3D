
#pragma once
#include <vector>

#include "containers/array.h"
#include "string/cstring.h"

namespace C3D
{
    constexpr auto ALLOCATOR_NAME_MAX_LENGTH = 128;

    enum class AllocatorType : u8
    {
        None,
        GlobalDynamic,
        Dynamic,
        System,
        Linear,
        Malloc,
        Stack,
        GpuLocal,
        External,
        MaxType,
    };

    enum class MemoryType : u8
    {
        Unknown,
        DynamicAllocator,
        LinearAllocator,
        FreeList,
        Array,
        DynamicArray,
        Stack,
        HashTable,
        HashMap,
        HashSet,
        Queue,
        RingQueue,
        Bst,
        String,
        C3DString,
        Engine,
        Platform,
        AssetManager,
        EventSystem,
        Job,
        Texture,
        MaterialInstance,
        Mesh,
        CoreSystem,
        RenderSystem,
        RenderData,
        RenderView,
        Game,
        Transform,
        Entity,
        EntityNode,
        Scene,
        CVar,
        Shader,
        Asset,
        Vulkan,
        VulkanExternal,
        Direct3D,
        OpenGL,
        AudioType,
        BitmapFont,
        SystemFont,
        Terrain,
        Test,
        DebugConsole,
        DebugRendering,
        Command,
        UI,
        ECS,
        MaxType,
    };

    constexpr auto MAX_MEMORY_TYPES = static_cast<u64>(MemoryType::MaxType);

    struct Allocation
    {
#ifdef C3D_MEMORY_METRICS_POINTERS
        Allocation(const MemoryType type, void* ptr, const u64 requestedSize, const u64 requiredSize = 0)
            : type(type), requestedSize(requestedSize), requiredSize(requiredSize == 0 ? requestedSize : requiredSize), ptr(ptr)
        {}
#endif

#ifndef C3D_MEMORY_METRICS_POINTERS
        Allocation(const MemoryType type, const u64 requestedSize, const u64 requiredSize = 0)
            : type(type), requestedSize(requestedSize), requiredSize(requiredSize == 0 ? requestedSize : requiredSize)
        {}
#endif

        MemoryType type;
        u64 requestedSize;
        u64 requiredSize;

#ifdef C3D_MEMORY_METRICS_POINTERS
        void* ptr;
#endif
    };

    struct DeAllocation
    {
#ifdef C3D_MEMORY_METRICS_POINTERS
        DeAllocation(const MemoryType type, void* ptr) : type(type), ptr(ptr) {}
#endif

#ifndef C3D_MEMORY_METRICS_POINTERS
        DeAllocation(const MemoryType type, const u64 requestedSize, const u64 requiredSize = 0)
            : type(type), requestedSize(requestedSize), requiredSize(requiredSize == 0 ? requestedSize : requiredSize)
        {}
#endif

        MemoryType type;

#ifdef C3D_MEMORY_METRICS_POINTERS
        void* ptr;
#else
        u64 requestedSize;
        u64 requiredSize;
#endif
    };

#ifdef C3D_MEMORY_METRICS_POINTERS
    struct TrackedAllocation
    {
        TrackedAllocation(void* ptr, std::string stacktrace, const u64 requestedSize, const u64 requiredSize)
            : ptr(ptr), stacktrace(std::move(stacktrace)), requestedSize(requestedSize), requiredSize(requiredSize)
        {}

        void* ptr = nullptr;

        std::string stacktrace;

        u64 requestedSize = 0;
        u64 requiredSize  = 0;
    };
#endif

    struct MemoryAllocations
    {
#ifdef C3D_MEMORY_METRICS_POINTERS
        std::vector<TrackedAllocation> allocations;
#endif
        u32 count         = 0;
        u64 requestedSize = 0;
        u64 requiredSize  = 0;
    };

    struct ExternalAllocations
    {
        u32 count = 0;
        u64 size  = 0;
    };

    using TaggedAllocations = Array<MemoryAllocations, MAX_MEMORY_TYPES>;

    struct MemoryStats
    {
        // The type of this allocator
        AllocatorType type = AllocatorType::None;
        // The name of this allocator
        CString<ALLOCATOR_NAME_MAX_LENGTH> name;
        // The amount of total space available in this allocator
        u64 totalAvailableSpace = 0;
        // The amount of total space current required for all the allocations associated with this allocator
        u64 totalRequired = 0;
        // The amount of total space requested by the user for this allocator
        u64 totalRequested = 0;
        // The amount of total allocations currently done by this allocator
        u64 allocCount = 0;
        // An array of all the different types of allocations with stats about each
        TaggedAllocations taggedAllocations{};
    };
}  // namespace C3D
