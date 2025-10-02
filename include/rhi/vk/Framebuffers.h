#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Vk
{

    class RenderPass;
    class ImageViews;
    class SwapChain;
    class VulkanLogicalDevice;

    /**
     * Owns one VkFramebuffer per swapchain image view.
     * Lifetime must be after: SwapChain, ImageViews, RenderPass, LogicalDevice.
     */
    class Framebuffers
    {
    public:
        Framebuffers(const VulkanLogicalDevice &device,
                     const RenderPass &renderPass,
                     const SwapChain &swapChain,
                     const ImageViews &imageViews,
                     VkImageView depthView);
        ~Framebuffers();

        Framebuffers(const Framebuffers &) = delete;
        Framebuffers &operator=(const Framebuffers &) = delete;

        void create();
        void cleanup();

        const std::vector<VkFramebuffer> &getFramebuffers() const { return framebuffers; }

    private:
        const VulkanLogicalDevice &logicalDevice;
        const RenderPass &renderPass;
        const SwapChain &swapChain;
        const ImageViews &imageViews;
        VkImageView depthView = VK_NULL_HANDLE;

        std::vector<VkFramebuffer> framebuffers;
    };

} // namespace Vk
