#include "input/InputSystem.h"
#include "platform/WindowManager.h"

namespace Input
{

    InputSystem::InputSystem(Platform::WindowManager &wm) noexcept : wm_(wm)
    {
        mouseCaptured_ = wm_.isFullscreen(); // arbitrary initial - you can set default later
    }

    void InputSystem::poll()
    {
        // Pull mouse delta from WindowManager (WindowManager already computed dx/dy per frame)
        rawMouseDelta_ = wm_.mouseDelta();
    }

    bool InputSystem::isKeyDown(int key) const noexcept { return wm_.isKeyDown(key); }
    bool InputSystem::wasKeyPressed(int key) const noexcept { return wm_.wasKeyPressed(key); }
    bool InputSystem::isMouseDown(int b) const noexcept { return wm_.isMouseDown(b); }

    glm::vec2 InputSystem::mouseDelta() noexcept
    {
        glm::vec2 d = rawMouseDelta_;
        // apply invert and sensitivity; note: positive x = move right, positive y = move down
        float x = d.x * mouseSensitivity_ * (invertX_ ? -1.0f : 1.0f);
        float y = d.y * mouseSensitivity_ * (invertY_ ? -1.0f : 1.0f);
        // zero stored delta (consumed)
        rawMouseDelta_ = {0.0f, 0.0f};
        return {x, y};
    }

    void InputSystem::captureMouse(bool enabled) noexcept
    {
        mouseCaptured_ = enabled;
        wm_.captureMouse(enabled);
    }

    bool InputSystem::isMouseCaptured() const noexcept { return mouseCaptured_; }

} // namespace Input
