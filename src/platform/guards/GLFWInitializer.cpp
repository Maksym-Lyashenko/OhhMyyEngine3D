#include "platform/guards/GLFWInitializer.h"

#include <GLFW/glfw3.h>
#include <cstdio>  // std::fprintf
#include <cstring> // std::strlen

namespace
{

    // Small error callback that prints to stderr (you can wire this to your Logger if desired)
    void glfwErrorCallback(int code, const char *desc)
    {
        if (!desc)
            desc = "(null)";
        std::fprintf(stderr, "[GLFW][%d] %s\n", code, desc);
    }

} // namespace

namespace Platform::Guards
{

    GLFWInitializer::GLFWInitializer()
    {
        // Save previous error callback (glfw returns the old one)
        prevErrorCallback_ = reinterpret_cast<void *>(glfwSetErrorCallback(glfwErrorCallback));

        if (!glfwInit())
        {
            // Restore previous callback before throwing
            glfwSetErrorCallback(reinterpret_cast<GLFWerrorfun>(prevErrorCallback_));
            prevErrorCallback_ = nullptr;
            throw std::runtime_error("Failed to initialize GLFW");
        }

        initialized_ = true;
    }

    GLFWInitializer::~GLFWInitializer() noexcept
    {
        // Restore previous error callback
        glfwSetErrorCallback(reinterpret_cast<GLFWerrorfun>(prevErrorCallback_));
        prevErrorCallback_ = nullptr;

        if (initialized_)
        {
            glfwTerminate();
        }
    }

} // namespace Platform::Guards
