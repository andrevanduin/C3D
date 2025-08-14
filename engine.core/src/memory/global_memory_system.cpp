
#include "global_memory_system.h"

#include "allocators/linear_allocator.h"
#include "allocators/malloc_allocator.h"
#include "allocators/stack_allocator.h"
#include "platform/platform.h"

namespace C3D
{
    static bool initialized = false;

    void GlobalMemorySystem::Init(const MemorySystemConfig& config)
    {
        C3D_ASSERT_MSG(!initialized, "Can't initialize the global memory system when it's already initialized!");

        const u64 memoryRequirement = DynamicAllocator::GetMemoryRequirements(config.totalAllocSize);
        const auto memoryBlock      = std::malloc(memoryRequirement);
        if (!memoryBlock)
        {
            FATAL_LOG("Allocating memory pool failed");
        }

        const auto globalAllocator = BaseAllocator<DynamicAllocator>::GetDefault();
        globalAllocator->Create(memoryBlock, memoryRequirement, config.totalAllocSize);

        const auto linearAllocator = BaseAllocator<LinearAllocator>::GetDefault();
        linearAllocator->Create("DefaultLinearAllocator", KibiBytes(8));

        const auto stackAllocator = BaseAllocator<StackAllocator<KibiBytes(8)>>::GetDefault();
        stackAllocator->Create("DefaultStackAllocator");

        initialized = true;

        INFO_LOG("Initialized successfully.");
    }

    void GlobalMemorySystem::Destroy()
    {
        INFO_LOG("Shutting down");

        if (const auto stackAllocator = BaseAllocator<StackAllocator<KibiBytes(8)>>::GetDefault())
        {
            stackAllocator->Destroy();
            delete stackAllocator;
        }

        if (const auto linearAllocator = BaseAllocator<LinearAllocator>::GetDefault())
        {
            linearAllocator->Destroy();
            delete linearAllocator;
        }

        if (const auto globalAllocator = BaseAllocator<DynamicAllocator>::GetDefault())
        {
            // Destroy the global allocator and delete it
            globalAllocator->Destroy();

            // Free our entire memory block
            auto memory = globalAllocator->GetMemory();
            std::free(memory);

            delete globalAllocator;
        }

        initialized = false;
    }

    DynamicAllocator& GlobalMemorySystem::GetAllocator() { return *BaseAllocator<DynamicAllocator>::GetDefault(); }
}  // namespace C3D
