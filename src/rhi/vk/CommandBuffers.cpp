#include "rhi/vk/CommandBuffers.h"

#include "rhi/vk/CommandPool.h"
#include "rhi/vk/RenderPass.h"
#include "rhi/vk/Framebuffers.h"
#include "rhi/vk/GraphicsPipeline.h"
#include "rhi/vk/SwapChain.h"

#include "core/Logger.h"
#include <rhi/vk/Common.h>

#include "rhi/vk/gfx/Mesh.h"
#include "render/MVP.h"
#include "render/ViewUniforms.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "core/math/MathUtils.h"

using namespace Core;

namespace Vk
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
                                const SwapChain &swapchain,
                                const std::vector<const Gfx::Mesh *> &meshes,
                                Render::ViewUniforms &view)
    {
        VkCommandBuffer cmd = buffers[imageIndex];

        // 1) Begin recording (SIMULTANEOUS_USE allows reuse without waiting)
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

        // 2) Begin render pass with a clear color
        VkClearValue clears[2]{};
        clears[0].color = {{0.02f, 0.02f, 0.04f, 1.0f}};
        clears[1].depthStencil = {1.0f, 0}; // clear depth

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = renderPass.get();
        rpInfo.framebuffer = framebuffers.getFramebuffers()[imageIndex];
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = swapchain.getExtent();
        rpInfo.clearValueCount = 2;
        rpInfo.pClearValues = clears;

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

        // 5) Draw call
        if (!meshes.empty())
        {
            Render::MVP base{};
            base.view = view.view;
            base.proj = view.proj;
            base.viewProj = view.viewProj;

            for (auto *mesh : meshes)
            {
                Render::MVP mvp = base;
                mvp.model = mesh->getLocalTransform();

                mesh->bind(cmd);
                vkCmdPushConstants(cmd, pipeline.getPipelineLayout(),
                                   VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Render::MVP), &mvp);
                mesh->draw(cmd);
            }
        }

        // 6) End render pass
        vkCmdEndRenderPass(cmd);

        // 7) Finish recording
        VK_CHECK(vkEndCommandBuffer(cmd));
    }

} // namespace Vk
