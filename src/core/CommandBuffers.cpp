#include "core/CommandBuffers.h"

#include "core/CommandPool.h"
#include "core/RenderPass.h"
#include "core/Framebuffers.h"
#include "core/GraphicsPipeline.h"
#include "core/SwapChain.h"

#include "core/Logger.h"
#include <core/VulkanUtils.h>

namespace Core
{

    CommandBuffers::CommandBuffers(VkDevice device, const CommandPool &pool, size_t count)
        : device(device)
    {
        allocate(pool, count);
    }

    void CommandBuffers::allocate(const CommandPool &pool, size_t count)
    {
        buffers.resize(count);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = pool.get();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(count);

        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, buffers.data()));

        Logger::log(LogLevel::INFO, "Allocated " + std::to_string(count) + " command buffers");
    }

    void CommandBuffers::record(uint32_t imageIndex,
                                const RenderPass &renderPass,
                                const Framebuffers &framebuffers,
                                const GraphicsPipeline &pipeline,
                                const SwapChain &swapchain)
    {
        VkCommandBuffer cmd = buffers[imageIndex];

        // 1) Begin recording (SIMULTANEOUS_USE allows reuse without waiting)
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

        // 2) Begin render pass with a clear color
        VkClearValue clearColor{};
        clearColor.color = {{0.02f, 0.02f, 0.04f, 1.0f}};

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = renderPass.get();
        rpInfo.framebuffer = framebuffers.getFramebuffers()[imageIndex];
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = swapchain.getExtent();
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        // 3) Bind graphics pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());

        // 4) Dynamic viewport & scissor (pipeline declared them as dynamic)
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapchain.getExtent().width);
        viewport.height = static_cast<float>(swapchain.getExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchain.getExtent();
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // 5) Minimal draw call: 3 vertices, no vertex buffer
        vkCmdDraw(cmd, 3, 1, 0, 0);

        // 6) End render pass
        vkCmdEndRenderPass(cmd);

        // 7) Finish recording
        VK_CHECK(vkEndCommandBuffer(cmd));
    }

} // namespace Core
