
#pragma once
#include "cson/cson_types.h"
#include "platform/platform_types.h"
#include "string/string.h"

namespace C3D
{
    struct FrameData;

    class C3D_API Application
    {
    public:
        Application() = default;

        Application(const Application&) = delete;
        Application(Application&&)      = delete;

        Application& operator=(const Application&) = delete;
        Application& operator=(Application&&)      = delete;

        virtual ~Application() = default;

        virtual bool OnBoot()                    = 0;
        virtual bool OnRun(FrameData& frameData) = 0;

        virtual void OnUpdate(FrameData& frameData)       = 0;
        virtual bool OnPrepareFrame(FrameData& frameData) = 0;
        virtual bool OnRenderFrame(FrameData& frameData)  = 0;

        virtual void OnWindowResize(const Window& window) = 0;

        virtual void OnShutdown() = 0;

        virtual void OnLibraryLoad()   = 0;
        virtual void OnLibraryUnload() = 0;
    };

    Application* CreateApplication();
    void InitApplication();
    void DestroyApplication();
}  // namespace C3D
