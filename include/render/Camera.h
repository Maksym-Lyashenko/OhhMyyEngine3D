#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Render
{

    /**
     * @brief Abstract, polymorphic camera interface used across the renderer.
     * - Provides access to cached view/proj matrices (returned by const& to avoid copies).
     * - Exposes a small, general control API so callers can operate on cameras polymorphically:
     * * moveLocal(...)         : translate in camera-local space (right, up, forward)
     * * addYawPitch(...)       : rotate yaw/pitch in degrees
     * * lookAt(...)            : place camera to look at a target point
     * * setPerspective(...)    : set FOV/aspect/near/far
     * * setAspect(...)         : change aspect ratio only
     * - Default implementations are no-ops when the operation is not meaningful for a specific camera.
     * - All accessors are noexcept where practical; implementers should honor noexcept promises.
     */

    class Camera
    {
    public:
        virtual ~Camera() = default;

        // --- Matrix accessors (cached in implementations) ---
        // Return const references to avoid copying 4x4 matrices each frame.
        // Implementations should update their internal cache lazily when state changes.
        [[nodiscard]] virtual const glm::mat4 &view() const noexcept = 0;
        [[nodiscard]] virtual const glm::mat4 &proj() const noexcept = 0;

        // Convenience: returns proj * view (not cached by base)
        [[nodiscard]] glm::mat4 viewProj() const noexcept { return proj() * view(); }

        // --- Projection control ---
        // fovDeg is vertical FOV in degrees.
        virtual void setPerspective(float fovDeg, float aspect, float znear, float zfar) noexcept = 0;
        // Change aspect ratio only (useful on resize).
        virtual void setAspect(float aspect) noexcept = 0;

        // --- World state / queries ---
        [[nodiscard]] virtual glm::vec3 position() const noexcept = 0;

        // --- Generic control API (polymorphic) ---
        // Move in camera-local coordinates: deltaLocal = (right, up, forward).
        // Default: no-op. Concrete cameras that support translation should override.
        virtual void moveLocal(const glm::vec3 & /*deltaLocal*/) noexcept {}

        // Rotate camera by yaw/pitch in degrees (yaw around world Y by convention).
        // Default: no-op. Concrete cameras override if rotation is meaningful.
        virtual void addYawPitch(float /*deltaYawDeg*/, float /*deltaPitchDeg*/) noexcept {}

        // Put camera at 'eye' and look at 'target'; optional 'up' vector.
        // Default: no-op. Concrete implementations should update internal state and mark caches dirty.
        virtual void lookAt(const glm::vec3 & /*eye*/, const glm::vec3 & /*target*/, const glm::vec3 & /*up*/ = glm::vec3(0.0f, 1.0f, 0.0f)) noexcept {}

        // --- Debug / read-only helpers ---
        // Not all cameras will have meaningful yaw/pitch; defaults provided.
        virtual float yawDeg() const noexcept { return 0.0f; }
        virtual float pitchDeg() const noexcept { return 0.0f; }
        virtual float zNear() const noexcept { return 0.0f; }
        virtual float zFar() const noexcept { return 0.0f; }

        // Non-copyable base to prevent slicing.
        Camera(const Camera &) = delete;
        Camera &operator=(const Camera &) = delete;

    protected:
        Camera() = default;
    };

} // namespace Render
