#pragma once

#include "Camera.h"
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace Render
{

    /**
     * @brief Camera that orbits around a target point at a radius.
     * - Controlled via azimuth (yaw) and elevation (pitch) angles.
     * - Useful for inspecting a model/object.
     * - Exposes same Camera interface so it can be used polymorphically.
     */

    class OrbitCamera : public Camera
    {
    public:
        OrbitCamera() noexcept;

        // Matrix accessors
        const glm::mat4 &view() const noexcept override;
        const glm::mat4 &proj() const noexcept override;

        // Projection control
        void setPerspective(float fovDeg, float aspect, float znear, float zfar) noexcept override;
        void setAspect(float aspect) noexcept override;

        // World state
        glm::vec3 position() const noexcept override; // computed from target + spherical coords

        // Generic control
        // MoveLocal is not meaningful for orbit camera (no-op).
        void moveLocal(const glm::vec3 & /*deltaLocal*/) noexcept override {}

        // Rotate by yaw (azimuth) and pitch (elevation)
        void addYawPitch(float deltaYawDeg, float deltaPitchDeg) noexcept override;

        // Set target/eye via lookAt
        void lookAt(const glm::vec3 &eye, const glm::vec3 &target, const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f)) noexcept override;

        // Specific helpers for orbit camera
        void setTarget(const glm::vec3 &target) noexcept;
        void setRadius(float radius) noexcept;
        void setAngles(float azimuthDeg, float elevationDeg) noexcept;

        float yawDeg() const noexcept override { return azimuthDeg_; }
        float pitchDeg() const noexcept override { return elevationDeg_; }
        float zNear() const noexcept override { return znear_; }
        float zFar() const noexcept override { return zfar_; }

    private:
        glm::vec3 target_;
        float radius_;       // distance from target
        float azimuthDeg_;   // horizontal angle (degrees)
        float elevationDeg_; // vertical angle (degrees), clamped

        // proj params
        float fovDeg_;
        float aspect_;
        float znear_;
        float zfar_;

        // cached matrices + dirty flags
        mutable glm::mat4 viewMat_;
        mutable glm::mat4 projMat_;
        mutable bool viewDirty_;
        mutable bool projDirty_;

        void recomputeView() const noexcept;
        void recomputeProj() const noexcept;

        static constexpr float kElevationMin = -89.0f;
        static constexpr float kElevationMax = 89.0f;
    };
} // namespace Render
