
#pragma once
#include "memory/global_memory_system.h"

namespace C3D
{
    enum class GLTFExtensionType
    {
        None,
        Transmission,
        PBRSpecularGlossiness,
        LightsPunctual,
        NodeLightsPunctual,
        TextureDDS,
    };

    struct GLTFExtension
    {
        GLTFExtensionType type;
        void* data = nullptr;

        template <typename T>
        T& Allocate()
        {
            data = Memory.New<T>(MemoryType::Scene);
            return *static_cast<T*>(data);
        }
    };
}  // namespace C3D