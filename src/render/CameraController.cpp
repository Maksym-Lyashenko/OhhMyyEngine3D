#include "render/CameraController.h"

#include "input/InputSystem.h"
#include "render/Camera.h"
#include <glm/glm.hpp> // for normalize, etc.
#include <glm/gtc/constants.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include <GLFW/glfw3.h>

namespace Render
{

    CameraController::CameraController(Camera &cam, Input::InputSystem &input) noexcept
        : camera_(cam), input_(input) {}

    void CameraController::update(float dt) noexcept
    {
        if (dt <= 0.0f)
            return;

        // Polling input already done by InputSystem::poll() from main loop

        // --- Mouse look ---
        // We use right mouse button OR captured mouse to enter look mode.
        const bool lookMode = input_.isMouseDown(/*GLFW_BUTTON_RIGHT=*/GLFW_MOUSE_BUTTON_RIGHT) || input_.isMouseCaptured();
        if (lookMode)
        {
            glm::vec2 md = input_.mouseDelta(); // degrees-ish because InputSystem applies sensitivity
            // note: addYawPitch expects (deltaYawDeg, deltaPitchDeg)
            // Yaw sign convention: FreeCamera::addYawPitch should expect positive yaw -> turn right.
            camera_.addYawPitch(md.x, -md.y); // flip pitch so moving mouse up looks up
        }
        else
        {
            // consume mouse delta even if not looking to avoid accumulating next time
            (void)input_.mouseDelta();
        }

        // --- Movement ---
        glm::vec3 moveLocal{0.0f};
        // forward/back: W/S, left/right: A/D, up/down: E/Q
        if (input_.isKeyDown(GLFW_KEY_W))
            moveLocal.z -= 1.0f; // moving forward reduces local Z (depending on camera implementation)
        if (input_.isKeyDown(GLFW_KEY_S))
            moveLocal.z += 1.0f;
        if (input_.isKeyDown(GLFW_KEY_A))
            moveLocal.x -= 1.0f;
        if (input_.isKeyDown(GLFW_KEY_D))
            moveLocal.x += 1.0f;
        if (input_.isKeyDown(GLFW_KEY_E))
            moveLocal.y -= 1.0f;
        if (input_.isKeyDown(GLFW_KEY_Q))
            moveLocal.y += 1.0f;

        if (glm::length2(moveLocal) > 0.0f)
        {
            moveLocal = glm::normalize(moveLocal);

            float speed = baseSpeed_;
            if (input_.isKeyDown(GLFW_KEY_LEFT_SHIFT) || input_.isKeyDown(GLFW_KEY_RIGHT_SHIFT))
                speed *= boostMul_;
            if (input_.isKeyDown(GLFW_KEY_LEFT_CONTROL) || input_.isKeyDown(GLFW_KEY_RIGHT_CONTROL))
                speed *= slowMul_;

            // forward inversion toggle (some GLTFs use -Z forward, adjust if required)
            if (invertForward_)
                moveLocal.z = -moveLocal.z;

            // Apply movement in local camera space; FreeCamera::moveLocal expects displacement in local axes
            camera_.moveLocal(moveLocal * speed * dt);
        }
    }
} // namespace Render
