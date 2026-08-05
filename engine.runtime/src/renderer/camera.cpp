
#include "camera.h"

#include <glm/detail/type_quat.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "logger/logger.h"
#include "math/c3d_math.h"

namespace C3D
{
    static constexpr f32 CAMERA_GIMBALL_LOCK_LIMIT = DegToRad(89.0f);

    void Camera::Create() { m_handle.Generate(); }

    const mat4& Camera::GetViewMatrix()
    {
        if (m_needsUpdate)
        {
            m_viewMatrix    = glm::mat4_cast(m_rotation);
            m_viewMatrix[3] = vec4(m_position, 1.0f);
            m_viewMatrix    = glm::inverse(m_viewMatrix);
            m_viewMatrix    = glm::scale(glm::identity<glm::mat4>(), vec3(1, 1, -1)) * m_viewMatrix;

            m_needsUpdate = false;
        }
        return m_viewMatrix;
    }

    void Camera::SetPosition(const vec3& position)
    {
        m_position    = position;
        m_needsUpdate = true;
    }

    void Camera::SetRotation(const quat& rotation) { m_rotation = rotation; }

    void Camera::SetFovY(f32 fovY) { m_fovY = fovY; }

    void Camera::MoveForward(f32 amount)
    {
        m_position += amount * (m_rotation * vec3(0, 0, -1));
        m_needsUpdate = true;
    }

    void Camera::MoveBackward(f32 amount)
    {
        m_position -= amount * (m_rotation * vec3(0, 0, -1));
        m_needsUpdate = true;
    }

    void Camera::MoveLeft(f32 amount)
    {
        m_position -= amount * (m_rotation * vec3(1, 0, 0));
        m_needsUpdate = true;
    }

    void Camera::MoveRight(f32 amount)
    {
        m_position += amount * (m_rotation * vec3(1, 0, 0));
        m_needsUpdate = true;
    }

    void Camera::MoveUp(f32 amount)
    {
        m_position += amount * (m_rotation * vec3(0, 1, 0));
        m_needsUpdate = true;
    }

    void Camera::MoveDown(f32 amount)
    {
        m_position -= amount * (m_rotation * vec3(0, 1, 0));
        m_needsUpdate = true;
    }

    void Camera::AddYaw(f32 amount)
    {
        m_rotation    = glm::rotate(glm::quat(1, 0, 0, 0), -amount, vec3(0, 1, 0)) * m_rotation;
        m_needsUpdate = true;
    }

    void Camera::AddPitch(f32 amount)
    {
        m_rotation    = glm::rotate(glm::quat(1, 0, 0, 0), -amount, m_rotation * vec3(1, 0, 0)) * m_rotation;
        m_needsUpdate = true;
    }

    bool CameraSystem::OnInit()
    {
        // Generate a default valid camera for position 0. This will be the default camera
        auto& defaultCam = m_cameras[0];
        defaultCam.Create();

        INFO_LOG("Initialized successfully.");

        return true;
    }

    void CameraSystem::OnShutdown() {}

    Camera& CameraSystem::GetDefaultCamera() { return m_cameras[0]; }

    Camera& CameraSystem::Get(UUID handle)
    {
        if (!handle.IsValid())
        {
            ERROR_LOG("Invalid handle: {} passed. Returning default camera.", handle);
            return m_cameras[0];
        }

        for (auto& camera : m_cameras)
        {
            if (camera.GetHandle() == handle)
            {
                return camera;
            }
        }

        ERROR_LOG("No known camera for handle: {}. Returning default camera.", handle);
        return m_cameras[0];
    }
}  // namespace C3D