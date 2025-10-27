#include "render/FreeCamera.h"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>

namespace Render
{

    FreeCamera::FreeCamera(const glm::vec3 &eye,
                           float yawDeg,
                           float pitchDeg,
                           float fovDeg,
                           float aspect,
                           float znear,
                           float zfar) noexcept
        : eye_(eye),
          yawDeg_(yawDeg),
          pitchDeg_(pitchDeg),
          fovDeg_(fovDeg),
          aspect_(aspect),
          znear_(znear),
          zfar_(zfar),
          viewMat_(1.0f),
          projMat_(1.0f),
          viewDirty_(true),
          projDirty_(true)
    {
    }

    // Return cached view matrix, recompute lazily
    const glm::mat4 &FreeCamera::view() const noexcept
    {
        if (viewDirty_)
            recomputeView();
        return viewMat_;
    }

    // Return cached projection matrix, recompute lazily
    const glm::mat4 &FreeCamera::proj() const noexcept
    {
        if (projDirty_)
            recomputeProj();
        return projMat_;
    }

    void FreeCamera::setPerspective(float fovDeg, float aspect, float znear, float zfar) noexcept
    {
        fovDeg_ = fovDeg;
        aspect_ = aspect;
        znear_ = znear;
        zfar_ = zfar;
        projDirty_ = true;
    }

    void FreeCamera::setAspect(float aspect) noexcept
    {
        aspect_ = aspect;
        projDirty_ = true;
    }

    void FreeCamera::moveLocal(const glm::vec3 &deltaLocal) noexcept
    {
        // deltaLocal: (right, up, forward)
        // Compute local axes from current orientation
        const float yawRad = glm::radians(yawDeg_);
        const float pitchRad = glm::radians(pitchDeg_);

        // Build forward vector from yaw/pitch
        glm::vec3 forward;
        forward.x = std::cos(pitchRad) * std::sin(yawRad);
        forward.y = std::sin(pitchRad);
        forward.z = std::cos(pitchRad) * std::cos(yawRad);
        forward = glm::normalize(forward);

        // Right = cross(forward, worldUp)
        const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));

        // Apply movement (note forward is +Z forward in this function's convention)
        eye_ += right * deltaLocal.x;
        eye_ += up * deltaLocal.y;
        eye_ += forward * deltaLocal.z;

        viewDirty_ = true;
    }

    void FreeCamera::addYawPitch(float deltaYawDeg, float deltaPitchDeg) noexcept
    {
        yawDeg_ -= deltaYawDeg;
        pitchDeg_ += deltaPitchDeg;
        // clamp pitch to sane range
        pitchDeg_ = std::clamp(pitchDeg_, -kPitchLimitDeg, kPitchLimitDeg);
        viewDirty_ = true;
    }

    void FreeCamera::lookAt(const glm::vec3 &eye, const glm::vec3 &target, const glm::vec3 &up) noexcept
    {
        eye_ = eye;

        // Derive yaw/pitch from direction vector
        glm::vec3 dir = glm::normalize(target - eye_);
        // pitch = asin(y)
        pitchDeg_ = glm::degrees(std::asin(glm::clamp(dir.y, -1.0f, 1.0f)));
        // yaw = atan2(x, z) (note: atan2(x, z) yields yaw around Y with our forward convention)
        yawDeg_ = glm::degrees(std::atan2(dir.x, dir.z));
        viewDirty_ = true;
    }

    void FreeCamera::recomputeView() const noexcept
    {
        // Build orientation from yaw/pitch (roll = 0)
        const float yawRad = glm::radians(yawDeg_);
        const float pitchRad = glm::radians(pitchDeg_);

        glm::vec3 forward;
        forward.x = std::cos(pitchRad) * std::sin(yawRad);
        forward.y = std::sin(pitchRad);
        forward.z = std::cos(pitchRad) * std::cos(yawRad);
        forward = glm::normalize(forward);

        const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        const glm::vec3 center = eye_ + forward;
        viewMat_ = glm::lookAt(eye_, center, worldUp);
        viewDirty_ = false;
    }

    void FreeCamera::recomputeProj() const noexcept
    {
        // glm::perspective expects radians
        projMat_ = glm::perspective(glm::radians(fovDeg_), aspect_, znear_, zfar_);

        // Vulkan clip space has inverted Y compared to OpenGL
        projMat_[1][1] *= -1.0f;

        // Vulkan's clip space has inverted Y; if your renderer expects that adjust externally.
        projDirty_ = false;
    }

} // namespace Render
