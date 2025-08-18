
#include "game.h"

#include <engine.h>
#include <events/event_system.h>
#include <frame_data.h>
#include <logger/logger.h>
#include <math/ray.h>
#include <metrics/metrics.h>
#include <string/cstring.h>
#include <system/system_manager.h>

#include <glm/gtx/matrix_decompose.hpp>

TestEnv::TestEnv(void* state) : m_state(reinterpret_cast<GameState*>(state)) {}

bool TestEnv::OnBoot()
{
    INFO_LOG("Booting TestEnv.");
    return true;
}

bool TestEnv::OnRun(C3D::FrameData& frameData) { return true; }

void TestEnv::OnUpdate(C3D::FrameData& frameData) {}

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
