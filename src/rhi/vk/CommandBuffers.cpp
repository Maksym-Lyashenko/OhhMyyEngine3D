#include "rhi/vk/CommandBuffers.h"

#include "rhi/vk/CommandPool.h"
#include "rhi/vk/RenderPass.h"
#include "rhi/vk/Framebuffers.h"
#include "rhi/vk/GraphicsPipeline.h"
#include "rhi/vk/SwapChain.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

#include "rhi/vk/gfx/DrawItem.h"
#include "render/materials/Material.h"
#include "render/ViewUniforms.h" // per-view UBO layout (CPU side)

// no <glm/gtx/*> needed here

namespace Vk
{

    CommandBuffers::CommandBuffers(VkDevice device, const CommandPool &pool, std::size_t count)
        : device_(device)
    {
        allocate(pool, count);
    }

    void CommandBuffers::allocate(const CommandPool &pool, std::size_t count)
    {
        buffers_.resize(count);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = pool.get();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(count);

        VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, buffers_.data()));

        Core::Logger::log(Core::LogLevel::INFO,
                          "Allocated " + std::to_string(count) + " command buffers");
    }

    void CommandBuffers::record(uint32_t imageIndex,
                                const RenderPass &renderPass,
                                const Framebuffers &framebuffers,
                                const GraphicsPipeline &pipeline,
                                const SwapChain &swapchain,
                                const std::vector<Gfx::DrawItem> &items,
                                VkDescriptorSet viewSet)
    {
        if (imageIndex >= buffers_.size())
        {
            Core::Logger::log(Core::LogLevel::ERROR, "record(): imageIndex out of range");
            return;
        }

        VkCommandBuffer cmd = buffers_[imageIndex];

        // 1) Begin recording (SIMULTANEOUS_USE allows reuse without waiting)
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // ok for simple samples

        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

        // 2) Begin render pass with clear color/depth
        VkClearValue clears[2]{};
        clears[0].color = {{0.02f, 0.02f, 0.04f, 1.0f}};
        clears[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo rp{};
        rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp.renderPass = renderPass.get();
        rp.framebuffer = framebuffers.getFramebuffers()[imageIndex];
        rp.renderArea.offset = {0, 0};
        rp.renderArea.extent = swapchain.getExtent();
        rp.clearValueCount = 2;
        rp.pClearValues = clears;

        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

        // 3) Bind pipeline (descriptor set with ViewUniforms must be bound elsewhere)
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());

        // 4) Dynamic viewport & scissor
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

        // 5) Draw meshes
        //    We push only 'model' matrix as push-constants (64 bytes).

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

        // 6) End render pass
        vkCmdEndRenderPass(cmd);

        // 7) Finish recording
        VK_CHECK(vkEndCommandBuffer(cmd));
    }

} // namespace Vk
