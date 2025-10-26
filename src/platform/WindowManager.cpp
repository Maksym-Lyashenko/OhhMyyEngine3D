#include "platform/WindowManager.h"

#include "core/Logger.h"

#include <algorithm>

namespace Platform
{

    WindowManager::WindowManager(std::uint32_t w, std::uint32_t h, const char *title)
        : width_(static_cast<int>(w)), height_(static_cast<int>(h))
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Vulkan only
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window_ = glfwCreateWindow(width_, height_, title ? title : "Window", nullptr, nullptr);
        if (!window_)
        {
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
        glfwSetKeyCallback(window_, keyCallback);

        // Save initial windowed geometry for later restore
        glfwGetWindowPos(window_, &windowPosX_, &windowPosY_);
        glfwGetWindowSize(window_, &windowedWidth_, &windowedHeight_);

        Core::Logger::log(Core::LogLevel::INFO, "Window created");
    }

    WindowManager::~WindowManager() noexcept
    {
        if (window_)
        {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }
    }

    bool WindowManager::shouldClose() const noexcept
    {
        return glfwWindowShouldClose(window_) == GLFW_TRUE;
    }

    void WindowManager::pollEvents() noexcept
    {
        std::copy(currKeys_.begin(), currKeys_.end(), prevKeys_.begin());
        std::copy(currMouse_.begin(), currMouse_.end(), prevMouse_.begin());

        glfwPollEvents();

        // --- keyboard ---
        // skip non-valide key codes < GLFW_KEY_SPACE
        for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_LAST; ++k)
        {
            currKeys_[k] = (glfwGetKey(window_, k) == GLFW_PRESS) ? 1u : 0u;
        }

        // --- mouse ---
        for (int b = 0; b < 8; ++b)
        {
            currMouse_[b] = (glfwGetMouseButton(window_, b) == GLFW_PRESS) ? 1u : 0u;
        }

        // --- mouse delta ---
        double x, y;
        glfwGetCursorPos(window_, &x, &y);

        if (firstMouse_)
        {
            lastX_ = x;
            lastY_ = y;
            dx_ = 0.f;
            dy_ = 0.f;
            firstMouse_ = false;
        }
        else
        {
            dx_ = static_cast<float>(x - lastX_);
            dy_ = static_cast<float>(y - lastY_);
            lastX_ = x;
            lastY_ = y;
        }

        // update width_/height_, for renderer always know actual sizes
        int fbw, fbh;
        glfwGetFramebufferSize(window_, &fbw, &fbh);
        width_ = fbw;
        height_ = fbh;
    }

    bool WindowManager::isKeyDown(int key) const noexcept { return currKeys_[key] != 0; }
    bool WindowManager::wasKeyPressed(int key) const noexcept { return currKeys_[key] && !prevKeys_[key]; }
    glm::vec2 WindowManager::mouseDelta() const noexcept { return {dx_, dy_}; }
    bool WindowManager::isMouseDown(int b) const noexcept { return currMouse_[b] != 0; }

    void WindowManager::captureMouse(bool enabled) noexcept
    {
        glfwSetInputMode(window_, GLFW_CURSOR, enabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        firstMouse_ = true; // for no jumping dx/dy when on
    }

    std::vector<const char *> WindowManager::getRequiredExtensions() const
    {
        uint32_t count = 0;
        const char **exts = glfwGetRequiredInstanceExtensions(&count);
        if (!exts || count == 0)
        {
            // GLFW can return nullptr on platforms without Vulkan surface support yet
            Core::Logger::log(Core::LogLevel::WARNING, "GLFW returned no required Vulkan extensions.");
            return {};
        }
        return std::vector<const char *>{exts, exts + count};
    }

    void WindowManager::toggleFullscreen()
    {
        fullscreen_ = !fullscreen_;

        if (fullscreen_)
        {
            // Save current windowed position & size
            glfwGetWindowPos(window_, &windowPosX_, &windowPosY_);
            glfwGetWindowSize(window_, &windowedWidth_, &windowedHeight_);

            // Prefer the monitor the window is currently on
            GLFWmonitor *monitor = glfwGetWindowMonitor(window_);
            if (!monitor)
            {
                // If not already fullscreen, pick primary or nearest
                monitor = glfwGetPrimaryMonitor();
            }

            const GLFWvidmode *mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(window_,
                                 monitor,
                                 0, 0,
                                 mode->width, mode->height,
                                 mode->refreshRate);
        }
        else
        {
            // Restore windowed geometry
            glfwSetWindowMonitor(window_,
                                 nullptr,
                                 windowPosX_, windowPosY_,
                                 windowedWidth_, windowedHeight_,
                                 0);
        }
    }

    void WindowManager::setTitle(const std::string &title) noexcept
    {
        glfwSetWindowTitle(window_, title.c_str());
    }

    void WindowManager::framebufferResizeCallback(GLFWwindow *wnd, int w, int h) noexcept
    {
        auto *wm = reinterpret_cast<WindowManager *>(glfwGetWindowUserPointer(wnd));
        if (!wm)
            return;

        wm->width_ = w;
        wm->height_ = h;

        if (wm->onFramebufferResize)
        {
            wm->onFramebufferResize(w, h);
        }
    }

    void WindowManager::keyCallback(GLFWwindow *wnd, int key, int /*scancode*/, int action, int mods) noexcept
    {
        if (action == GLFW_PRESS && key == GLFW_KEY_ENTER && (mods & GLFW_MOD_ALT))
        {
            auto *wm = reinterpret_cast<WindowManager *>(glfwGetWindowUserPointer(wnd));
            if (wm)
                wm->toggleFullscreen();
        }
    }

} // namespace Platform
