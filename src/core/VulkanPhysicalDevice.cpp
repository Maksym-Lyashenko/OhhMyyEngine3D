#include "core/VulkanPhysicalDevice.h"
#include "core/Surface.h"

#include "core/Logger.h"
#include <core/VulkanUtils.h>

namespace Core
{
    VulkanPhysicalDevice::VulkanPhysicalDevice(VkInstance instance, const Surface &surface)
        : instance(instance)
    {
        pickPhysicalDevice(surface);
    }

    void VulkanPhysicalDevice::pickPhysicalDevice(const Surface &surface)
    {
        uint32_t deviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));

        if (deviceCount == 0)
        {
            Logger::log(LogLevel::ERROR, "No Vulkan-compatible GPUs found!");
            throw std::runtime_error("No Vulkan-compatible GPUs found!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

        for (const auto &device : devices)
        {
            if (isDeviceSuitable(device, surface))
            {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE)
        {
            Logger::log(LogLevel::ERROR, "Failed to find a suitable GPU!");
            throw std::runtime_error("Failed to find a suitable GPU!");
        }

        // Log chosen device
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physicalDevice, &props);

        Logger::log(LogLevel::INFO, "Selected GPU: " + std::string(props.deviceName) +
                                        " (" + deviceTypeToString(props.deviceType) + ")");
    }

    bool VulkanPhysicalDevice::isDeviceSuitable(VkPhysicalDevice device, const Surface &surface)
    {
        auto indices = findQueueFamilies(device, surface);
        return indices.isComplete();
    }

    QueueFamilyIndices VulkanPhysicalDevice::findQueueFamilies(VkPhysicalDevice device, const Surface &surface)
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueProps(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueProps.data());

        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            const auto &queueFamily = queueProps[i];

            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                indices.graphicsFamily = i;

            VkBool32 presentSupport = VK_FALSE;
            VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface.getSurface(), &presentSupport));
            if (presentSupport)
                indices.presentFamily = i;

            if (indices.isComplete())
                break;
        }

        this->queueFamilies = indices;
        return indices;
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
}
