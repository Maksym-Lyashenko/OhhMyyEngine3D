#include "rhi/vk/VulkanLogicalDevice.h"
#include "rhi/vk/VulkanPhysicalDevice.h"

#include "core/Logger.h"
#include <rhi/vk/Common.h>

#include <set>

using namespace Core;

namespace Vk
{
    VulkanLogicalDevice::VulkanLogicalDevice(const VulkanPhysicalDevice &physicalDevice)
    {
        // 1. Get queue family indices from physical device
        auto indices = physicalDevice.getQueueFamilies();

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // 2. Device features
        VkPhysicalDeviceFeatures deviceFeatures{};

        // 3. Device extensions
        constexpr const char *deviceExtensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkPhysicalDeviceVulkan13Features f13{};
        f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        f13.synchronization2 = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pNext = &f13;
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(std::size(deviceExtensions));
        createInfo.ppEnabledExtensionNames = deviceExtensions;
        createInfo.enabledLayerCount = 0; // layers deprecated for device-level

        // 4. Create logical device
        VK_CHECK(vkCreateDevice(physicalDevice.getDevice(), &createInfo, nullptr, &device));

        // 5. Retrieve queues
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

        Logger::log(LogLevel::INFO, "Logical device created successfully with VK_KHR_swapchain extension");
        Logger::log(LogLevel::INFO, "Graphics and present queues obtained");
    }

    VulkanLogicalDevice::~VulkanLogicalDevice()
    {
        if (device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(device);
            vkDestroyDevice(device, nullptr);
            Logger::log(LogLevel::INFO, "Logical device destroyed");
        }
    }
} // namespace Vk
