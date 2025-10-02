#pragma once

#include <glm/glm.hpp>

#include "render/Camera.h"

namespace Render
{

    class OrbitCamera final : public Camera
    {
    public:
        // setters
        void setTarget(const glm::vec3 &t)
        {
            target = t;
            dirtyView = true;
        }
        void setDistance(float d)
        {
            distance = glm::max(d, 0.001f);
            dirtyView = true;
        }
        void setAzimuth(float deg)
        {
            azimuthDeg = deg;
            dirtyView = true;
        }
        void setElevation(float deg)
        {
            elevationDeg = glm::clamp(deg, -89.9f, 89.9f);
            dirtyView = true;
        }
        void setRoll(float deg)
        {
            rollDeg = deg;
            dirtyView = true;
        }

        // Camera interface
        const glm::mat4 &view() const override;
        const glm::mat4 &proj() const override;
        void setPerspective(float fovDeg, float aspect, float znear, float zfar) override;
        void setAspect(float aspect) override;
        glm::vec3 position() const override;

        // фрейминг под AABB (один раз после загрузки)
        void frameToBox(const glm::vec3 &worldMin,
                        const glm::vec3 &worldMax,
                        float fovY_deg,
                        float aspect,
                        float pad = 1.05f,         // небольшой запас по краям
                        float targetLift = 0.06f); // приподнять цель на % высоты

    private:
        // orbit-parameters
        glm::vec3 target{0.0f};
        float distance{5.0f};
        float azimuthDeg{225.0f};
        float elevationDeg{-20.0f};
        float rollDeg{-2.5f};

        // projection
        float fovDeg{45.0f};
        float aspect{16.0f / 9.0f};
        float nearZ{0.01f};
        float farZ{2000.0f};

        // cache
        mutable bool dirtyView{true};
        mutable bool dirtyProj{true};
        mutable glm::mat4 cachedView{1.0f};
        mutable glm::mat4 cachedProj{1.0f};
    };

} // namespace Render