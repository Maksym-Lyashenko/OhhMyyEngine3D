#pragma once

#include <vulkan/vulkan.h>
#include "platform/WindowManager.h"

namespace Vk
{

    /**
     * @brief RAII wrapper for VkSurfaceKHR created from a GLFW window.
     *
     * Notes:
     *  - VkInstance and WindowManager must outlive this object.
     *  - Resize of the window does NOT require recreating the surface;
     *    recreate only if the underlying OS window is recreated.
     */
    class Surface final
    {
    public:
        Surface(VkInstance instance, const Platform::WindowManager &window);
        ~Surface() noexcept;

        Surface(const Surface &) = delete;
        Surface &operator=(const Surface &) = delete;

        Surface(Surface &&other) noexcept;
        Surface &operator=(Surface &&other) noexcept;

        /// Create the surface if not created yet (idempotent).
        void create();

        /// Destroy the surface if created (safe to call multiple times).
        void cleanup() noexcept;

        /// Recreate for a (potentially) new window handle.
        void recreate(const Platform::WindowManager &newWindow)
        {
            window_ = &newWindow;
            cleanup();
            create();
        }

        VkSurfaceKHR get() const noexcept { return surface_; }

    private:
        VkInstance instance_{VK_NULL_HANDLE};            // not owned
        const Platform::WindowManager *window_{nullptr}; // not owned
        VkSurfaceKHR surface_{VK_NULL_HANDLE};           // owned
    };

} // namespace Vk
