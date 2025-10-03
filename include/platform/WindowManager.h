#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <string>

struct GLFWwindow; // forward-declare to avoid pulling GLFW in the header

namespace Platform
{

    class WindowManager
    {
    public:
        explicit WindowManager(std::uint32_t width, std::uint32_t height, const char *title);
        ~WindowManager() noexcept;

        WindowManager(const WindowManager &) = delete;
        WindowManager &operator=(const WindowManager &) = delete;
        WindowManager(WindowManager &&) = delete;
        WindowManager &operator=(WindowManager &&) = delete;

        [[nodiscard]] GLFWwindow *getWindow() const noexcept { return window_; }
        [[nodiscard]] int width() const noexcept { return width_; }
        [[nodiscard]] int height() const noexcept { return height_; }
        [[nodiscard]] float aspect() const noexcept { return height_ > 0 ? float(width_) / float(height_) : 0.0f; }

        // User callback for framebuffer resize (width, height)
        std::function<void(int, int)> onFramebufferResize;

        [[nodiscard]] bool shouldClose() const noexcept;
        void pollEvents() const noexcept;

        // Vulkan instance extensions required by GLFW
        [[nodiscard]] std::vector<const char *> getRequiredExtensions() const;

        // Toggle fullscreen on/off (Alt+Enter by default)
        void toggleFullscreen();

        [[nodiscard]] bool isFullscreen() const noexcept { return fullscreen_; }

        void setTitle(const std::string &title) noexcept;

    private:
        GLFWwindow *window_{nullptr};
        int width_{0}, height_{0};
        bool fullscreen_{false};

        // Saved windowed geometry to restore after fullscreen
        int windowPosX_{0}, windowPosY_{0};
        int windowedWidth_{0}, windowedHeight_{0};

        // Callbacks
        static void framebufferResizeCallback(GLFWwindow *window, int width, int height) noexcept;
        static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) noexcept;

        // Helper to pick monitor for fullscreen
        static GLFWwindow *getWindowHandle(GLFWwindow *w) noexcept { return w; }
    };

} // namespace Platform
