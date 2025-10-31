#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <vector>

namespace Render
{
    struct ViewUniforms; // per-view UBO (view/proj/viewProj/etc.)
}

namespace Render
{
    class Material;
}

namespace UI
{
    class ImGuiLayer;
}

namespace Vk
{

    class CommandPool;
    class RenderPass;
    class Framebuffers;
    class GraphicsPipeline;
    class SwapChain;
    class ImageViews;
    class DepthResources;

    namespace Gfx
    {
        struct DrawItem;
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
        // Now binds *two* descriptor sets:
        //   set=0 : view (UBO per image)
        //   set=1 : material (albedo sampler) -- TEMPORARY single set reused for all draws
        void record(uint32_t imageIndex,
                    const GraphicsPipeline &pipeline,
                    const SwapChain &swapchain,
                    const ImageViews &imageViews,
                    const DepthResources &depth,
                    const std::vector<Gfx::DrawItem> &items,
                    VkDescriptorSet viewSet);

        // record only ImGui draw commands for given image index (called each frame)
        void recordImGuiForImage(uint32_t imageIndex,
                                 const SwapChain &swapchain,
                                 const ImageViews &imageViews,
                                 const DepthResources &depth,
                                 UI::ImGuiLayer &imguiLayer);

        // Accessors (add to public API)
        VkCommandBuffer sceneCommand(uint32_t imageIndex) const { return sceneBuffers_.at(imageIndex); }
        VkCommandBuffer uiCommand(uint32_t imageIndex) const { return uiBuffers_.at(imageIndex); }

    private:
        VkDevice device_{};
        std::vector<VkCommandBuffer> sceneBuffers_; // pre-recorded scene commands (one per image)
        std::vector<VkCommandBuffer> uiBuffers_;    // per-frame ImGui commands (one per image)

        void allocate(const CommandPool &pool, std::size_t count);
    };

} // namespace Vk
