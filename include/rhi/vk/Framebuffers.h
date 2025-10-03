#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstddef>

namespace Vk
{

    class RenderPass;
    class ImageViews;
    class SwapChain;
    class VulkanLogicalDevice;

    /**
     * @brief Owns one VkFramebuffer per swapchain image.
     *
     * Requirements:
     *  - SwapChain, ImageViews, RenderPass, and VulkanLogicalDevice must outlive this object.
     *  - Depth view may be VK_NULL_HANDLE if depth is not used in the render pass.
     */
    class Framebuffers
    {
    public:
        Framebuffers(const VulkanLogicalDevice &device,
                     const RenderPass &renderPass,
                     const SwapChain &swapChain,
                     const ImageViews &imageViews,
                     VkImageView depthView);
        ~Framebuffers() noexcept;

        Framebuffers(const Framebuffers &) = delete;
        Framebuffers &operator=(const Framebuffers &) = delete;

        /// (Re)create all framebuffers. Safe to call multiple times.
        void create();
        /// Destroy all framebuffers. Safe to call multiple times.
        void cleanup() noexcept;

        const std::vector<VkFramebuffer> &getFramebuffers() const noexcept { return framebuffers; }
        VkFramebuffer get(size_t i) const noexcept { return framebuffers[i]; }

    private:
        const VulkanLogicalDevice &logicalDevice;
        const RenderPass &renderPass;
        const SwapChain &swapChain;
        const ImageViews &imageViews;
        VkImageView depthView = VK_NULL_HANDLE;

        std::vector<VkFramebuffer> framebuffers;
    };

} // namespace Vk
