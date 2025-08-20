
#pragma once
#include "defines.h"
#include "memory/allocators/linear_allocator.h"
#include "system.h"

#define Config C3D::SystemManager::GetSystem<C3D::ConfigSystem>(C3D::SystemType::ConfigSystemType)
#define Event C3D::SystemManager::GetSystem<C3D::EventSystem>(C3D::SystemType::EventSystemType)
#define Renderer C3D::SystemManager::GetSystem<C3D::RenderSystem>(C3D::SystemType::RenderSystemType)
#define Input C3D::SystemManager::GetSystem<C3D::InputSystem>(C3D::SystemType::InputSystemType)

namespace C3D
{
    enum SystemType
    {
        RenderSystemType = 0,
        InputSystemType,
        EventSystemType,
        ConfigSystemType,
        MaxKnownSystemType
    };

    namespace SystemManager
    {
        C3D_API bool OnInit();

        C3D_API void RegisterSystem(const u16 type, ISystem* system);

        C3D_API LinearAllocator& GetAllocator();
        C3D_API C3D_INLINE ISystem* GetSystem(u16 type);

        C3D_API bool OnPrepareFrame(FrameData& frameData);

        C3D_API void OnShutdown();

        template <class System>
        bool RegisterSystem(const u16 systemType)
        {
            static_assert(std::is_base_of_v<BaseSystem, System>, "The provided system does not derive from the ISystem class.");

            if (systemType > MaxKnownSystemType)
            {
                ERROR_LOG("The provided systemType should be 0 <= {} < {}.", systemType, ToUnderlying(MaxKnownSystemType));
                return false;
            }

            auto& allocator = GetAllocator();
            auto system     = allocator.New<System>(MemoryType::CoreSystem);
            if (!system->OnInit())
            {
                FATAL_LOG("Failed to initialize system.");
                return false;
            }

            RegisterSystem(systemType, system);
            return true;
        }

        template <class System>
        bool RegisterSystem(const u16 systemType, const CSONObject& config)
        {
            static_assert(std::is_base_of_v<ISystem, System>, "The provided system does not derive from the ISystem class.");

            if (systemType > MaxKnownSystemType)
            {
                ERROR_LOG("The provided systemType should be 0 <= {} < {}.", systemType, ToUnderlying(MaxKnownSystemType));
                return false;
            }

            auto& allocator = GetAllocator();
            auto system     = allocator.New<System>(MemoryType::CoreSystem);
            if (!system->OnInit(config))
            {
                FATAL_LOG("Failed to initialize system.");
                return false;
            }

            RegisterSystem(systemType, system);
            return true;
        }

        template <class SystemType>
        inline SystemType& GetSystem(const u16 type)
        {
            return *reinterpret_cast<SystemType*>(GetSystem(type));
        }

    };  // namespace SystemManager
}  // namespace C3D