#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Core
{
    class VulkanPhysicalDevice;

    class VulkanLogicalDevice
    {
    public:
        VulkanLogicalDevice(const VulkanPhysicalDevice &physicalDevice);
        ~VulkanLogicalDevice();

        VulkanLogicalDevice(const VulkanLogicalDevice &) = delete;
        VulkanLogicalDevice &operator=(const VulkanLogicalDevice &) = delete;

        VkDevice getDevice() const { return device; }
        VkQueue getGraphicsQueue() const { return graphicsQueue; }
        VkQueue getPresentQueue() const { return presentQueue; }

    private:
        VkDevice device{VK_NULL_HANDLE};
        VkQueue graphicsQueue{VK_NULL_HANDLE};
        VkQueue presentQueue{VK_NULL_HANDLE};
    };
}
