#pragma once

#include <vulkan/vulkan.h>

namespace Core
{
    class VulkanLogicalDevice;
    class SwapChain;

    class RenderPass
    {
    public:
        RenderPass(const VulkanLogicalDevice &device, const SwapChain &swapChain);
        ~RenderPass();

        RenderPass(const RenderPass &) = delete;
        RenderPass &operator=(const RenderPass &) = delete;

        VkRenderPass get() const { return renderPass; }

    private:
        const VulkanLogicalDevice &device;
        VkRenderPass renderPass{VK_NULL_HANDLE};

        void create(const SwapChain &swapChain);
        void cleanup();
    };
}
