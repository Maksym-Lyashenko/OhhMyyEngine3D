#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace Platform
{
    class WindowManager;
}

namespace Vk
{
    class VulkanLogicalDevice;
    class VulkanPhysicalDevice;
    class Surface;

    class SwapChain
    {
    public:
        SwapChain(const VulkanPhysicalDevice &physicalDevice,
                  const VulkanLogicalDevice &logicalDevice,
                  const Surface &surface,
                  const Platform::WindowManager &window);
        ~SwapChain();

        SwapChain(const SwapChain &) = delete;
        SwapChain &operator=(const SwapChain &) = delete;

        void create();
        void cleanup();

        VkSwapchainKHR getSwapChain() const { return swapChain; }
        const std::vector<VkImage> &getImages() const { return images; }
        VkFormat getImageFormat() const { return swapChainImageFormat; }
        VkExtent2D getExtent() const { return extent; }

    private:
        const VulkanPhysicalDevice &physicalDevice;
        const VulkanLogicalDevice &logicalDevice;
        const Surface &surface;
        const Platform::WindowManager &window;

        VkSwapchainKHR swapChain{VK_NULL_HANDLE};
        std::vector<VkImage> images;
        VkFormat swapChainImageFormat{};
        VkExtent2D extent{};

        // Helper functions
        VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
        VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
        VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR &capabilities);
    };
} // namespace Vk
