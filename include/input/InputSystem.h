#pragma once

#include <glm/vec2.hpp>
#include <cstdint>

namespace Platform
{
    class WindowManager;
}

namespace Input
{

    /**
     * @brief InputSystem: small abstraction that samples the WindowManager and offers a
     * cleaner input API for gameplay/controls code (non-blocking, easy to test).
     *
     * Design notes:
     *  - Not tied to GLFW directly; uses your Platform::WindowManager instance.
     *  - Stores per-frame mouse delta (consumed on demand).
     *  - Supports simple key/button 'consume' semantics (optional) to avoid double-handling.
     *  - Contains basic sensitivity/invert settings used by CameraController.
     */

    class InputSystem
    {
    public:
        explicit InputSystem(Platform::WindowManager &wm) noexcept;

        // Polls window to update internal state (call once per frame)
        void poll();

        // Keyboard
        bool isKeyDown(int key) const noexcept;
        bool wasKeyPressed(int key) const noexcept;

        // Mouse
        glm::vec2 mouseDelta() noexcept; // returns delta and zeroes stored delta (consumed)
        bool isMouseDown(int button) const noexcept;

        // Toggle mouse capture via input system (proxy to WindowManager)
        void captureMouse(bool enabled) noexcept;
        bool isMouseCaptured() const noexcept;

        // Fine tuning (defaults reasonable for typical mouse)
        void setMouseSensitivity(float s) noexcept { mouseSensitivity_ = s; }
        void setInvertX(bool invert) noexcept { invertX_ = invert; }
        void setInvertY(bool invert) noexcept { invertY_ = invert; }

    private:
        Platform::WindowManager &wm_;

        // Raw cached snapshot each frame
        glm::vec2 rawMouseDelta_{0.0f, 0.0f};

        // Settings
        float mouseSensitivity_ = 0.12f; // degrees per pixel multiplier (tweak)
        bool invertX_ = false;
        bool invertY_ = false;

        // mouse captured flag cached
        bool mouseCaptured_ = false;
    };

} // namespace Input
