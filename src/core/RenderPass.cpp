#include "core/RenderPass.h"
#include "core/VulkanLogicalDevice.h"
#include "core/SwapChain.h"

#include "core/Logger.h"
#include <core/VulkanUtils.h>

namespace Core
{
    RenderPass::RenderPass(const VulkanLogicalDevice &device, const SwapChain &swapChain)
        : device(device)
    {
        create(swapChain);
    }

    RenderPass::~RenderPass()
    {
        cleanup();
    }

    void RenderPass::create(const SwapChain &swapChain)
    {
        cleanup(); // in case of re-create

        // --- Attachment description ---
        // This describes the color attachment that will be used during rendering
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChain.getImageFormat(); // must match swapchain images
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // clear before draw
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // store after draw
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Reference to the attachment from a subpass
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // --- Subpass ---
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        // --- Subpass dependency ---
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // --- RenderPass create info ---
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VK_CHECK(vkCreateRenderPass(device.getDevice(), &renderPassInfo, nullptr, &renderPass));

        Logger::log(LogLevel::INFO, "RenderPass created successfully");
    }

    void RenderPass::cleanup()
    {
        if (renderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device.getDevice(), renderPass, nullptr);
            Logger::log(LogLevel::INFO, "RenderPass destroyed");
            renderPass = VK_NULL_HANDLE;
        }
    }
}
