
#pragma once

#include "identifiers/uuid.h"
#include "system/system.h"

namespace C3D
{
    class C3D_API Camera
    {
    public:
        Camera() = default;

        // Ensure we don't copy a camera by deleting copy constructors and operators
        Camera(const Camera&) = delete;
        Camera(Camera&&)      = delete;

        Camera& operator=(const Camera&) = delete;
        Camera& operator=(Camera&&)      = delete;

        void Create();

        const mat4& GetViewMatrix();

        void SetPosition(const vec3& position);
        void SetRotation(const quat& rotation);
        void SetFovY(f32 fovY);

        void MoveForward(f32 amount);
        void MoveBackward(f32 amount);

        void MoveLeft(f32 amount);
        void MoveRight(f32 amount);

        void MoveUp(f32 amount);
        void MoveDown(f32 amount);

        void AddYaw(f32 amount);
        void AddPitch(f32 amount);

        UUID GetHandle() const { return m_handle; }

        const vec3& GetPosition() const { return m_position; }

        f32 GetFovY() const { return m_fovY; }

    private:
        UUID m_handle;

        bool m_needsUpdate = true;

        vec3 m_position;
        quat m_rotation;

        mat4 m_viewMatrix;

        f32 m_fovY = 0.f;
    };

    class C3D_API CameraSystem final : public BaseSystem
    {
    public:
        bool OnInit() override;
        void OnShutdown() override;

        [[nodiscard]] Camera& GetDefaultCamera();

        [[nodiscard]] Camera& Get(UUID handle);

    private:
        Camera m_cameras[8];
    };

}  // namespace C3D