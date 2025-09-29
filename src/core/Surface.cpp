#include "core/Surface.h"

#include "core/Logger.h"
#include <core/VulkanUtils.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace Core
{
    Surface::Surface(VkInstance instance, const WindowManager &window)
        : instance_(instance), window_(window)
    {
    }

    Surface::~Surface()
    {
        if (surface_ != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(instance_, surface_, nullptr);
            Logger::log(LogLevel::INFO, "Surface destroyed");
        }
    }

    void Surface::create()
    {
        if (created_)
        {
            Logger::log(LogLevel::WARNING, "Surface already created, skipping");
            return;
        }

        GLFWwindow *glfwWindow = window_.getWindow();
        VK_CHECK(glfwCreateWindowSurface(instance_, glfwWindow, nullptr, &surface_));

        Logger::log(LogLevel::INFO, "Surface created successfully");
        created_ = true;
    }
}
