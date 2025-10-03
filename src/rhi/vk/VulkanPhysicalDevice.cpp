#include "rhi/vk/VulkanPhysicalDevice.h"
#include "rhi/vk/Surface.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

#include <vector>
#include <cstring>
#include <algorithm>
#include <limits>

namespace Vk
{

    using Core::LogLevel;

#ifndef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
#define VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME "VK_KHR_portability_enumeration"
#endif

    VulkanPhysicalDevice::VulkanPhysicalDevice(VkInstance inInstance, const Surface &surface)
        : instance(inInstance)
    {
        pickPhysicalDevice(surface);
    }

    void VulkanPhysicalDevice::pickPhysicalDevice(const Surface &surface)
    {
        uint32_t deviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
        if (deviceCount == 0)
        {
            Core::Logger::log(LogLevel::ERROR, "No Vulkan-compatible GPUs found!");
            throw std::runtime_error("No Vulkan-compatible GPUs found!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

        // Score all devices, pick the best valid one
        int bestScore = std::numeric_limits<int>::min();
        VkPhysicalDevice best = VK_NULL_HANDLE;
        QueueFamilyIndices bestIndices{};

        for (auto dev : devices)
        {
            if (!isDeviceSuitable(dev, surface))
                continue;

            int score = deviceScore(dev);
            if (score > bestScore)
            {
                bestScore = score;
                best = dev;
                bestIndices = queueFamilies; // set by isDeviceSuitable/findQueueFamilies
            }
        }

        if (best == VK_NULL_HANDLE)
        {
            Core::Logger::log(LogLevel::ERROR, "Failed to find a suitable GPU!");
            throw std::runtime_error("Failed to find a suitable GPU!");
        }

        physicalDevice = best;
        queueFamilies = bestIndices;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        Core::Logger::log(LogLevel::INFO,
                          "Selected GPU: " + std::string(props.deviceName) +
                              " (" + deviceTypeToString(props.deviceType) + ")");
    }

    bool VulkanPhysicalDevice::isDeviceSuitable(VkPhysicalDevice device, const Surface &surface)
    {
        // 1) queues
        auto indices = findQueueFamilies(device, surface);
        if (!indices.isComplete())
            return false;

        // 2) device extensions (swapchain)
        if (!checkDeviceExtensions(device))
            return false;

        // 3) surface formats/present modes exist
        if (!checkSurfaceSupport(device, surface))
            return false;

        // cache for getters / later use
        this->queueFamilies = indices;
        return true;
    }

    QueueFamilyIndices VulkanPhysicalDevice::findQueueFamilies(VkPhysicalDevice device, const Surface &surface)
    {
        QueueFamilyIndices indices;

        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
        std::vector<VkQueueFamilyProperties> props(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, props.data());

        for (uint32_t i = 0; i < count; ++i)
        {
            const auto &q = props[i];

            if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                indices.graphicsFamily = i;

            VkBool32 presentSupport = VK_FALSE;
            // ⚠️ Surface API expects VkSurfaceKHR; your Surface exposes getSurface()
            VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface.get(), &presentSupport));
            if (presentSupport)
                indices.presentFamily = i;

            if (indices.isComplete())
                break;
        }

        return indices;
    }

    bool VulkanPhysicalDevice::checkDeviceExtensions(VkPhysicalDevice device) const
    {
        uint32_t extCount = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr));
        std::vector<VkExtensionProperties> exts(extCount);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, exts.data()));

        auto has = [&](const char *name)
        {
            for (const auto &e : exts)
                if (std::strcmp(e.extensionName, name) == 0)
                    return true;
            return false;
        };

        // must-have
        if (!has(VK_KHR_SWAPCHAIN_EXTENSION_NAME))
            return false;

        // portability subset is optional (MoltenVK). If present — fine; если нет — тоже ок.
        // if (has(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) { ... }

        return true;
    }

    bool VulkanPhysicalDevice::checkSurfaceSupport(VkPhysicalDevice device, const Surface &surface) const
    {
        // At least one format and one present mode must be available for the chosen surface.
        uint32_t formatCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface.get(), &formatCount, nullptr));
        uint32_t presentModeCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface.get(), &presentModeCount, nullptr));

        return (formatCount > 0) && (presentModeCount > 0);
    }

    int VulkanPhysicalDevice::deviceScore(VkPhysicalDevice device) const
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(device, &props);

        int score = 0;
        // Prefer discrete > integrated > others
        switch (props.deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 1000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 100;
            break;
        default:
            break;
        }
        // Slight bias: larger max 2D image size often correlates with stronger GPU
        score += static_cast<int>(props.limits.maxImageDimension2D / 1024);

        return score;
    }

    std::string VulkanPhysicalDevice::deviceTypeToString(VkPhysicalDeviceType type) const
    {
        switch (type)
        {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "Integrated GPU";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "Discrete GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "Virtual GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "CPU (software)";
        default:
            return "Other";
        }
    }

} // namespace Vk
