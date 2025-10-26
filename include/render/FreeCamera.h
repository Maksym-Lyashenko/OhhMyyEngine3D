#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <glm/gtx/norm.hpp>

namespace Render
{
    class FreeCamera
    {
    public:
        FreeCamera() = default;

        FreeCamera(
            const glm::vec3 &position,
            float yawDeg,
            float pitchDeg,
            float fovYDeg,
            float aspect,
            float nearPlane = 0.05f,
            float farPlane = 500.0f)
            : m_pos(position),
              m_yawDeg(yawDeg),
              m_pitchDeg(pitchDeg),
              m_fovYDeg(fovYDeg),
              m_aspect(aspect),
              m_near(nearPlane),
              m_far(farPlane)
        {
            clampPitch();
        }

        // --- setters you might call on resize ---
        void setAspect(float aspect)
        {
            m_aspect = aspect;
        }

        void setFovY(float fovDeg)
        {
            m_fovYDeg = fovDeg;
        }

        // Hard-set camera pose to "look at" a point.
        // This is handy for initial framing around your model box.
        void lookAt(const glm::vec3 &eye, const glm::vec3 &target, const glm::vec3 &up = glm::vec3(0, 1, 0))
        {
            m_pos = eye;

            const glm::vec3 fwd = glm::normalize(target - eye); // direction we look
            // yaw = angle around +Z axis (right-handed Vulkan/GLM style with -Z forward),
            // but we'll define yaw such that yaw=0 looks down -Z.
            // We'll recover yaw,pitch from forward.
            //
            // Forward in world from yaw/pitch is:
            //   fx = cos(pitch)*sin(yaw)
            //   fy = sin(pitch)
            //   fz = cos(pitch)*cos(yaw) * (-1)   (we want yaw=0,pitch=0 -> forward = (0,0,-1))
            //
            // Let's invert that.

            // pitch from fy:
            float pitchRad = std::asin(glm::clamp(fwd.y, -1.0f, 1.0f));

            // projected horizontal forward (xz plane, but note our -Z forward convention)
            float cp = std::cos(pitchRad);
            if (std::abs(cp) < 1e-5f)
                cp = 1e-5f;

            // when yaw=0, forward should be (0,0,-1)
            // so:
            //   fx =  cos(pitch)*sin(yaw)
            //   fz = -cos(pitch)*cos(yaw)
            // => yaw = atan2(fx, -fz)
            float yawRad = std::atan2(fwd.x, -fwd.z);

            m_yawDeg = glm::degrees(yawRad);
            m_pitchDeg = glm::degrees(pitchRad);
            clampPitch();

            (void)up; // up is not stored directly; we reconstruct "up" from yaw/pitch each frame
        }

        // --- movement API you'd call from input system ---

        // Move in *camera local* space (forward/right/up).
        // Example: moveLocal({0,0,-dt*speed}) to go forward.
        void moveLocal(const glm::vec3 &deltaLocal)
        {
            const Basis b = basis();

            // local x = right, y = up, z = forward
            m_pos += deltaLocal.x * b.right;
            m_pos += deltaLocal.y * b.worldUp; // we use worldUp for "rise", not camera tilt
            m_pos += deltaLocal.z * b.forward;
        }

        // Mouse-look style: add yaw (left/right), pitch (up/down)
        void addYawPitch(float deltaYawDeg, float deltaPitchDeg)
        {
            m_yawDeg += deltaYawDeg;
            m_pitchDeg += deltaPitchDeg;
            clampPitch();
        }

        // --- matrices we feed to the renderer ---

        glm::mat4 view() const
        {
            const Basis b = basis();
            // classic lookAt
            return glm::lookAt(m_pos, m_pos + b.forward, b.worldUp);
        }

        glm::mat4 proj() const
        {
            // Vulkan clip space has inverted Y *and* different Z range.
            // Easiest path: standard glm::perspective (OpenGL-style)
            // then fix Y for Vulkan by flipping sign on [1][1].
            glm::mat4 p = glm::perspective(glm::radians(m_fovYDeg),
                                           m_aspect,
                                           m_near,
                                           m_far);
            // GLM builds a matrix for OpenGL (Y down in clip). Vulkan expects Y not flipped,
            // so we flip here:
            p[1][1] *= -1.0f;
            return p;
        }

        glm::vec3 position() const { return m_pos; }

        float yawDeg() const { return m_yawDeg; }
        float pitchDeg() const { return m_pitchDeg; }

        float zNear() const noexcept { return m_near; }
        float zFar() const noexcept { return m_far; }

        float fovYDeg() const noexcept { return m_fovYDeg; }
        float aspect() const noexcept { return m_aspect; }

    private:
        struct Basis
        {
            glm::vec3 forward;
            glm::vec3 right;
            glm::vec3 worldUp;
        };

        Basis basis() const
        {
            // yaw,pitch in radians
            const float yawRad = glm::radians(m_yawDeg);
            const float pitchRad = glm::radians(m_pitchDeg);

            // We define:
            // yaw=0, pitch=0  -> look down -Z
            // yaw +right turn -> +X
            // pitch +up       -> +Y
            glm::vec3 fwd;
            fwd.x = std::cos(pitchRad) * std::sin(yawRad);
            fwd.y = std::sin(pitchRad);
            fwd.z = -std::cos(pitchRad) * std::cos(yawRad);
            fwd = glm::normalize(fwd);

            const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
            glm::vec3 right = glm::normalize(glm::cross(fwd, worldUp));

            // if we're looking almost straight up/down, cross can blow up;
            // guard that:
            if (glm::length2(right) < 1e-8f)
            {
                // pick any valid right
                right = glm::vec3(1, 0, 0);
            }

            return Basis{
                /*forward*/ fwd,
                /*right  */ right,
                /*worldUp*/ worldUp};
        }

        void clampPitch()
        {
            // don't let pitch flip upside down
            const float limit = 89.0f;
            if (m_pitchDeg > limit)
                m_pitchDeg = limit;
            if (m_pitchDeg < -limit)
                m_pitchDeg = -limit;
        }

        glm::vec3 m_pos = glm::vec3(0.0f, 0.0f, 3.0f);
        float m_yawDeg = 0.0f;   // degrees, 0 = look -Z, +yaw = turn right
        float m_pitchDeg = 0.0f; // degrees, +pitch = look up
        float m_fovYDeg = 32.0f; // vertical FOV in degrees
        float m_aspect = 16.0f / 9.0f;
        float m_near = 0.05f;
        float m_far = 2000.0f;
    };
} // namespace Render
