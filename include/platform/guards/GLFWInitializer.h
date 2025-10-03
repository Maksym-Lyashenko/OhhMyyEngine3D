#pragma once

#include <stdexcept>

namespace Platform::Guards
{

    /**
     * @brief RAII guard for GLFW global initialization/termination.
     *
     * On construction: calls glfwInit() and installs a temporary error callback.
     * On destruction:  restores the previous error callback and calls glfwTerminate().
     *
     * Notes:
     *  - Non-copyable and non-movable to keep ownership clear.
     *  - Throws std::runtime_error if glfwInit() fails.
     */
    class GLFWInitializer
    {
    public:
        GLFWInitializer();
        ~GLFWInitializer() noexcept;

        GLFWInitializer(const GLFWInitializer &) = delete;
        GLFWInitializer &operator=(const GLFWInitializer &) = delete;
        GLFWInitializer(GLFWInitializer &&) = delete;
        GLFWInitializer &operator=(GLFWInitializer &&) = delete;

    private:
        bool initialized_{false};
        // Opaque pointer type for the previous callback, stored/restored in .cpp
        void *prevErrorCallback_{nullptr};
    };

} // namespace Platform::Guards
