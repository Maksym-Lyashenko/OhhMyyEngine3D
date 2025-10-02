#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <vector>
#include <string>

namespace Vk
{
    class Surface;

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    class VulkanPhysicalDevice
    {
    public:
        VulkanPhysicalDevice(VkInstance instance, const Surface &surface);
        ~VulkanPhysicalDevice() = default;

        VulkanPhysicalDevice(const VulkanPhysicalDevice &) = delete;
        VulkanPhysicalDevice &operator=(const VulkanPhysicalDevice &) = delete;

        VkPhysicalDevice getDevice() const { return physicalDevice; }
        QueueFamilyIndices getQueueFamilies() const { return queueFamilies; }

    private:
        VkInstance instance{VK_NULL_HANDLE};
        VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
        QueueFamilyIndices queueFamilies;

        void pickPhysicalDevice(const Surface &surface);
        bool isDeviceSuitable(VkPhysicalDevice device, const Surface &surface);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, const Surface &surface);

        std::string deviceTypeToString(VkPhysicalDeviceType type) const;
    };
} // namespace Vk
