#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <vector>

namespace Render
{
    struct ViewUniforms; // per-view UBO (view/proj/viewProj/etc.)
}

namespace Vk
{

    class CommandPool;
    class RenderPass;
    class Framebuffers;
    class GraphicsPipeline;
    class SwapChain;

    namespace Gfx
    {
        class Mesh;
    }

    /**
     * @brief Owns one primary command buffer per swapchain image and records a simple draw list.
     *
     * Recording policy:
     *   - per-frame UBO (Render::ViewUniforms) is expected to be already bound externally
     *     via descriptor sets (set/binding defined in your pipeline layout);
     *   - per-object data (model matrix) is pushed as push-constants (64 bytes).
     */
    class CommandBuffers
    {
    public:
        CommandBuffers(VkDevice device, const CommandPool &pool, std::size_t count);
        ~CommandBuffers() = default;

        CommandBuffers(const CommandBuffers &) = delete;
        CommandBuffers &operator=(const CommandBuffers &) = delete;

        /// Record commands for a particular swapchain image index.
        void record(uint32_t imageIndex,
                    const RenderPass &renderPass,
                    const Framebuffers &framebuffers,
                    const GraphicsPipeline &pipeline,
                    const SwapChain &swapchain,
                    const std::vector<const Gfx::Mesh *> &meshes,
                    VkDescriptorSet viewSet);

        [[nodiscard]] VkCommandBuffer operator[](std::size_t i) const { return buffers_[i]; }
        [[nodiscard]] std::size_t size() const { return buffers_.size(); }
        [[nodiscard]] const std::vector<VkCommandBuffer> &getCommandBuffers() const { return buffers_; }

    private:
        VkDevice device_{};
        std::vector<VkCommandBuffer> buffers_;

        void allocate(const CommandPool &pool, std::size_t count);
    };

} // namespace Vk
