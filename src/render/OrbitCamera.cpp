#include "render/OrbitCamera.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/constants.hpp>

namespace Render
{

    static inline void azelForwardUp(float azimuthDeg, float elevationDeg, float rollDeg,
                                     glm::vec3 &forward, glm::vec3 &up)
    {
        const float az = glm::radians(azimuthDeg);
        const float el = glm::radians(elevationDeg);
        const float rl = glm::radians(rollDeg);

        const float cosEl = std::cos(el);
        forward = glm::normalize(glm::vec3(
            std::sin(az) * cosEl, // x
            std::sin(el),         // y
            std::cos(az) * cosEl  // z
            ));

        // базовый up из глобального +Y -> ортонормализуем к forward и применим roll вокруг forward
        glm::vec3 up0(0, 1, 0);
        glm::vec3 right = glm::normalize(glm::cross(forward, up0));
        up = glm::normalize(glm::cross(right, forward));
        up = glm::rotate(up, rl, forward);
    }

    const glm::mat4 &OrbitCamera::view() const
    {
        if (dirtyView)
        {
            glm::vec3 f, u;
            azelForwardUp(azimuthDeg, elevationDeg, rollDeg, f, u);
            const glm::vec3 camPos = target - f * distance;
            cachedView = glm::lookAt(camPos, target, u);
            dirtyView = false;
        }
        return cachedView;
    }

    const glm::mat4 &OrbitCamera::proj() const
    {
        if (dirtyProj)
        {
            cachedProj = glm::perspective(glm::radians(fovDeg), aspect, nearZ, farZ);
            cachedProj[1][1] *= -1.0f; // Vulkan NDC flip
            dirtyProj = false;
        }
        return cachedProj;
    }

    void OrbitCamera::setPerspective(float fovDeg_, float aspect_, float znear_, float zfar_)
    {
        fovDeg = fovDeg_;
        aspect = aspect_;
        nearZ = znear_;
        farZ = zfar_;
        dirtyProj = true;
    }

    void OrbitCamera::setAspect(float aspect_)
    {
        aspect = aspect_;
        dirtyProj = true;
    }

    glm::vec3 OrbitCamera::position() const
    {
        glm::vec3 f, u;
        azelForwardUp(azimuthDeg, elevationDeg, rollDeg, f, u);
        return target - f * distance;
    }

    void OrbitCamera::frameToBox(const glm::vec3 &worldMin,
                                 const glm::vec3 &worldMax,
                                 float fovY_deg,
                                 float aspect_,
                                 float pad,
                                 float targetLift)
    {
        const glm::vec3 C = 0.5f * (worldMin + worldMax);
        const glm::vec3 size = worldMax - worldMin;

        // половины габаритов (по максимальной ширине в XZ и по высоте Y)
        const float halfW = 0.5f * glm::max(size.x, size.z);
        const float halfH = 0.5f * size.y;

        const float fovY = glm::radians(fovY_deg);
        const float distY = halfH / std::tan(0.5f * fovY);
        const float distX = (halfW / aspect_) / std::tan(0.5f * fovY);
        const float dist = pad * glm::max(distX, distY);

        // приподнимем цель на кусочек высоты
        glm::vec3 lift(0.0f, targetLift * size.y, 0.0f);
        setTarget(C + lift);

        // применим FOV/ASPECT к проекции
        setPerspective(fovY_deg, aspect_, nearZ, farZ);

        // расстояние вдоль текущего forward (текущие азимут/элев/ролл уже выставлены сеттерами)
        setDistance(dist);
    }

} // namespace Render