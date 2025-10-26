#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <string>

namespace Platform
{
    class WindowManager;
}

namespace Vk
{

    class VulkanLogicalDevice;
    class VulkanPhysicalDevice;
    class Surface;

    /**
     * @brief RAII wrapper over VkSwapchainKHR and its images.
     *
     * Requirements:
     *  - Physical/Logical device, Surface, and WindowManager must outlive this object.
     *  - Call recreate() when surface capabilities change (resize, etc.).
     */
    class SwapChain
    {
    public:
        SwapChain(const VulkanPhysicalDevice &physicalDevice,
                  const VulkanLogicalDevice &logicalDevice,
                  const Surface &surface,
                  const Platform::WindowManager &window);
        ~SwapChain() noexcept;

        SwapChain(const SwapChain &) = delete;
        SwapChain &operator=(const SwapChain &) = delete;

        /// Create the swapchain (idempotent; will destroy old chain if any).
        void create();
        /// Destroy the swapchain (safe to call multiple times).
        void cleanup() noexcept;

        /// Destroy + create again.
        void recreate()
        {
            cleanup();
            create();
        }

        VkSwapchainKHR getSwapChain() const noexcept { return swapChain; }
        const std::vector<VkImage> &getImages() const noexcept { return images; }
        uint32_t imageCount() const noexcept { return static_cast<uint32_t>(images.size()); }
        VkFormat getImageFormat() const noexcept { return swapChainImageFormat; }
        VkExtent2D getExtent() const noexcept { return extent; }

        VkPresentModeKHR presentMode() const noexcept { return presentMode_; }
        const std::string &presentModeName() const noexcept { return presentModeName_; }

    private:
        const VulkanPhysicalDevice &physicalDevice;
        const VulkanLogicalDevice &logicalDevice;
        const Surface &surface;
        const Platform::WindowManager &window;

        VkSwapchainKHR swapChain{VK_NULL_HANDLE};
        std::vector<VkImage> images;
        VkFormat swapChainImageFormat{};
        VkExtent2D extent{};

        VkPresentModeKHR presentMode_{VK_PRESENT_MODE_FIFO_KHR};
        std::string presentModeName_ = "FIFO";

        // Helpers
        VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) const;
        VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) const;
        VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR &capabilities) const;
        VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported) const;
    };

} // namespace Vk
