#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace Core
{

    class CommandPool;
    class RenderPass;
    class Framebuffers;
    class GraphicsPipeline;
    class SwapChain;

    // Allocates and records one primary command buffer per swapchain image.
    class CommandBuffers
    {
    public:
        CommandBuffers(VkDevice device, const CommandPool &pool, size_t count);
        ~CommandBuffers() = default;

        CommandBuffers(const CommandBuffers &) = delete;
        CommandBuffers &operator=(const CommandBuffers &) = delete;

        // Re-record commands for a particular swapchain image index.
        void record(uint32_t imageIndex,
                    const RenderPass &renderPass,
                    const Framebuffers &framebuffers,
                    const GraphicsPipeline &pipeline,
                    const SwapChain &swapchain);

        VkCommandBuffer operator[](size_t i) const { return buffers[i]; }
        size_t size() const { return buffers.size(); }
        const std::vector<VkCommandBuffer> &getCommandBuffers() const { return buffers; }

    private:
        VkDevice device{};
        std::vector<VkCommandBuffer> buffers;

        void allocate(const CommandPool &pool, size_t count);
    };

} // namespace Core
