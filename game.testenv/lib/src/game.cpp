
#include "game.h"

#include <assets/managers/mesh_manager.h>
#include <assets/managers/scene/scene_manager.h>
#include <config/config_system.h>
#include <engine.h>
#include <events/event_system.h>
#include <frame_data.h>
#include <input/input_system.h>
#include <logger/logger.h>
#include <math/ray.h>
#include <metrics/metrics.h>
#include <renderer/camera.h>
#include <renderer/render_system.h>
#include <string/cstring.h>
#include <system/system_manager.h>

#include "input/keys.h"

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

    if (!meshNames.Empty())
    {
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

        // Upload our mesh assets to the renderer
        if (!Renderer.UploadMeshes(meshes))
        {
            ERROR_LOG("Failed to upload meshes.");
            return false;
        }

        if (!Renderer.GenerateDrawCommands())
        {
            ERROR_LOG("Failed to generate draw commands.");
            return false;
        }

        // Cleanup our assets since we no longer need them
        for (auto& mesh : meshes)
        {
            meshManager.Cleanup(mesh);
        }
    }
    else
    {
        C3D::String sceneName;
        if (!Config.GetProperty("Scene", sceneName))
        {
            ERROR_LOG("Failed to read scene name from configuration.");
            return false;
        }

        // Load the scene
        C3D::SceneManager sceneManager;
        C3D::SceneAsset sceneAsset;
        if (!sceneManager.Read(sceneName, sceneAsset))
        {
            ERROR_LOG("Failed to read: '{}' scene.", sceneName);
            return false;
        }

        // Upload our mesh assets to the renderer
        if (!Renderer.UploadMeshes(sceneAsset.meshes))
        {
            ERROR_LOG("Failed to upload meshes.");
            return false;
        }

        if (!Renderer.UploadTextures(sceneAsset.textures))
        {
            ERROR_LOG("Failed to upload textures.");
            return false;
        }

        if (!Renderer.UploadDrawCommands(sceneAsset.draws))
        {
            ERROR_LOG("Failed to generate draw commands.");
            return false;
        }

        // Get the default camera and copy over the scene camera settings
        auto& camera = Camera.GetDefaultCamera();

        camera.SetPosition(sceneAsset.camera.position);
        camera.SetRotation(sceneAsset.camera.orientation);
        camera.SetFovY(sceneAsset.camera.fovY);

        // Finally set out camera and sun direction
        Renderer.SetActiveCamera(camera.GetHandle());
        Renderer.SetSunDirection(sceneAsset.sunDirection);

        // Cleanup our scene asset since we are done with it
        sceneManager.Cleanup(sceneAsset);
    }

    return true;
}

void TestEnv::OnUpdate(C3D::FrameData& frameData)
{
    constexpr static C3D::Keys KEYS[]         = { C3D::KeyM, C3D::KeyC, C3D::KeyK, C3D::KeyR, C3D::KeyO, C3D::KeyL, C3D::KeyP, C3D::KeyT };
    constexpr static f32 DEFAULT_MOVE_SPEED   = 0.02f;
    constexpr static f32 DEFAULT_ROTATE_SPEED = glm::radians(0.1f);
    constexpr static f32 BOOST_MOVE_SPEED     = 0.2f;
    constexpr static f32 BOOST_ROTATE_SPEED   = glm::radians(0.2f);

    for (auto key : KEYS)
    {
        if (Input.IsKeyPressed(key))
        {
            C3D::EventContext context;
            context.data.u32[0] = key;
            Event.Fire(C3D::EventCodeDebug0, nullptr, context);
        }
    }

    auto& camera     = Camera.GetDefaultCamera();
    auto moveSpeed   = Input.IsShiftDown() ? BOOST_MOVE_SPEED : DEFAULT_MOVE_SPEED;
    auto rotateSpeed = Input.IsShiftDown() ? BOOST_ROTATE_SPEED : DEFAULT_ROTATE_SPEED;

    if (Input.IsKeyDown(C3D::KeyW))
    {
        camera.MoveForward(moveSpeed);
    }
    if (Input.IsKeyDown(C3D::KeyS))
    {
        camera.MoveBackward(moveSpeed);
    }
    if (Input.IsKeyDown(C3D::KeyD))
    {
        camera.MoveRight(moveSpeed);
    }
    if (Input.IsKeyDown(C3D::KeyA))
    {
        camera.MoveLeft(moveSpeed);
    }
    if (Input.IsKeyDown(C3D::KeySpace))
    {
        camera.MoveUp(moveSpeed);
    }
    if (Input.IsKeyDown(C3D::KeyLControl))
    {
        camera.MoveDown(moveSpeed);
    }
    if (Input.IsKeyDown(C3D::KeyArrowLeft))
    {
        camera.AddYaw(-rotateSpeed);
    }
    if (Input.IsKeyDown(C3D::KeyArrowRight))
    {
        camera.AddYaw(rotateSpeed);
    }
    if (Input.IsKeyDown(C3D::KeyArrowUp))
    {
        camera.AddPitch(-rotateSpeed);
    }
    if (Input.IsKeyDown(C3D::KeyArrowDown))
    {
        camera.AddPitch(rotateSpeed);
    }

    for (u32 i = 0; i < 9; i++)
    {
        if (Input.IsKeyPressed(C3D::Key0 + i))
        {
            C3D::EventContext context;
            context.data.u32[0] = i;
            Event.Fire(C3D::EventCodeDebug1, nullptr, context);
        }
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
