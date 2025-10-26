#pragma once

#include "VulkanLogicalDevice.h"
#include "SwapChain.h"
#include "CommandBuffers.h"
#include "SyncObjects.h"
#include "RenderPass.h"
#include "GraphicsPipeline.h"

#include "rhi/vk/gfx/Mesh.h"

#include "rhi/vk/FrameResources.h"

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

    extern VkDescriptorPool viewDescPool;                  // single pool for all view sets
    extern std::vector<Vk::FrameResources> frameResources; // one per swapchain image

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

        RendererContext(VulkanLogicalDevice &d,
                        SwapChain &s,
                        CommandBuffers &c,
                        SyncObjects &so,
                        RenderPass &rp,
                        GraphicsPipeline &gp) noexcept
            : device(d), swapChain(s), commandBuffers(c),
              syncObjects(so), renderPass(rp), graphicsPipeline(gp) {}

        ~RendererContext() noexcept { destroyViewResources(); }

        RendererContext(const RendererContext &) = delete;
        RendererContext &operator=(const RendererContext &) = delete;
        RendererContext(RendererContext &&) = default;
        RendererContext &operator=(RendererContext &&) = default;

        /**
         * @brief Allocate per-image UBOs, descriptor pool and descriptor sets.
         * @param physDev Physical device (for memory type selection)
         *
         * Lifecycle: call after pipeline creation (we need its set layout) and after swapchain create.
         * Destroy with destroyViewResources() before swapchain re-create or shutdown.
         */
        void createViewResources(VkPhysicalDevice physDev);

        /**
         * @brief Update UBO contents for the given swapchain image.
         *        Simple map/memcpy/unmap (HOST_VISIBLE | HOST_COHERENT).
         */
        void updateViewUbo(uint32_t imageIndex, const Render::ViewUniforms &uboData) const;

        /**
         * @brief Destroy UBO buffers/memory and descriptor pool.
         *        Safe to call multiple times.
         */
        void destroyViewResources() noexcept;

        // Convenience accessor (valid after createViewResources).
        [[nodiscard]] VkDescriptorSet viewSet(uint32_t imageIndex) const noexcept
        {
            return frameResources[imageIndex].viewSet;
        }
    };

} // namespace Vk
