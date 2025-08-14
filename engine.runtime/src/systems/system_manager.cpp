
#include "system_manager.h"

#include "logger/logger.h"

namespace C3D
{
    struct SystemManagerState
    {
        Array<ISystem*, MaxKnownSystemType> systems = {};
        LinearAllocator allocator;
    };

    static SystemManagerState state;

    bool SystemManager::OnInit()
    {
        // 8 mb of total space for all our systems
        constexpr u64 systemsAllocatorTotalSize = MebiBytes(2);
        state.allocator.Create("LINEAR_SYSTEM_ALLOCATOR", systemsAllocatorTotalSize);

        INFO_LOG("Initialized successfully.");
        return true;
    }

    void SystemManager::RegisterSystem(const u16 type, ISystem* system) { state.systems[type] = system; }

    LinearAllocator& SystemManager::GetAllocator() { return state.allocator; }

    ISystem* SystemManager::GetSystem(u16 type) { return state.systems[type]; }

    bool SystemManager::OnPrepareFrame(FrameData& frameData)
    {
        for (const auto system : state.systems)
        {
            if (!system->OnPrepareFrame(frameData))
            {
                return false;
            }
        }

        return true;
    }

    void SystemManager::OnShutdown()
    {
        INFO_LOG("Shutting down all Systems.");

        for (const auto system : state.systems)
        {
            if (system)
            {
                system->OnShutdown();
                state.allocator.Delete(system);
            }
        }

        state.allocator.Destroy();
    }
}  // namespace C3D
