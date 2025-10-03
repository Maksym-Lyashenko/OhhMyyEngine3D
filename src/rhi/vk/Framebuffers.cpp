#include "rhi/vk/Framebuffers.h"

#include "rhi/vk/VulkanLogicalDevice.h"
#include "rhi/vk/RenderPass.h"
#include "rhi/vk/SwapChain.h"
#include "rhi/vk/ImageViews.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

#include <stdexcept>

namespace Vk
{

    Framebuffers::Framebuffers(const VulkanLogicalDevice &device,
                               const RenderPass &rp,
                               const SwapChain &sc,
                               const ImageViews &iv,
                               VkImageView depthViewIn)
        : logicalDevice(device), renderPass(rp), swapChain(sc), imageViews(iv), depthView(depthViewIn)
    {
    }

    Framebuffers::~Framebuffers() noexcept
    {
        cleanup();
    }

    void Framebuffers::create()
    {
        cleanup(); // idempotent

        const auto &views = imageViews.getViews();
        const auto extent = swapChain.getExtent();

        if (views.empty())
        {
            throw std::runtime_error("Framebuffers::create: no color image views provided");
        }

        framebuffers.resize(views.size());

        for (size_t i = 0; i < views.size(); ++i)
        {
            // Build attachment list dynamically (color [+ depth])
            VkImageView attachments[2];
            uint32_t attachCount = 0;

            attachments[attachCount++] = views[i];
            if (depthView != VK_NULL_HANDLE)
            {
                attachments[attachCount++] = depthView;
            }

            VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fbInfo.renderPass = renderPass.get(); // must match formats/layouts declared in RenderPass
            fbInfo.attachmentCount = attachCount;
            fbInfo.pAttachments = attachments;
            fbInfo.width = extent.width;
            fbInfo.height = extent.height;
            fbInfo.layers = 1;

            VK_CHECK(vkCreateFramebuffer(logicalDevice.getDevice(), &fbInfo, nullptr, &framebuffers[i]));
        }

        Core::Logger::log(Core::LogLevel::INFO,
                          "Created " + std::to_string(framebuffers.size()) + " framebuffers");
    }

    void Framebuffers::cleanup() noexcept
    {
        if (framebuffers.empty())
            return;

        for (auto fb : framebuffers)
        {
            if (fb != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(logicalDevice.getDevice(), fb, nullptr);
            }
        }
        framebuffers.clear();
        Core::Logger::log(Core::LogLevel::INFO, "Framebuffers destroyed");
    }

} // namespace Vk
