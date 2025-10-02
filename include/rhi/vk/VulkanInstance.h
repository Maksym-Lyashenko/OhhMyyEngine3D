#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Platform
{
    class WindowManager;
}

namespace Vk
{

    class VulkanInstance
    {
    public:
        VulkanInstance(const Platform::WindowManager &window, bool enableValidationLayers = true);
        ~VulkanInstance();

        VulkanInstance(const VulkanInstance &) = delete;
        VulkanInstance &operator=(const VulkanInstance &) = delete;

        VkInstance getInstance() const noexcept { return instance; }

    private:
        VkInstance instance{VK_NULL_HANDLE};
        bool validationEnabled{true};

        // Helpers
        std::vector<const char *> getRequiredExtensions(const Platform::WindowManager &window) const;
        bool checkValidationLayerSupport() const;

        // Static list of validation layers
        inline static const std::vector<const char *> validationLayers{
            "VK_LAYER_KHRONOS_validation"};
    };
} // namespace Vk
