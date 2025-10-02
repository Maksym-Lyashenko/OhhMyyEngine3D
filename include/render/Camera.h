#pragma once
#include <glm/glm.hpp>

namespace Render
{

    class Camera
    {
    public:
        virtual ~Camera() = default;

        // Matrices
        virtual const glm::mat4 &view() const = 0;
        virtual const glm::mat4 &proj() const = 0;

        // Projection Options
        virtual void setPerspective(float fovDeg, float aspect, float znear, float zfar) = 0;
        virtual void setAspect(float aspect) = 0;

        // State in the world
        virtual glm::vec3 position() const = 0;
    };

} // namespace Render