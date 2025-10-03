#include "rhi/vk/RenderPass.h"
#include "rhi/vk/VulkanLogicalDevice.h"
#include "rhi/vk/SwapChain.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

namespace Vk
{

    RenderPass::RenderPass(const VulkanLogicalDevice &dev,
                           const SwapChain &swapChain,
                           VkFormat depthFmt,
                           VkSampleCountFlagBits samples_)
        : device(dev), depthFormat(depthFmt), samples(samples_)
    {
        create(swapChain);
    }

    RenderPass::~RenderPass() noexcept
    {
        cleanup();
    }

    void RenderPass::recreate(const SwapChain &swapChain,
                              VkFormat newDepthFormat,
                              VkSampleCountFlagBits newSamples)
    {
        cleanup();
        depthFormat = newDepthFormat;
        samples = newSamples;
        create(swapChain);
    }

    void RenderPass::create(const SwapChain &swapChain)
    {
        cleanup(); // idempotent

        // --- Color attachment (swapchain format) ---
        VkAttachmentDescription color{};
        color.format = swapChain.getImageFormat();
        color.samples = samples; // MSAA (VK_SAMPLE_COUNT_1_BIT by default)
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // keep for present
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // --- Optional depth attachment ---
        VkAttachmentDescription depth{};
        VkAttachmentReference depthRef{};
        bool useDepth = (depthFormat != VK_FORMAT_UNDEFINED);

        if (useDepth)
        {
            depth.format = depthFormat;
            depth.samples = samples;
            depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // not presenting depth
            depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            depthRef.attachment = 1;
            depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        // --- Subpass (single) ---
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = useDepth ? &depthRef : nullptr;

        // --- External dependency ---
        // Synchronize with external stages so our subpass can write color/depth safely.
        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.srcAccessMask = 0;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        // --- Assemble ---
        VkAttachmentDescription attachments[2];
        uint32_t attachmentCount = 0;
        attachments[attachmentCount++] = color;
        if (useDepth)
            attachments[attachmentCount++] = depth;

        VkRenderPassCreateInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpInfo.attachmentCount = attachmentCount;
        rpInfo.pAttachments = attachments;
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpass;
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies = &dep;

        VK_CHECK(vkCreateRenderPass(device.getDevice(), &rpInfo, nullptr, &renderPass));
        Core::Logger::log(Core::LogLevel::INFO, "RenderPass created successfully");
    }

    void RenderPass::cleanup() noexcept
    {
        if (renderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device.getDevice(), renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
            Core::Logger::log(Core::LogLevel::INFO, "RenderPass destroyed");
        }
    }

} // namespace Vk
