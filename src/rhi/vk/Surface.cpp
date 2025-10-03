#include "rhi/vk/Surface.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

#include <GLFW/glfw3.h>

namespace Vk
{

    Surface::Surface(VkInstance instance, const Platform::WindowManager &window)
        : instance_(instance), window_(&window) {}

    Surface::~Surface() noexcept
    {
        cleanup();
    }

    Surface::Surface(Surface &&other) noexcept
        : instance_(other.instance_), window_(other.window_), surface_(other.surface_)
    {
        other.instance_ = VK_NULL_HANDLE;
        other.window_ = nullptr;
        other.surface_ = VK_NULL_HANDLE;
    }

    Surface &Surface::operator=(Surface &&other) noexcept
    {
        if (this != &other)
        {
            cleanup();
            instance_ = other.instance_;
            window_ = other.window_;
            surface_ = other.surface_;
            other.instance_ = VK_NULL_HANDLE;
            other.window_ = nullptr;
            other.surface_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Surface::create()
    {
        if (surface_ != VK_NULL_HANDLE)
        {
            Core::Logger::log(Core::LogLevel::WARNING, "Surface already created; skipping");
            return;
        }
        if (!instance_ || !window_)
        {
            Core::Logger::log(Core::LogLevel::ERROR, "Surface::create: invalid instance/window");
            throw std::runtime_error("Surface::create: invalid instance/window");
        }

        GLFWwindow *glfwWindow = window_->getWindow();
        VK_CHECK(glfwCreateWindowSurface(instance_, glfwWindow, nullptr, &surface_));
        Core::Logger::log(Core::LogLevel::INFO, "Surface created successfully");
    }

    void Surface::cleanup() noexcept
    {
        if (surface_ != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(instance_, surface_, nullptr);
            surface_ = VK_NULL_HANDLE;
            Core::Logger::log(Core::LogLevel::INFO, "Surface destroyed");
        }
    }

} // namespace Vk
