
#pragma once
#include "game_state.h"

extern "C" {
C3D_API C3D::Application* CreateApplication(void* state);
C3D_API void* CreateApplicationState();
}

class TestEnv final : public C3D::Application
{
public:
    TestEnv(void* state);

    bool OnBoot() override;
    bool OnRun(C3D::FrameData& frameData) override;

    void OnUpdate(C3D::FrameData& frameData) override;
    bool OnPrepareFrame(C3D::FrameData& frameData) override;
    bool OnRenderFrame(C3D::FrameData& frameData) override;

    void OnWindowResize(const C3D::Window& window) override;

    void OnShutdown() override;

    void OnLibraryLoad() override;
    void OnLibraryUnload() override;

private:
    GameState* m_state = nullptr;
};
