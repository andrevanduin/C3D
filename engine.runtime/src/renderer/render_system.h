
#pragma once
#include "defines.h"
#include "dynamic_library/dynamic_library.h"
#include "system/system.h"
#include "types.h"

namespace C3D
{
    class RendererPlugin;

    class C3D_API RenderSystem final : public SystemWithConfig
    {
    public:
        bool OnInit(const CSONObject& config) override;
        void OnShutdown() override;

        bool Begin(Window& window) const;
        bool End(Window& window) const;

        bool Submit(Window& window) const;
        bool Present(Window& window) const;

        bool OnCreateWindow(Window& window) const;
        bool OnResizeWindow(Window& window) const;
        void OnDestroyWindow(Window& window) const;

    private:
        /** @brief A pointer to the backend rendering plugin used to actually render things on the screen. */
        RendererPlugin* m_backendPlugin = nullptr;
        /** @brief A dynamic library object to load the rendering plugin. */
        DynamicLibrary m_backendDynamicLibrary;

        /** @brief The configuration for our render system. */
        RenderSystemConfig m_config;
    };
}  // namespace C3D