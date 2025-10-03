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

    /**
     * @brief Picks a suitable VkPhysicalDevice for rendering to the given Surface.
     *
     * Guarantees:
     *  - graphics + present queue families are available;
     *  - VK_KHR_swapchain is supported by the device;
     *  - the surface has at least one format and one present mode.
     *
     * Heuristic:
     *  - prefer DISCRETE_GPU over others; ties broken by maxImageDimension2D.
     */
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
        QueueFamilyIndices queueFamilies{};

        // Selection pipeline
        void pickPhysicalDevice(const Surface &surface);
        bool isDeviceSuitable(VkPhysicalDevice device, const Surface &surface);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, const Surface &surface);

        // Swapchain support checks
        bool checkDeviceExtensions(VkPhysicalDevice device) const;
        bool checkSurfaceSupport(VkPhysicalDevice device, const Surface &surface) const;

        // Scoring heuristic
        int deviceScore(VkPhysicalDevice device) const;

        // Pretty-print
        std::string deviceTypeToString(VkPhysicalDeviceType type) const;
    };

} // namespace Vk
