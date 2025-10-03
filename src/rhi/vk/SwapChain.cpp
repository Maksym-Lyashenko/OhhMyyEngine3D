#include "rhi/vk/SwapChain.h"
#include "rhi/vk/VulkanPhysicalDevice.h"
#include "rhi/vk/VulkanLogicalDevice.h"
#include "rhi/vk/Surface.h"
#include "platform/WindowManager.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

#include <algorithm>
#include <stdexcept>

namespace Vk
{

    SwapChain::SwapChain(const VulkanPhysicalDevice &physicalDevice_,
                         const VulkanLogicalDevice &logicalDevice_,
                         const Surface &surface_,
                         const Platform::WindowManager &window_)
        : physicalDevice(physicalDevice_), logicalDevice(logicalDevice_), surface(surface_), window(window_)
    {
    }

    SwapChain::~SwapChain() noexcept
    {
        cleanup();
    }

    void SwapChain::create()
    {
        // --- 1) Query surface capabilities / formats / present modes ---
        VkSurfaceCapabilitiesKHR capabilities{};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice.getDevice(),
                                                           surface.get(), &capabilities));

        uint32_t formatCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice.getDevice(),
                                                      surface.get(), &formatCount, nullptr));
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice.getDevice(),
                                                      surface.get(), &formatCount, formats.data()));

        uint32_t presentModeCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice.getDevice(),
                                                           surface.get(), &presentModeCount, nullptr));
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice.getDevice(),
                                                           surface.get(), &presentModeCount, presentModes.data()));

        if (formats.empty() || presentModes.empty())
        {
            throw std::runtime_error("SwapChain::create: empty surface formats or present modes");
        }

        // --- 2) Choose config ---
        const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(formats);
        const VkPresentModeKHR presentMode = choosePresentMode(presentModes);
        extent = chooseExtent(capabilities);

        // --- 3) Decide image count ---
        uint32_t desiredImageCount = capabilities.minImageCount + 1; // triple-buffer if possible
        if (capabilities.maxImageCount > 0 && desiredImageCount > capabilities.maxImageCount)
        {
            desiredImageCount = capabilities.maxImageCount;
        }

        // --- 4) Fill create info ---
        VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        info.surface = surface.get();
        info.minImageCount = desiredImageCount;
        info.imageFormat = surfaceFormat.format;
        info.imageColorSpace = surfaceFormat.colorSpace;
        info.imageExtent = extent;
        info.imageArrayLayers = 1;
        // Allow color attachment, plus transfer src (useful for screenshots/blit; drop if not needed)
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        const auto indices = physicalDevice.getQueueFamilies();
        const uint32_t qIndices[2] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily)
        {
            info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            info.queueFamilyIndexCount = 2;
            info.pQueueFamilyIndices = qIndices;
        }
        else
        {
            info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        info.preTransform = capabilities.currentTransform;
        info.compositeAlpha = chooseCompositeAlpha(capabilities.supportedCompositeAlpha);
        info.presentMode = presentMode;
        info.clipped = VK_TRUE;
        info.oldSwapchain = swapChain; // pass old chain if recreating (may be VK_NULL_HANDLE)

        // --- 5) Create swapchain (cleanly replace old if present) ---
        VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSwapchainKHR(logicalDevice.getDevice(), &info, nullptr, &newSwapchain));

        // Destroy the old swapchain AFTER creating the new one to avoid flicker on some drivers
        if (swapChain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(logicalDevice.getDevice(), swapChain, nullptr);
        }
        swapChain = newSwapchain;

        // --- 6) Get images ---
        uint32_t imageCount = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(logicalDevice.getDevice(), swapChain, &imageCount, nullptr));
        images.resize(imageCount);
        VK_CHECK(vkGetSwapchainImagesKHR(logicalDevice.getDevice(), swapChain, &imageCount, images.data()));

        swapChainImageFormat = info.imageFormat;

        Core::Logger::log(Core::LogLevel::INFO,
                          "SwapChain created: " + std::to_string(imageCount) + " images, " + std::to_string(extent.width) + "x" + std::to_string(extent.height));
    }

    void SwapChain::cleanup() noexcept
    {
        if (swapChain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(logicalDevice.getDevice(), swapChain, nullptr);
            swapChain = VK_NULL_HANDLE;
            images.clear();
            Core::Logger::log(Core::LogLevel::INFO, "SwapChain destroyed");
        }
    }

    VkSurfaceFormatKHR SwapChain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats) const
    {
        // Prefer sRGB if available
        for (const auto &f : formats)
        {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return f;
            }
        }
        // Otherwise, take the first offered by the platform
        return formats.front();
    }

    VkPresentModeKHR SwapChain::choosePresentMode(const std::vector<VkPresentModeKHR> &modes) const
    {
        // Prefer mailbox (low-latency, no tearing) if available
        for (const auto &m : modes)
        {
            if (m == VK_PRESENT_MODE_MAILBOX_KHR)
                return m;
        }
        // FIFO is always present (VSync)
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D SwapChain::chooseExtent(const VkSurfaceCapabilitiesKHR &caps) const
    {
        if (caps.currentExtent.width != UINT32_MAX)
        {
            // The surface size is defined by the windowing system (e.g., on mobile)
            return caps.currentExtent;
        }

        VkExtent2D actual{
            static_cast<uint32_t>(window.width()),
            static_cast<uint32_t>(window.height())};

        actual.width = std::max(caps.minImageExtent.width,
                                std::min(caps.maxImageExtent.width, actual.width));
        actual.height = std::max(caps.minImageExtent.height,
                                 std::min(caps.maxImageExtent.height, actual.height));
        return actual;
    }

    VkCompositeAlphaFlagBitsKHR SwapChain::chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported) const
    {
        // Prefer OPAQUE, else pick the first supported flag (common fallback — PRE_MULTIPLIED)
        if (supported & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
            return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        if (supported & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
            return VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
        if (supported & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
            return VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
        if (supported & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
            return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        // Spec guarantees at least one bit; fallback to OPAQUE to be safe
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }

} // namespace Vk
