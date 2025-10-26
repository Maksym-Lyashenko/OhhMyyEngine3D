#pragma once

#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL

#include <cstdint>
#include <functional>
#include <vector>
#include <string>
#include <array>
#include <glm/vec2.hpp>
#include <glm/ext/vector_float2.hpp>

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

        bool isKeyDown(int key) const noexcept;      // GLFW_KEY_*
        bool wasKeyPressed(int key) const noexcept;  // edge (this frame)
        glm::vec2 mouseDelta() const noexcept;       // dx,dy за кадр
        bool isMouseDown(int button) const noexcept; // GLFW_MOUSE_BUTTON_*
        void captureMouse(bool enabled) noexcept;

        [[nodiscard]] GLFWwindow *getWindow() const noexcept { return window_; }
        [[nodiscard]] int width() const noexcept { return width_; }
        [[nodiscard]] int height() const noexcept { return height_; }
        [[nodiscard]] float aspect() const noexcept { return height_ > 0 ? float(width_) / float(height_) : 0.0f; }

        // User callback for framebuffer resize (width, height)
        std::function<void(int, int)> onFramebufferResize;

        [[nodiscard]] bool shouldClose() const noexcept;
        void pollEvents() noexcept;

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

        std::array<unsigned char, GLFW_KEY_LAST + 1> currKeys_{};
        std::array<unsigned char, GLFW_KEY_LAST + 1> prevKeys_{};

        // Мышь
        std::array<unsigned char, 8> currMouse_{};
        std::array<unsigned char, 8> prevMouse_{};

        double lastX_{0.0}, lastY_{0.0};
        float dx_{0.0f}, dy_{0.0f};
        bool firstMouse_{true};
    };

} // namespace Platform
