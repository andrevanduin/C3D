
#pragma once
#include "application.h"
#include "dynamic_library.h"

namespace C3D
{
    class C3D_API GameLibrary final : public DynamicLibrary
    {
    public:
        void* CreateState();

        Application* Create(void* state);
    };
}  // namespace C3D
