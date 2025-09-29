#pragma once

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include <functional>

namespace Core
{
    class WindowManager
    {
    public:
        WindowManager(uint32_t width, uint32_t height, const char *title);
        ~WindowManager();

        // Non-copyable
        WindowManager(const WindowManager &) = delete;
        WindowManager &operator=(const WindowManager &) = delete;

        GLFWwindow *getWindow() const { return window; }
        int getWidth() const { return width; }
        int getHeight() const { return height; }

        // Callback that user can set to handle framebuffer resize
        std::function<void(int, int)> onFramebufferResize;

        bool shouldClose() const;
        void pollEvents() const;

        // Get extensions required by GLFW for Vulkan instance
        std::vector<const char *> getRequiredExtensions() const;

        // Toggle fullscreen on/off
        void toggleFullscreen();

        bool isFullscreen() const { return fullscreen; }

    private:
        GLFWwindow *window{nullptr};
        int width{0}, height{0};
        bool fullscreen{false};

        // To restore windowed mode after fullscreen
        int windowPosX{0}, windowPosY{0};
        int windowedWidth{0}, windowedHeight{0};

        // GLFW callbacks
        static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
        static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
    };
} // namespace Core
