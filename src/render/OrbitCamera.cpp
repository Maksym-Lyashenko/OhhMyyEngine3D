#include "render/OrbitCamera.h"

#include <glm/gtc/matrix_transform.hpp> // lookAt, perspective, radians
#include <glm/gtc/constants.hpp>
#include <cmath> // sin, cos, tan

namespace Render
{

    // Rodrigues' rotation formula: rotate vector v around unit axis a by angle (radians)
    static inline glm::vec3 rotateAroundAxis(const glm::vec3 &v, float angle, const glm::vec3 &a) noexcept
    {
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        return v * c + glm::cross(a, v) * s + a * (glm::dot(a, v) * (1.0f - c));
    }

    // Build forward & up vectors from azimuth/elevation/roll (degrees)
    static inline void azelForwardUp(float azimuthDeg, float elevationDeg, float rollDeg,
                                     glm::vec3 &forward, glm::vec3 &up) noexcept
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

        // Start from global +Y, orthonormalize against forward, then apply roll about forward
        const glm::vec3 up0(0.0f, 1.0f, 0.0f);
        const glm::vec3 right = glm::normalize(glm::cross(forward, up0));
        up = glm::normalize(glm::cross(right, forward));
        up = rotateAroundAxis(up, rl, forward);
    }

    const glm::mat4 &OrbitCamera::view() const noexcept
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

    const glm::mat4 &OrbitCamera::proj() const noexcept
    {
        if (dirtyProj)
        {
            cachedProj = glm::perspective(glm::radians(fovDeg), aspect, nearZ, farZ);
            // Vulkan NDC has Y down compared to GL; flip projection Y
            cachedProj[1][1] *= -1.0f;
            dirtyProj = false;
        }
        return cachedProj;
    }

    void OrbitCamera::setPerspective(float fovDeg_, float aspect_, float znear_, float zfar_) noexcept
    {
        fovDeg = fovDeg_;
        aspect = aspect_;
        nearZ = znear_;
        farZ = zfar_;
        dirtyProj = true;
    }

    void OrbitCamera::setAspect(float aspect_) noexcept
    {
        aspect = aspect_;
        dirtyProj = true;
    }

    glm::vec3 OrbitCamera::position() const noexcept
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
                                 float targetLift) noexcept
    {
        const glm::vec3 C = 0.5f * (worldMin + worldMax);
        const glm::vec3 size = worldMax - worldMin;

        // Half-extent in XZ footprint and Y height
        const float halfW = 0.5f * glm::max(size.x, size.z);
        const float halfH = 0.5f * size.y;

        const float fovY = glm::radians(fovY_deg);
        const float distY = halfH / std::tan(0.5f * fovY);
        const float distX = (halfW / aspect_) / std::tan(0.5f * fovY);
        const float dist = pad * glm::max(distX, distY);

        // Lift target by a fraction of the box height (looks better)
        const glm::vec3 lift(0.0f, targetLift * size.y, 0.0f);
        setTarget(C + lift);

        // Apply FOV/ASPECT to projection; keep current near/far
        setPerspective(fovY_deg, aspect_, nearZ, farZ);

        // Distance along current forward
        setDistance(dist);
    }

} // namespace Render
