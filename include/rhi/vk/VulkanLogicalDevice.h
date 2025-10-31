#pragma once

#include <vulkan/vulkan.h>

namespace Vk
{

    class VulkanPhysicalDevice;

    /**
     * @brief RAII wrapper over VkDevice + retrieval of graphics/present queues.
     *
     * - Enables VK_KHR_swapchain (required for presenting).
     * - Enables VK_KHR_portability_subset if the physical device advertises it (MoltenVK).
     * - Requests Vulkan 1.3 feature: synchronization2 (already used by your code).
     */
    class VulkanLogicalDevice
    {
    public:
        explicit VulkanLogicalDevice(const VulkanPhysicalDevice &physicalDevice);
        ~VulkanLogicalDevice() noexcept;

        VulkanLogicalDevice(const VulkanLogicalDevice &) = delete;
        VulkanLogicalDevice &operator=(const VulkanLogicalDevice &) = delete;

        VkDevice getDevice() const noexcept { return device; }
        VkQueue getGraphicsQueue() const noexcept { return graphicsQueue; }
        VkQueue getPresentQueue() const noexcept { return presentQueue; }

        [[nodiscard]] uint32_t getGraphicsQueueFamilyIndex() const noexcept { return graphicsQueueFamilyIndex_; }
        [[nodiscard]] uint32_t getPresentQueueFamilyIndex() const noexcept { return presentQueueFamilyIndex_; }

    private:
        VkDevice device{VK_NULL_HANDLE};
        VkQueue graphicsQueue{VK_NULL_HANDLE};
        VkQueue presentQueue{VK_NULL_HANDLE};

        uint32_t graphicsQueueFamilyIndex_ = 0;
        uint32_t presentQueueFamilyIndex_ = 0;
    };

} // namespace Vk
