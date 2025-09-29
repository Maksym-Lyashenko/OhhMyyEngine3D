#include "core/SwapChain.h"
#include "core/VulkanPhysicalDevice.h"
#include "core/VulkanLogicalDevice.h"
#include "core/Surface.h"
#include "core/WindowManager.h"

#include "core/Logger.h"
#include <core/VulkanUtils.h>

namespace Core
{
    SwapChain::SwapChain(const VulkanPhysicalDevice &physicalDevice,
                         const VulkanLogicalDevice &logicalDevice,
                         const Surface &surface,
                         const WindowManager &window)
        : physicalDevice(physicalDevice),
          logicalDevice(logicalDevice),
          surface(surface),
          window(window)
    {
    }

    SwapChain::~SwapChain()
    {
        cleanup();
    }

    void SwapChain::create()
    {
        // 1. Query surface capabilities/formats/present modes
        VkSurfaceCapabilitiesKHR capabilities{};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice.getDevice(),
                                                           surface.getSurface(), &capabilities));

        uint32_t formatCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice.getDevice(),
                                                      surface.getSurface(), &formatCount, nullptr));
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice.getDevice(),
                                                      surface.getSurface(), &formatCount, formats.data()));

        uint32_t presentModeCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice.getDevice(),
                                                           surface.getSurface(), &presentModeCount, nullptr));
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice.getDevice(),
                                                           surface.getSurface(), &presentModeCount, presentModes.data()));

        if (formats.empty() || presentModes.empty())
        {
            throw std::runtime_error("Surface formats or present modes are empty!");
        }

        // 2. Choose best settings
        VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(formats);
        VkPresentModeKHR presentMode = choosePresentMode(presentModes);
        extent = chooseExtent(capabilities);

        // 3. Decide image count
        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
            imageCount = capabilities.maxImageCount;

        // 4. Fill create info
        VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        createInfo.surface = surface.getSurface();
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        auto indices = physicalDevice.getQueueFamilies();
        uint32_t queueFamilyIndices[] = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        // 5. Create swapchain
        VK_CHECK(vkCreateSwapchainKHR(logicalDevice.getDevice(), &createInfo, nullptr, &swapChain));

        // 6. Get images
        VK_CHECK(vkGetSwapchainImagesKHR(logicalDevice.getDevice(), swapChain, &imageCount, nullptr));
        images.resize(imageCount);
        VK_CHECK(vkGetSwapchainImagesKHR(logicalDevice.getDevice(), swapChain, &imageCount, images.data()));

        swapChainImageFormat = createInfo.imageFormat;

        Logger::log(LogLevel::INFO, "SwapChain created successfully (" + std::to_string(imageCount) + " images)");
    }

    void SwapChain::cleanup()
    {
        if (swapChain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(logicalDevice.getDevice(), swapChain, nullptr);
            swapChain = VK_NULL_HANDLE;
            Logger::log(LogLevel::INFO, "SwapChain destroyed");
        }
    }

    VkSurfaceFormatKHR SwapChain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
    {
        for (const auto &availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    VkPresentModeKHR SwapChain::choosePresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes)
    {
        for (const auto &availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR; // Always available
    }

    VkExtent2D SwapChain::chooseExtent(const VkSurfaceCapabilitiesKHR &capabilities)
    {
        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            return capabilities.currentExtent;
        }
        VkExtent2D actualExtent{
            static_cast<uint32_t>(window.getWidth()),
            static_cast<uint32_t>(window.getHeight())};

        actualExtent.width = std::max(capabilities.minImageExtent.width,
                                      std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height,
                                       std::min(capabilities.maxImageExtent.height, actualExtent.height));
        return actualExtent;
    }
}
