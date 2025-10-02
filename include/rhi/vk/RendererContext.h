#pragma once

#include "VulkanLogicalDevice.h"
#include "SwapChain.h"
#include "CommandBuffers.h"
#include "SyncObjects.h"
#include "RenderPass.h"
#include "GraphicsPipeline.h"

#include "rhi/vk/gfx/Mesh.h"

namespace Vk
{

    struct RendererContext
    {
        VulkanLogicalDevice &device;
        SwapChain &swapChain;
        CommandBuffers &commandBuffers;
        SyncObjects &syncObjects;
        RenderPass &renderPass;
        GraphicsPipeline &graphicsPipeline;
        std::vector<const Vk::Gfx::Mesh *> drawList;

        RendererContext(VulkanLogicalDevice &d,
                        SwapChain &s,
                        CommandBuffers &c,
                        SyncObjects &so,
                        RenderPass &rp,
                        GraphicsPipeline &gp)
            : device(d), swapChain(s), commandBuffers(c),
              syncObjects(so), renderPass(rp), graphicsPipeline(gp) {}
    };

} // namespace Vk