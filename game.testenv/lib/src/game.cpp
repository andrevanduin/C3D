
#include "game.h"

#include <assets/managers/mesh_manager.h>
#include <config/config_system.h>
#include <engine.h>
#include <events/event_system.h>
#include <frame_data.h>
#include <input/input_system.h>
#include <logger/logger.h>
#include <math/ray.h>
#include <metrics/metrics.h>
#include <renderer/render_system.h>
#include <string/cstring.h>
#include <system/system_manager.h>

TestEnv::TestEnv(void* state) : m_state(reinterpret_cast<GameState*>(state)) {}

bool TestEnv::OnBoot()
{
    INFO_LOG("Booting TestEnv.");
    return true;
}

bool TestEnv::OnRun(C3D::FrameData& frameData)
{
    // Get the names of the meshes we want to use
    C3D::DynamicArray<C3D::String> meshNames;
    if (!Config.GetProperty("Meshes", meshNames))
    {
        ERROR_LOG("Failed to read list of Meshes from configuration.");
        return false;
    }

    // Prepare an array for all the mesh assets
    C3D::DynamicArray<C3D::MeshAsset> meshes(meshNames.Size());
    // Load/read in all the mesh assets
    C3D::MeshManager meshManager;
    for (u32 i = 0; i < meshNames.Size(); ++i)
    {
        auto& name  = meshNames[i];
        auto& asset = meshes[i];

        if (!meshManager.Read(name, asset))
        {
            INFO_LOG("Failed to read: '{}' mesh.", name);
        }
    }

    auto window = C3D::Engine::GetCurrentWindow();

    // Upload our mesh assets to the renderer
    if (!Renderer.UploadMeshes(window, meshes))
    {
        ERROR_LOG("Failed to upload meshes.");
        return false;
    }

    if (!Renderer.GenerateDrawCommands(window))
    {
        ERROR_LOG("Failed to generate draw commands.");
        return false;
    }

    // Cleanup our assets since we no longer need them
    for (auto& mesh : meshes)
    {
        meshManager.Cleanup(mesh);
    }

    return true;
}

void TestEnv::OnUpdate(C3D::FrameData& frameData)
{
    if (Input.IsKeyPressed(C3D::Key0))
    {
        C3D::EventContext context;
        Event.Fire(C3D::EventCodeDebug0, nullptr, context);
    }
    if (Input.IsKeyPressed(C3D::KeyC))
    {
        C3D::EventContext context;
        Event.Fire(C3D::EventCodeDebug1, nullptr, context);
    }
}

bool TestEnv::OnPrepareFrame(C3D::FrameData& frameData) { return true; }

bool TestEnv::OnRenderFrame(C3D::FrameData& frameData) { return true; }

void TestEnv::OnWindowResize(const C3D::Window& window) {}

void TestEnv::OnShutdown() { m_state->registeredCallbacks.Destroy(); }

void TestEnv::OnLibraryLoad() {}

void TestEnv::OnLibraryUnload()
{
    for (auto& cb : m_state->registeredCallbacks)
    {
        Event.Unregister(cb);
    }
    m_state->registeredCallbacks.Destroy();
}

C3D::Application* CreateApplication(void* state) { return Memory.New<TestEnv>(C3D::MemoryType::Game, state); }

void* CreateApplicationState() { return Memory.New<GameState>(C3D::MemoryType::Game); }
