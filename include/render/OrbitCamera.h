#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/common.hpp> // glm::clamp, glm::max

#include "render/Camera.h"

namespace Render
{

    /**
     * @brief Simple orbit (turntable) camera.
     *
     * Parameters:
     *  - target:     point to orbit around (world-space)
     *  - distance:   distance from target along forward axis
     *  - azimuthDeg: yaw around +Y in degrees
     *  - elevationDeg: pitch in degrees (clamped to avoid gimbal flip)
     *  - rollDeg:    roll in degrees (rotation around forward)
     *
     * Projection:
     *  - Perspective with vertical fov in degrees, aspect (width/height), near/far planes.
     *
     * Notes:
     *  - Matrices are cached and recomputed only when marked dirty.
     *  - Vulkan NDC (Y-flip) is applied in proj().
     */
    class OrbitCamera final : public Camera
    {
    public:
        // ---- Parameter setters ----
        void setTarget(const glm::vec3 &t) noexcept
        {
            target = t;
            dirtyView = true;
        }

        void setDistance(float d) noexcept
        {
            distance = glm::max(d, 0.001f);
            dirtyView = true;
        }

        void setAzimuth(float deg) noexcept
        {
            azimuthDeg = deg;
            dirtyView = true;
        }

        void setElevation(float deg) noexcept
        {
            // Prevent looking exactly up/down to avoid unstable basis
            elevationDeg = glm::clamp(deg, -89.9f, 89.9f);
            dirtyView = true;
        }

        void setRoll(float deg) noexcept
        {
            rollDeg = deg;
            dirtyView = true;
        }

        // ---- Camera interface ----
        [[nodiscard]] const glm::mat4 &view() const noexcept override;
        [[nodiscard]] const glm::mat4 &proj() const noexcept override;

        void setPerspective(float fovDeg, float aspect, float znear, float zfar) noexcept override;
        void setAspect(float aspect) noexcept override;

        [[nodiscard]] glm::vec3 position() const noexcept override;

        /**
         * @brief Frame the camera to fully fit an AABB.
         * @param worldMin/worldMax  AABB in world space.
         * @param fovY_deg           Vertical FOV (degrees) to use.
         * @param aspect             Aspect ratio (width/height).
         * @param pad                Safety multiplier (>1) to avoid tight fit.
         * @param targetLift         Raise target by this fraction of AABB height.
         *
         * Keeps current azimuth/elevation/roll; adjusts target, perspective and distance.
         */
        void frameToBox(const glm::vec3 &worldMin,
                        const glm::vec3 &worldMax,
                        float fovY_deg,
                        float aspect,
                        float pad = 1.05f,
                        float targetLift = 0.06f) noexcept;

    private:
        // Orbit parameters
        glm::vec3 target{0.0f};
        float distance{5.0f};
        float azimuthDeg{225.0f};
        float elevationDeg{-20.0f};
        float rollDeg{-2.5f};

        // Projection parameters
        float fovDeg{45.0f};        // vertical FOV, degrees
        float aspect{16.0f / 9.0f}; // width/height
        float nearZ{0.01f};
        float farZ{2000.0f};

        // Cached matrices
        mutable bool dirtyView{true};
        mutable bool dirtyProj{true};
        mutable glm::mat4 cachedView{1.0f};
        mutable glm::mat4 cachedProj{1.0f};
    };

} // namespace Render
