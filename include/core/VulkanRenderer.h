#pragma once

#include "core/WindowManager.h"

#include <memory>

namespace Core
{
    class VulkanInstance;
    class Surface;
    class VulkanPhysicalDevice;
    class VulkanLogicalDevice;
    class SwapChain;
    class ImageViews;
    class RenderPass;
    class Framebuffers;
    class GraphicsPipeline;
    class CommandPool;
    class CommandBuffers;
    class SyncObjects;
    struct RendererContext;
    class FrameRenderer;

    class VulkanRenderer
    {
    public:
        VulkanRenderer();
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer &) = delete;
        VulkanRenderer &operator=(const VulkanRenderer &) = delete;

        void run(); // Main loop

        void recreateSwapChain();

        void setFramebufferResized(bool resized) { framebufferResized = resized; }

        void markSwapchainDirty() { swapchainDirty = true; }
        void maybeRecreateSwapchain();

    private:
        WindowManager window;

        std::unique_ptr<VulkanInstance> instance;
        std::unique_ptr<Surface> surface;
        std::unique_ptr<VulkanPhysicalDevice> physicalDevice;
        std::unique_ptr<VulkanLogicalDevice> logicalDevice;
        std::unique_ptr<SwapChain> swapChain;
        std::unique_ptr<ImageViews> imageViews;
        std::unique_ptr<RenderPass> renderPass;
        std::unique_ptr<Framebuffers> framebuffers;
        std::unique_ptr<GraphicsPipeline> graphicsPipeline;
        std::unique_ptr<CommandPool> commandPool;
        std::unique_ptr<CommandBuffers> commandBuffers;
        std::unique_ptr<SyncObjects> syncObjects;
        std::unique_ptr<RendererContext> ctx;
        std::unique_ptr<FrameRenderer> frameRenderer;

        bool framebufferResized = false;
        bool swapchainDirty = false;

        void init();
        void mainLoop();
        void cleanup();
    };
}
