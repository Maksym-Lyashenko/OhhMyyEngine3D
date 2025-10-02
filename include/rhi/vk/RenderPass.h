#pragma once

#include <vulkan/vulkan.h>

namespace Vk
{
    class VulkanLogicalDevice;
    class SwapChain;

    class RenderPass
    {
    public:
        RenderPass(const VulkanLogicalDevice &device,
                   const SwapChain &swapChain,
                   VkFormat depthFormat);
        ~RenderPass();

        RenderPass(const RenderPass &) = delete;
        RenderPass &operator=(const RenderPass &) = delete;

        VkRenderPass get() const { return renderPass; }

    private:
        const VulkanLogicalDevice &device;
        VkRenderPass renderPass{VK_NULL_HANDLE};
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

        void create(const SwapChain &swapChain);
        void cleanup();
    };
} // namespace Vk
