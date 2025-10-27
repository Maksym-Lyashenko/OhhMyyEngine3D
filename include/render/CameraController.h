#pragma once

#include <glm/vec3.hpp>

#include <memory>

namespace Input
{
    class InputSystem;
}
namespace Render
{
    class Camera;
}

namespace Render
{

    /**
     * @brief CameraController: high-level camera control that reads InputSystem and moves
     * a FreeCamera or OrbitCamera instance. Keeps all input-to-motion mapping in one place.
     *
     * Features:
     *  - WASD movement in local camera space, E/Q up/down
     *  - Shift for speed boost, Ctrl for slow (walk)
     *  - Mouse look when RMB (or when mouse captured)
     *  - Configurable base speed + multipliers
     *  - Optionally flips forward/back sign if your model/world uses different forward
     */

    class CameraController
    {
    public:
        explicit CameraController(Camera &cam, Input::InputSystem &input) noexcept;

        // Call each frame with delta time (seconds)
        void update(float dt) noexcept;

        // tweakable parameters
        void setBaseSpeed(float unitsPerSec) noexcept { baseSpeed_ = unitsPerSec; }
        void setBoostMultiplier(float m) noexcept { boostMul_ = m; }
        void setSlowMultiplier(float m) noexcept { slowMul_ = m; }

        // invert forward/back if needed (some coordinate conventions)
        void setInvertForward(bool invert) noexcept { invertForward_ = invert; }

    private:
        Camera &camera_;
        Input::InputSystem &input_;

        // movement tuning
        float baseSpeed_ = 3.0f; // world units / second
        float boostMul_ = 3.0f;
        float slowMul_ = 0.3f;

        bool invertForward_ = false;
    };
} // namespace Render
