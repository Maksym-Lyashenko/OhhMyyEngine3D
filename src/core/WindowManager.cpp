#include "core/WindowManager.h"
#include "core/Logger.h"

namespace Core
{
    WindowManager::WindowManager(uint32_t width, uint32_t height, const char *title)
        : width(static_cast<int>(width)), height(static_cast<int>(height))
    {
        if (!glfwInit())
            throw std::runtime_error("Failed to initialize GLFW");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Vulkan only

        window = glfwCreateWindow(this->width, this->height, title, nullptr, nullptr);
        if (!window)
            throw std::runtime_error("Failed to create GLFW window");

        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        glfwSetKeyCallback(window, keyCallback);

        // Save windowed size for restoring after fullscreen
        glfwGetWindowPos(window, &windowPosX, &windowPosY);
        glfwGetWindowSize(window, &windowedWidth, &windowedHeight);

        // Log window creation
        Logger::log(LogLevel::INFO, "Window created");
    }

    WindowManager::~WindowManager()
    {
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
    }

    bool WindowManager::shouldClose() const
    {
        return glfwWindowShouldClose(window);
    }

    void WindowManager::pollEvents() const
    {
        glfwPollEvents();
    }

    std::vector<const char *> WindowManager::getRequiredExtensions() const
    {
        uint32_t count = 0;
        const char **extensions = glfwGetRequiredInstanceExtensions(&count);
        return std::vector<const char *>(extensions, extensions + count);
    }

    void WindowManager::toggleFullscreen()
    {
        fullscreen = !fullscreen;

        if (fullscreen)
        {
            // Save windowed position and size
            glfwGetWindowPos(window, &windowPosX, &windowPosY);
            glfwGetWindowSize(window, &windowedWidth, &windowedHeight);

            GLFWmonitor *monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode *mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(window, monitor,
                                 0, 0,
                                 mode->width, mode->height,
                                 mode->refreshRate);
        }
        else
        {
            // Restore windowed mode
            glfwSetWindowMonitor(window, nullptr,
                                 windowPosX, windowPosY,
                                 windowedWidth, windowedHeight,
                                 0);
        }
    }

    void WindowManager::framebufferResizeCallback(GLFWwindow *wnd, int width, int height)
    {
        auto wm = reinterpret_cast<WindowManager *>(glfwGetWindowUserPointer(wnd));
        wm->width = width;
        wm->height = height;
        if (wm->onFramebufferResize)
        {
            wm->onFramebufferResize(width, height);
        }
    }

    void WindowManager::keyCallback(GLFWwindow *wnd, int key, int, int action, int mods)
    {
        if (action == GLFW_PRESS && key == GLFW_KEY_ENTER && (mods & GLFW_MOD_ALT))
        {
            auto wm = reinterpret_cast<WindowManager *>(glfwGetWindowUserPointer(wnd));
            wm->toggleFullscreen();
        }
    }
}
