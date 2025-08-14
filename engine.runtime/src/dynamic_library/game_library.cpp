
#include "game_library.h"

namespace C3D
{
    void* GameLibrary::CreateState()
    {
        const auto createStateFunc = LoadFunction<void* (*)()>("CreateApplicationState");
        if (!createStateFunc)
        {
            ERROR_LOG("Failed to load CreateState function for: '{}'.", m_name);
            return nullptr;
        }

        const auto state = createStateFunc();
        if (!state)
        {
            ERROR_LOG("Failed to create state for: '{}'.", m_name);
            return nullptr;
        }

        return state;
    }

    Application* GameLibrary::Create(void* state)
    {
        const auto createApplicationFunc = LoadFunction<Application* (*)(void*)>("CreateApplication");
        if (!createApplicationFunc)
        {
            ERROR_LOG("Failed to load CreateApplication function for: '{}'.", m_name);
            return nullptr;
        }

        return createApplicationFunc(state);
    }
}  // namespace C3D
