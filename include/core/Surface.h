#pragma once

#include <vulkan/vulkan.h>
#include "core/WindowManager.h"

namespace Core
{
    class Surface
    {
    public:
        // Construct with VkInstance and a WindowManager reference
        Surface(VkInstance instance, const WindowManager &window);
        ~Surface();

        Surface(const Surface &) = delete;
        Surface &operator=(const Surface &) = delete;

        void create();
        VkSurfaceKHR getSurface() const { return surface_; }

    private:
        VkInstance instance_{VK_NULL_HANDLE};  // not owned
        const WindowManager &window_;          // not owned
        VkSurfaceKHR surface_{VK_NULL_HANDLE}; // owned
        bool created_ = false;
    };
}
