#pragma once
#include <vector>
#include <vulkan/vulkan.h>

namespace Core
{

    struct RendererContext;
    class VulkanRenderer;

    class FrameRenderer
    {
    public:
        FrameRenderer(RendererContext &ctx,
                      VulkanRenderer &vulkanRenderer);

        FrameRenderer(const FrameRenderer &) = delete;
        FrameRenderer &operator=(const FrameRenderer &) = delete;

        void drawFrame();

    private:
        RendererContext &ctx;
        VulkanRenderer &vulkanRenderer;

        size_t currentFrame = 0;

        // fence per swapchain image (who now “in flight”)
        std::vector<VkFence> imagesInFlight;
    };

} // namespace Core
