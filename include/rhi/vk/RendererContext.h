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

        // ---- (TEMPORARY) View UBO/Descriptors (owned here) ----
        // TODO: Move to a dedicated "ViewResources" or "FrameResources" module.
        VkDescriptorPool descPool = VK_NULL_HANDLE; // TEMPORARY
        std::vector<VkBuffer> viewUbos;             // TEMPORARY: per-swapchain-image UBO buffers
        std::vector<VkDeviceMemory> viewUboMem;     // TEMPORARY: matching memory objects
        std::vector<VkDescriptorSet> viewSets;      // TEMPORARY: set=0 per image

        VkDescriptorPool materialPool = VK_NULL_HANDLE; // TEMPORARY
        VkDescriptorSet materialSet = VK_NULL_HANDLE;   // TEMPORARY

        // TEMPORARY : create a single material descriptor set(set = 1).
        // Expects a valid sampled image: view + sampler + layout (typically SHADER_READ_ONLY_OPTIMAL).
        void createMaterialSet(VkImageView imageView,
                               VkSampler sampler,
                               VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VkDescriptorSet getMaterialSet() const noexcept { return materialSet; }

        RendererContext(VulkanLogicalDevice &d,
                        SwapChain &s,
                        CommandBuffers &c,
                        SyncObjects &so,
                        RenderPass &rp,
                        GraphicsPipeline &gp)
            : device(d), swapChain(s), commandBuffers(c),
              syncObjects(so), renderPass(rp), graphicsPipeline(gp) {}

        /**
         * @brief (TEMPORARY) Allocate per-image UBOs, descriptor pool and descriptor sets.
         * @param physDev Physical device (for memory type selection)
         *
         * Lifecycle: call after pipeline creation (we need its set layout) and after swapchain create.
         * Destroy with destroyViewResources() before swapchain re-create or shutdown.
         */
        void createViewResources(VkPhysicalDevice physDev);

        /**
         * @brief (TEMPORARY) Update UBO contents for the given swapchain image.
         *        Simple map/memcpy/unmap (HOST_VISIBLE | HOST_COHERENT).
         */
        void updateViewUbo(uint32_t imageIndex, const Render::ViewUniforms &uboData) const;

        /**
         * @brief (TEMPORARY) Destroy UBO buffers/memory and descriptor pool.
         *        Safe to call multiple times.
         */
        void destroyViewResources() noexcept;

        // Convenience accessor (valid after createViewResources).
        VkDescriptorSet viewSet(uint32_t imageIndex) const noexcept
        {
            return viewSets[imageIndex];
        }
    };

} // namespace Vk
