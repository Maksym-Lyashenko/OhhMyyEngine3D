#pragma once

#include "VulkanLogicalDevice.h"
#include "SwapChain.h"
#include "CommandBuffers.h"
#include "SyncObjects.h"
#include "RenderPass.h"
#include "GraphicsPipeline.h"

#include "rhi/vk/gfx/Mesh.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace Render
{
    struct ViewUniforms;
}

namespace Vk
{

    /**
     * @brief Shared per-frame/per-swapchain rendering resources.
     *
     * Owns:
     *  - Descriptor pool for view UBO
     *  - Per-image UBO buffers + device memory
     *  - Per-image descriptor sets (set=0, binding=0)
     *
     * Does NOT own:
     *  - GraphicsPipeline's descriptor set layout (taken from pipeline)
     */
    struct RendererContext
    {
        VulkanLogicalDevice &device;
        SwapChain &swapChain;
        CommandBuffers &commandBuffers;
        SyncObjects &syncObjects;
        RenderPass &renderPass;
        GraphicsPipeline &graphicsPipeline;

        // Draw list is just borrowed pointers (no ownership)
        std::vector<const Vk::Gfx::Mesh *> drawList;

        // ---- View UBO/Descriptors (owned here) ----
        VkDescriptorPool descPool = VK_NULL_HANDLE;
        std::vector<VkBuffer> viewUbos;         // per-swapchain-image
        std::vector<VkDeviceMemory> viewUboMem; // per-swapchain-image
        std::vector<VkDescriptorSet> viewSets;  // per-swapchain-image

        RendererContext(VulkanLogicalDevice &d,
                        SwapChain &s,
                        CommandBuffers &c,
                        SyncObjects &so,
                        RenderPass &rp,
                        GraphicsPipeline &gp)
            : device(d), swapChain(s), commandBuffers(c),
              syncObjects(so), renderPass(rp), graphicsPipeline(gp) {}

        // Allocate per-image UBOs, descriptor pool and descriptor sets.
        // physDev is required to choose memory type for UBOs.
        void createViewResources(VkPhysicalDevice physDev);

        // Update UBO contents for the given swapchain image (map/memcpy/unmap).
        void updateViewUbo(uint32_t imageIndex, const Render::ViewUniforms &uboData) const;

        // Destroy UBO buffers/memory and descriptor pool.
        void destroyViewResources() noexcept;

        // Convenience accessor (valid after createViewResources).
        VkDescriptorSet viewSet(uint32_t imageIndex) const noexcept
        {
            return viewSets[imageIndex];
        }
    };

} // namespace Vk
