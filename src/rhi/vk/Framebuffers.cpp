#include "rhi/vk/Framebuffers.h"

#include "rhi/vk/VulkanLogicalDevice.h"
#include "rhi/vk/RenderPass.h"
#include "rhi/vk/SwapChain.h"
#include "rhi/vk/ImageViews.h"

#include "core/Logger.h"
#include <rhi/vk/Common.h>

using namespace Core;

namespace Vk
{

    Framebuffers::Framebuffers(const VulkanLogicalDevice &device,
                               const RenderPass &renderPass,
                               const SwapChain &swapChain,
                               const ImageViews &imageViews,
                               VkImageView depthViewIn)
        : logicalDevice(device),
          renderPass(renderPass),
          swapChain(swapChain),
          imageViews(imageViews),
          depthView(depthViewIn)
    {
    }

    Framebuffers::~Framebuffers()
    {
        cleanup();
    }

    void Framebuffers::create()
    {
        cleanup(); // safe-guard if called twice

        // NOTE: We assume your helpers expose:
        //   - RenderPass::getRenderPass() -> VkRenderPass
        //   - SwapChain::getExtent()      -> VkExtent2D
        //   - ImageViews::getViews()      -> const std::vector<VkImageView>&
        //
        // If your names differ (e.g. getImageViews()), just rename here accordingly.

        const auto &views = imageViews.getViews();
        const auto extent = swapChain.getExtent();
        framebuffers.resize(views.size());

        for (size_t i = 0; i < views.size(); ++i)
        {
            // We have a single color attachment per framebuffer.
            VkImageView attachments[2] = {views[i], depthView};

            VkFramebufferCreateInfo fbInfo{};
            fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass = renderPass.get(); // must match the attachment format/layout
            fbInfo.attachmentCount = 2;
            fbInfo.pAttachments = attachments;
            fbInfo.width = extent.width;
            fbInfo.height = extent.height;
            fbInfo.layers = 1;

            VK_CHECK(vkCreateFramebuffer(logicalDevice.getDevice(), &fbInfo, nullptr, &framebuffers[i]));
        }

        Logger::log(LogLevel::INFO, "Created " + std::to_string(framebuffers.size()) + " framebuffers");
    }

    void Framebuffers::cleanup()
    {
        if (!framebuffers.empty())
        {
            for (auto fb : framebuffers)
            {
                if (fb != VK_NULL_HANDLE)
                {
                    vkDestroyFramebuffer(logicalDevice.getDevice(), fb, nullptr);
                }
            }
            framebuffers.clear();
            Logger::log(LogLevel::INFO, "Framebuffers destroyed");
        }
    }

} // namespace Vk
