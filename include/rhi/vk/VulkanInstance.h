#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Platform
{
    class WindowManager;
}

namespace Vk
{

    /**
     * @brief RAII wrapper for VkInstance + (optional) debug messenger.
     *
     * Features:
     *  - Picks the highest supported Vulkan API (capped to 1.3 by default).
     *  - Adds VK_EXT_debug_utils & sets up Debug Utils Messenger when validation is enabled.
     *  - Adds portability extensions/flags on platforms that require them (MoltenVK).
     */
    class VulkanInstance
    {
    public:
        VulkanInstance(const Platform::WindowManager &window,
                       bool enableValidationLayers = true);
        ~VulkanInstance() noexcept;

        VulkanInstance(const VulkanInstance &) = delete;
        VulkanInstance &operator=(const VulkanInstance &) = delete;

        VkInstance getInstance() const noexcept { return instance; }

    private:
        VkInstance instance{VK_NULL_HANDLE};
        VkDebugUtilsMessengerEXT debugMessenger{VK_NULL_HANDLE};
        bool validationEnabled{true};
        bool portabilityEnabled{false}; // set if portability extension is present

        // Helpers
        std::vector<const char *> getRequiredExtensions(const Platform::WindowManager &window) const;
        bool checkValidationLayerSupport() const;

        // Debug utils
        void setupDebugMessenger() noexcept;
        static VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
            void *pUserData) noexcept;

        // Static list of validation layers
        inline static const std::vector<const char *> validationLayers{
            "VK_LAYER_KHRONOS_validation"};
    };

} // namespace Vk
