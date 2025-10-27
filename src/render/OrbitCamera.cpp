// OrbitCamera.cpp
#include "render/OrbitCamera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>

namespace Render
{

    OrbitCamera::OrbitCamera() noexcept
        : target_(0.0f),
          radius_(3.0f),
          azimuthDeg_(0.0f),
          elevationDeg_(0.0f),
          fovDeg_(60.0f),
          aspect_(16.0f / 9.0f),
          znear_(0.01f),
          zfar_(1000.0f),
          viewMat_(1.0f),
          projMat_(1.0f),
          viewDirty_(true),
          projDirty_(true)
    {
    }

    const glm::mat4 &OrbitCamera::view() const noexcept
    {
        if (viewDirty_)
            recomputeView();
        return viewMat_;
    }

    const glm::mat4 &OrbitCamera::proj() const noexcept
    {
        if (projDirty_)
            recomputeProj();
        return projMat_;
    }

    void OrbitCamera::setPerspective(float fovDeg, float aspect, float znear, float zfar) noexcept
    {
        fovDeg_ = fovDeg;
        aspect_ = aspect;
        znear_ = znear;
        zfar_ = zfar;
        projDirty_ = true;
    }

    void OrbitCamera::setAspect(float aspect) noexcept
    {
        aspect_ = aspect;
        projDirty_ = true;
    }

    glm::vec3 OrbitCamera::position() const noexcept
    {
        // compute from spherical coordinates
        const float az = glm::radians(azimuthDeg_);
        const float el = glm::radians(elevationDeg_);

        glm::vec3 dir;
        dir.x = std::cos(el) * std::sin(az);
        dir.y = std::sin(el);
        dir.z = std::cos(el) * std::cos(az);

        return target_ + dir * radius_;
    }

    void OrbitCamera::addYawPitch(float deltaYawDeg, float deltaPitchDeg) noexcept
    {
        azimuthDeg_ -= deltaYawDeg;
        elevationDeg_ -= deltaPitchDeg;
        elevationDeg_ = std::clamp(elevationDeg_, kElevationMin, kElevationMax);
        viewDirty_ = true;
    }

    void OrbitCamera::lookAt(const glm::vec3 &eye, const glm::vec3 &target, const glm::vec3 & /*up*/) noexcept
    {
        target_ = target;
        // compute radius and angles from eye -> target
        glm::vec3 v = eye - target_;
        radius_ = glm::length(v);
        if (radius_ <= 1e-6f)
            radius_ = 0.0001f;

        // derive angles:
        // elevation = asin(y / r)
        elevationDeg_ = glm::degrees(std::asin(glm::clamp(v.y / radius_, -1.0f, 1.0f)));
        // azimuth: atan2(x, z) measured the same way as in FreeCamera
        azimuthDeg_ = glm::degrees(std::atan2(v.x, v.z));
        viewDirty_ = true;
    }

    void OrbitCamera::setTarget(const glm::vec3 &target) noexcept
    {
        target_ = target;
        viewDirty_ = true;
    }

    void OrbitCamera::setRadius(float radius) noexcept
    {
        radius_ = std::max(radius, 1e-6f);
        viewDirty_ = true;
    }

    void OrbitCamera::setAngles(float azimuthDeg, float elevationDeg) noexcept
    {
        azimuthDeg_ = azimuthDeg;
        elevationDeg_ = std::clamp(elevationDeg, kElevationMin, kElevationMax);
        viewDirty_ = true;
    }

    void OrbitCamera::recomputeView() const noexcept
    {
        // compute camera position from spherical coords (note: azimuth/elevation conventions match FreeCamera)
        const float az = glm::radians(azimuthDeg_);
        const float el = glm::radians(elevationDeg_);

        glm::vec3 dir;
        dir.x = std::cos(el) * std::sin(az);
        dir.y = std::sin(el);
        dir.z = std::cos(el) * std::cos(az);

        glm::vec3 eye = target_ + dir * radius_;
        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        viewMat_ = glm::lookAt(eye, target_, up);
        viewDirty_ = false;
    }

    void OrbitCamera::recomputeProj() const noexcept
    {
        projMat_ = glm::perspective(glm::radians(fovDeg_), aspect_, znear_, zfar_);

        // Vulkan clip space has inverted Y compared to OpenGL
        projMat_[1][1] *= -1.0f;

        projDirty_ = false;
    }

} // namespace Render
