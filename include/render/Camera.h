#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Render
{

    /**
     * @brief Abstract camera interface.
     *
     * Notes:
     *  - All accessors are noexcept and [[nodiscard]].
     *  - Field of view is expected in DEGREES (vertical FOV).
     *  - Implementations may cache matrices; returning const& avoids copies.
     */
    class Camera
    {
    public:
        virtual ~Camera() = default;

        // Matrices
        [[nodiscard]] virtual const glm::mat4 &view() const noexcept = 0;
        [[nodiscard]] virtual const glm::mat4 &proj() const noexcept = 0;

        // Convenience: computed on the fly (proj * view)
        [[nodiscard]] glm::mat4 viewProj() const noexcept { return proj() * view(); }

        // Projection options
        // fovDeg: vertical FOV in degrees; aspect: width/height; znear/zfar in world units
        virtual void setPerspective(float fovDeg, float aspect, float znear, float zfar) noexcept = 0;
        virtual void setAspect(float aspect) noexcept = 0;

        // World state
        [[nodiscard]] virtual glm::vec3 position() const noexcept = 0;

        // Non-copyable base to avoid slicing and confusing ownership
        Camera(const Camera &) = delete;
        Camera &operator=(const Camera &) = delete;

    protected:
        Camera() = default;
    };

} // namespace Render
