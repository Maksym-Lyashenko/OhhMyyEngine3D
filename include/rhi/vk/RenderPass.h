#pragma once

#include <vulkan/vulkan.h>

namespace Vk
{

    class VulkanLogicalDevice;
    class SwapChain;

    /**
     * @brief RAII wrapper over VkRenderPass for a single subpass (color [+ depth]).
     *
     * Requirements:
     *  - VulkanLogicalDevice and SwapChain outlive this object.
     *  - If depthFormat == VK_FORMAT_UNDEFINED, depth attachment is omitted.
     *  - MSAA controlled by 'samples' (default 1).
     */
    class RenderPass
    {
    public:
        RenderPass(const VulkanLogicalDevice &device,
                   const SwapChain &swapChain,
                   VkFormat depthFormat,
                   VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
        ~RenderPass() noexcept;

        RenderPass(const RenderPass &) = delete;
        RenderPass &operator=(const RenderPass &) = delete;

        /// Underlying handle.
        VkRenderPass get() const noexcept { return renderPass; }

        /// Destroy and create again (useful on resize / MSAA change).
        void recreate(const SwapChain &swapChain,
                      VkFormat newDepthFormat,
                      VkSampleCountFlagBits newSamples = VK_SAMPLE_COUNT_1_BIT);

    private:
        const VulkanLogicalDevice &device;
        VkRenderPass renderPass{VK_NULL_HANDLE};
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

        void create(const SwapChain &swapChain);
        void cleanup() noexcept;
    };

} // namespace Vk
