#pragma once

#include "Camera.h"
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace Render
{

    /**
     * @brief Simple free-fly camera (FPS-like) with yaw/pitch rotation and local movement.
     * - Yaw rotates around world Y (up) axis.
     * - Pitch rotates around camera's local X axis and is clamped to avoid flipping.
     * - Movement is expressed in local camera space: (right, up, forward).
     * - Caches view/proj matrices and updates lazily.
     */

    class FreeCamera : public Camera
    {
    public:
        FreeCamera(const glm::vec3 &eye = glm::vec3(0.0f, 0.0f, 3.0f),
                   float yawDeg = 0.0f,
                   float pitchDeg = 0.0f,
                   float fovDeg = 60.0f,
                   float aspect = 16.0f / 9.0f,
                   float znear = 0.01f,
                   float zfar = 1000.0f) noexcept;

        // Matrix accessors
        const glm::mat4 &view() const noexcept override;
        const glm::mat4 &proj() const noexcept override;

        // Projection
        void setPerspective(float fovDeg, float aspect, float znear, float zfar) noexcept override;
        void setAspect(float aspect) noexcept override;

        // World state
        glm::vec3 position() const noexcept override { return eye_; }

        // Control API
        void moveLocal(const glm::vec3 &deltaLocal) noexcept override;
        void addYawPitch(float deltaYawDeg, float deltaPitchDeg) noexcept override;
        void lookAt(const glm::vec3 &eye, const glm::vec3 &target, const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f)) noexcept override;

        // Helpers
        float yawDeg() const noexcept override { return yawDeg_; }
        float pitchDeg() const noexcept override { return pitchDeg_; }
        float zNear() const noexcept override { return znear_; }
        float zFar() const noexcept override { return zfar_; }

    private:
        // state
        glm::vec3 eye_;
        float yawDeg_;
        float pitchDeg_;

        // projection params
        float fovDeg_;
        float aspect_;
        float znear_;
        float zfar_;

        // cached matrices + dirty flags
        mutable glm::mat4 viewMat_;
        mutable glm::mat4 projMat_;
        mutable bool viewDirty_;
        mutable bool projDirty_;

        // recompute helpers
        void recomputeView() const noexcept;
        void recomputeProj() const noexcept;

        // clamp pitch to avoid gimbal flip (degrees)
        static constexpr float kPitchLimitDeg = 89.0f;
    };

} // namespace Render
