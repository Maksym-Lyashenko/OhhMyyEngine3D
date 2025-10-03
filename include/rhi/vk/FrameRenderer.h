#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <cstddef>

namespace Vk
{

    struct RendererContext; // holds swapchain, device, sync, command buffers, etc.

    class VulkanRenderer; // high-level app driver (resize/recreate hooks)

    /**
     * @brief Per-frame orchestration: acquire → submit → present.
     *
     * Notes:
     *  - Uses one fence per frame-in-flight (from SyncObjects) and tracks
     *    which swapchain image is currently "in flight" via imagesInFlight[imageIndex].
     *  - On VK_ERROR_OUT_OF_DATE_KHR / VK_SUBOPTIMAL_KHR we mark swapchain dirty
     *    and return early (caller will recreate before the next frame).
     */
    class FrameRenderer
    {
    public:
        FrameRenderer(RendererContext &ctx, VulkanRenderer &vulkanRenderer);

        FrameRenderer(const FrameRenderer &) = delete;
        FrameRenderer &operator=(const FrameRenderer &) = delete;

        /// Render one frame. May early-return if swapchain must be recreated.
        void drawFrame();

    private:
        RendererContext &ctx;
        VulkanRenderer &vulkanRenderer;

        std::size_t currentFrame = 0;        // rotating index in [0, maxFramesInFlight)
        std::vector<VkFence> imagesInFlight; // per swapchain image: fence currently using it (or VK_NULL_HANDLE)
    };

} // namespace Vk
