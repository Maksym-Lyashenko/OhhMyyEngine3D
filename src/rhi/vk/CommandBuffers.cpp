#include "rhi/vk/CommandBuffers.h"

#include "rhi/vk/CommandPool.h"
#include "rhi/vk/GraphicsPipeline.h"
#include "rhi/vk/SwapChain.h"
#include "rhi/vk/ImageViews.h"
#include "rhi/vk/DepthResources.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

#include "ui/ImGuiLayer.h"

#include "rhi/vk/gfx/DrawItem.h"
#include "render/materials/Material.h"
#include "render/ViewUniforms.h"

namespace Vk
{

    CommandBuffers::CommandBuffers(VkDevice device, const CommandPool &pool, std::size_t count)
        : device_(device)
    {
        allocate(pool, count);
    }

    void CommandBuffers::allocate(const CommandPool &pool, std::size_t count)
    {
        sceneBuffers_.resize(count);
        uiBuffers_.resize(count);

        // Allocate scene buffers (primary)
        VkCommandBufferAllocateInfo allocScene{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocScene.commandPool = pool.get();
        allocScene.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocScene.commandBufferCount = static_cast<uint32_t>(count);

        VK_CHECK(vkAllocateCommandBuffers(device_, &allocScene, sceneBuffers_.data()));

        // Allocate UI buffers (primary) — separate pool allocation call
        VkCommandBufferAllocateInfo allocUi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocUi.commandPool = pool.get();
        allocUi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocUi.commandBufferCount = static_cast<uint32_t>(count);

        VK_CHECK(vkAllocateCommandBuffers(device_, &allocUi, uiBuffers_.data()));

        Core::Logger::log(Core::LogLevel::INFO,
                          "Allocated " + std::to_string(count) + " scene command buffers and " +
                              std::to_string(count) + " ui command buffers");
    }

    void CommandBuffers::record(uint32_t imageIndex,
                                const GraphicsPipeline &pipeline,
                                const SwapChain &swapchain,
                                const ImageViews &imageViews,
                                const DepthResources &depth,
                                const std::vector<Gfx::DrawItem> &items,
                                VkDescriptorSet viewSet)
    {
        if (imageIndex >= sceneBuffers_.size())
        {
            Core::Logger::log(Core::LogLevel::ERROR, "record(): imageIndex out of range");
            return;
        }

        VkCommandBuffer cmd = sceneBuffers_[imageIndex];
        VkImage swapchainImage = swapchain.getImages()[imageIndex];

        // 1) Begin recording
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

        // 2) TRANSITION: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL (для сцены)
        VkImageMemoryBarrier acquireBarrier{};
        acquireBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        acquireBarrier.srcAccessMask = 0;
        acquireBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        acquireBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        acquireBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        acquireBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        acquireBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        acquireBarrier.image = swapchainImage;
        acquireBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        acquireBarrier.subresourceRange.baseMipLevel = 0;
        acquireBarrier.subresourceRange.levelCount = 1;
        acquireBarrier.subresourceRange.baseArrayLayer = 0;
        acquireBarrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &acquireBarrier);

        // 3) Setup clear values
        VkClearValue clears[2]{};
        clears[0].color = {{0.02f, 0.02f, 0.04f, 1.0f}};
        clears[1].depthStencil = {1.0f, 0};

        // 4) Begin dynamic rendering
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, swapchain.getExtent()};
        renderingInfo.layerCount = 1;

        // Color attachment
        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = imageViews.get(imageIndex);
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = clears[0];

        // Depth attachment
        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = depth.getView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue = clears[1];

        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        // 5) Bind pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());

        // 6) Dynamic viewport & scissor
        const auto extent = swapchain.getExtent();

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // 7) Draw meshes
        for (const Gfx::DrawItem &it : items)
        {
            if (!it.mesh || !it.material)
                continue;

            // Bind descriptor sets: [0] view UBO, [1] material
            VkDescriptorSet sets[2] = {viewSet, it.material->descriptorSet()};
            vkCmdBindDescriptorSets(cmd,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline.getPipelineLayout(),
                                    /*firstSet*/ 0, /*setCount*/ 2, sets,
                                    /*dynamicOffsetCount*/ 0, /*pDynamicOffsets*/ nullptr);

            // Bind geometry
            it.mesh->bind(cmd);

            // Push constants: model matrix only (64 bytes)
            glm::mat4 model = it.mesh->getLocalTransform();
            vkCmdPushConstants(cmd, pipeline.getPipelineLayout(),
                               VK_SHADER_STAGE_VERTEX_BIT, 0,
                               static_cast<uint32_t>(sizeof(glm::mat4)), &model);

            it.mesh->draw(cmd);
        }

        // 8) End dynamic rendering
        vkCmdEndRendering(cmd);

        // 9) TRANSITION: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR (для презентации)
        VkImageMemoryBarrier presentBarrier{};
        presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        presentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        presentBarrier.dstAccessMask = 0;
        presentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentBarrier.image = swapchainImage;
        presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        presentBarrier.subresourceRange.baseMipLevel = 0;
        presentBarrier.subresourceRange.levelCount = 1;
        presentBarrier.subresourceRange.baseArrayLayer = 0;
        presentBarrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &presentBarrier);

        // 10) Finish recording
        VK_CHECK(vkEndCommandBuffer(cmd));
    }

    void CommandBuffers::recordImGuiForImage(uint32_t imageIndex,
                                             const SwapChain &swapchain,
                                             const ImageViews &imageViews,
                                             const DepthResources &depth,
                                             UI::ImGuiLayer &imguiLayer)
    {
        if (imageIndex >= uiBuffers_.size())
        {
            Core::Logger::log(Core::LogLevel::ERROR, "recordImGuiForImage(): imageIndex out of range");
            return;
        }

        VkCommandBuffer cmd = uiBuffers_[imageIndex];
        VkImage swapchainImage = swapchain.getImages()[imageIndex];

        // Begin with SIMULTANEOUS_USE so we can re-record while GPU may still hold previous submissions.
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

        // TRANSITION: PRESENT_SRC_KHR -> COLOR_ATTACHMENT_OPTIMAL (для UI)
        VkImageMemoryBarrier uiAcquireBarrier{};
        uiAcquireBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        uiAcquireBarrier.srcAccessMask = 0;
        uiAcquireBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        uiAcquireBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        uiAcquireBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        uiAcquireBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        uiAcquireBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        uiAcquireBarrier.image = swapchainImage;
        uiAcquireBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        uiAcquireBarrier.subresourceRange.baseMipLevel = 0;
        uiAcquireBarrier.subresourceRange.levelCount = 1;
        uiAcquireBarrier.subresourceRange.baseArrayLayer = 0;
        uiAcquireBarrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &uiAcquireBarrier);

        // Clear values for attachments
        VkClearValue clears[2]{};
        clears[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
        clears[1].depthStencil = {1.0f, 0};

        // Begin dynamic rendering
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, swapchain.getExtent()};
        renderingInfo.layerCount = 1;

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = imageViews.get(imageIndex);
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = clears[0];

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = depth.getView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue = clears[1];

        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        // Render ImGui
        imguiLayer.render(cmd);

        vkCmdEndRendering(cmd);

        // TRANSITION: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR (после UI)
        VkImageMemoryBarrier uiPresentBarrier{};
        uiPresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        uiPresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        uiPresentBarrier.dstAccessMask = 0;
        uiPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        uiPresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        uiPresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        uiPresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        uiPresentBarrier.image = swapchainImage;
        uiPresentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        uiPresentBarrier.subresourceRange.baseMipLevel = 0;
        uiPresentBarrier.subresourceRange.levelCount = 1;
        uiPresentBarrier.subresourceRange.baseArrayLayer = 0;
        uiPresentBarrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &uiPresentBarrier);

        VK_CHECK(vkEndCommandBuffer(cmd));
    }

} // namespace Vk