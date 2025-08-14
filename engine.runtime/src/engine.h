
#pragma once
#include "application.h"
#include "application_config.h"
#include "defines.h"
#include "frame_data.h"
#include "platform/platform_types.h"

namespace C3D
{
    class Application;
    class LinearAllocator;
    struct EventContext;

    namespace Engine
    {
        C3D_API bool OnInit(Application* application, const ApplicationConfig& config);

        C3D_API void Run();
        C3D_API void Quit();
        C3D_API void OnShutdown();

        C3D_API void OnUpdate();
        C3D_API void OnWindowResize(Window& window);

        C3D_API void OnApplicationLibraryReload(Application* app);

        C3D_API const ApplicationConfig& GetAppConfig();

        C3D_API const Window& GetCurrentWindow();
        C3D_API const Window& GetWindow(u32 index);

        C3D_API const LinearAllocator& GetFrameAllocator();
    };  // namespace Engine
}  // namespace C3D
