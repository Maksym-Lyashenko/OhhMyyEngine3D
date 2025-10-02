#pragma once

#include <GLFW/glfw3.h>

#include <stdexcept>

namespace Platform::Guards
{

    class GLFWInitializer
    {
    public:
        GLFWInitializer()
        {
            if (!glfwInit())
                throw std::runtime_error("Failed to initialize GLFW");
        }

        ~GLFWInitializer() noexcept
        {
            glfwTerminate();
        }

        GLFWInitializer(const GLFWInitializer &) = delete;
        GLFWInitializer &operator=(const GLFWInitializer &) = delete;
    };

} // namespace Platform::Guards
