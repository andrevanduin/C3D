
#include "engine.h"

#include "application.h"
#include "events/event_system.h"
#include "logger/logger.h"
#include "metrics/metrics.h"
#include "platform/platform.h"
#include "renderer/render_system.h"
#include "string/string.h"
#include "systems/system_manager.h"
#include "time/clock.h"

namespace C3D
{
    struct EngineState
    {
        /** @brief A few flags to keep track of the current state. */
        bool running     = false;
        bool suspended   = false;
        bool initialized = false;
        /** @brief A list of windows currently in use. */
        DynamicArray<Window> windows;
        /** @brief A */
        /** @brief Allocator used for allocating frame data. Gets cleared on every frame. */
        LinearAllocator frameAllocator;
        /** @brief The data that is relevant for every frame. */
        FrameData frameData;
        /** @brief Struct containing all the different clocks we need to keep track off. */
        Clocks clocks;
        /** @brief Keep track of the last time. So we can get a delta between each update. */
        f64 lastTime = 0;
        /** @brief A pointer to the application which the engine can use to call it's methods. */
        Application* app;
        /** @brief Application config. */
        ApplicationConfig config;
    };

    static EngineState state;

    static void OnWindowResizeEvent(const Window& window);

    bool Engine::OnInit(Application* application, const ApplicationConfig& config)
    {
        C3D_ASSERT_MSG(!state.initialized, "Tried to initialize the engine twice");

        INFO_LOG("Initializing.");

        // Initialize the platform layer
        Platform::Init();

        // Take a pointer to the application
        state.app = application;
        // Move the config to our engien state
        state.config = config;

        auto threadCount = Platform::GetProcessorCount();
        if (threadCount <= 1)
        {
            ERROR_LOG("System reported: {} threads. C3DEngine requires at least 1 thread besides the main thread.", threadCount);
            return false;
        }

        INFO_LOG("System reported: {} threads (including main thread).", threadCount);

        // Setup our frame allocator
        if (state.config.frameAllocatorSize < MebiBytes(8))
        {
            ERROR_LOG("Frame allocator must be >= 8 Mebibytes.");
            return false;
        }

        // Create a frame allocator that will be freed after every frame
        state.frameAllocator.Create("FRAME_ALLOCATOR", state.config.frameAllocatorSize);

        SystemManager::OnInit();

        Platform::SetOnQuitCallback(Quit);
        Platform::SetOnWindowResizedCallback(OnWindowResizeEvent);

        // Init before boot systems
        SystemManager::RegisterSystem<EventSystem>(EventSystemType);  // Event System

        // After the Event system is up and running we register an OnQuit event
        Event.Register(EventCodeApplicationQuit, [](u16 code, void* sender, const EventContext& context) {
            Quit();
            return true;
        });

        SystemManager::RegisterSystem<RenderSystem>(RenderSystemType, state.config.systemConfigs["Renderer"]);  // Render System

        // Create all the requested windows
        u32 index = 0;
        for (auto& windowConfig : state.config.windowConfigs)
        {
            // Add a window to our engine state
            auto& window = state.windows.EmplaceBack();
            window.index = index;

            // Create the platform specific stuff
            if (!Platform::CreateWindow(windowConfig, window))
            {
                ERROR_LOG("Failed to create window: '{}'.", windowConfig.name);
                return false;
            }

            if (!Renderer.OnCreateWindow(window))
            {
                ERROR_LOG("Failed to create renderer specific internals for window: '{}'.", windowConfig.name);
                return false;
            }

            index++;
        }

        // Try to boot the application
        if (!state.app->OnBoot())
        {
            ERROR_LOG("Application failed to boot!");
            return false;
        }

        state.initialized = true;
        state.lastTime    = 0;

        INFO_LOG("Successfully initialized.");
        return true;
    }

    void Engine::Run()
    {
        INFO_LOG("Started.");

        state.running  = true;
        state.lastTime = Platform::GetAbsoluteTime();

        if (!state.app->OnRun(state.frameData))
        {
            ERROR_LOG("Failed to execute application OnRun() method.");
            state.running = false;
        }

        Metrics.PrintMemoryUsage();

        // TODO: This currently assumes only 1 active window
        // Since we support multiple windows we need to handle this better
        auto& currentWindow = state.windows[0];

        while (state.running)
        {
            if (!Platform::PumpMessages())
            {
                state.running = false;
            }

            if (!state.suspended)
            {
                state.clocks.total.Begin();

                const f64 currentTime = Platform::GetAbsoluteTime();
                const f64 delta       = currentTime - state.lastTime;

                state.frameData.timeData.total += delta;
                state.frameData.timeData.delta = delta;

                // Reset our frame allocator (freeing all memory used previous frame)
                state.frameAllocator.FreeAll();

                state.clocks.onUpdate.Begin();

                OnUpdate();

                state.clocks.onUpdate.End();

                // Reset our drawn mesh count for the next frame
                state.frameData.drawnMeshCount = 0;

                state.clocks.onRender.Begin();

                if (!Renderer.Begin(currentWindow))
                {
                    INFO_LOG("Begin failed, skipping this frame.");
                    continue;
                }

                // Call the game's render routine
                if (!state.app->OnRenderFrame(state.frameData))
                {
                    FATAL_LOG("Failed to render frame.");
                    state.running = false;
                    break;
                }

                if (!Renderer.End(currentWindow))
                {
                    FATAL_LOG("Failed to end rendering")
                    break;
                }

                if (!Renderer.Submit(currentWindow))
                {
                    FATAL_LOG("Failed to submit.");
                    break;
                }

                if (!Renderer.Present(currentWindow))
                {
                    FATAL_LOG("Failed to present.");
                    break;
                }

                state.clocks.onRender.End();

                state.clocks.total.End();
                state.lastTime = currentTime;
            }
        }

        INFO_LOG("Finished.");
    }

    void Engine::Quit() { state.running = false; }

    void Engine::OnUpdate() { state.app->OnUpdate(state.frameData); }

    void Engine::OnWindowResize(Window& window)
    {
        // Notify our application of the resize
        state.app->OnWindowResize(window);
        // Mark the window as done resizing
        window.resizing = false;
        // Notify user of resize event

        EventContext context;
        context.data.u32[0] = window.index;

        Event.Fire(EventCodeResized, nullptr, context);
    }

    void Engine::OnApplicationLibraryReload(Application* app)
    {
        // Take a pointer to the app
        state.app = app;
        // Call the OnLibraryLoad method for the application
        state.app->OnLibraryLoad();
    }

    const ApplicationConfig& Engine::GetAppConfig() { return state.config; }

    const Window& Engine::GetCurrentWindow() { return state.windows[0]; }

    const Window& Engine::GetWindow(u32 index) { return state.windows[index]; }

    const LinearAllocator& Engine::GetFrameAllocator() { return state.frameAllocator; }

    void Engine::OnShutdown()
    {
        INFO_LOG("Shutting down.");

        // Call the OnShutdown() method that is defined by the user
        if (state.app)
        {
            state.app->OnShutdown();
        }

        // Destroy the application config
        state.config.name.Destroy();
        state.config.rendergraphs.Destroy();
        state.config.systemConfigs.Destroy();
        state.config.windowConfigs.Destroy();

        // Destroy our frame allocator since we will no longer render any frames
        state.frameAllocator.Destroy();

        // Cleanup up all our windows
        for (auto& window : state.windows)
        {
            Renderer.OnDestroyWindow(window);
            Platform::DestroyWindow(window);
        }
        state.windows.Destroy();

        // Finally our systems manager can be shut down
        SystemManager::OnShutdown();

        // Shutdown the platform layer
        Platform::Shutdown();

        state.initialized = false;
    }

    void OnWindowResizeEvent(const Window& window)
    {
        if (window.width == 0 || window.height == 0)
        {
            // TODO: This should be done per window and not globally
            INFO_LOG("Window minimized, suspending application.");
            state.suspended = true;
        }
        else if (state.suspended)
        {
            INFO_LOG("Window restored, resuming application.");
            state.suspended = false;
        }
    }
}  // namespace C3D
