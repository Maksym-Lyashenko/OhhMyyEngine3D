#include "core/VulkanRenderer.h"

#include "core/Logger.h"

#include "core/VulkanInstance.h"
#include "core/Surface.h"
#include "core/VulkanPhysicalDevice.h"
#include "core/VulkanLogicalDevice.h"
#include "core/SwapChain.h"
#include "core/ImageViews.h"
#include "core/RenderPass.h"
#include "core/Framebuffers.h"
#include "core/GraphicsPipeline.h"
#include "core/CommandPool.h"
#include "core/CommandBuffers.h"
#include "core/SyncObjects.h"
#include "core/RendererContext.h"
#include "core/FrameRenderer.h"
#include <core/VulkanUtils.h>

namespace Core
{
    VulkanRenderer::VulkanRenderer()
        : window(800, 600, "OhhMyyEngine3D")
    {
        init();
    }

    VulkanRenderer::~VulkanRenderer()
    {
        cleanup();
    }

    void VulkanRenderer::init()
    {
        Logger::log(LogLevel::INFO, "VulkanRenderer initialized");

        window.onFramebufferResize = [&](int /*w*/, int /*h*/)
        {
            markSwapchainDirty();
        };

        // 1. Vulkan instance
        instance = std::make_unique<VulkanInstance>(window, true);

        // 2. Surface (after instance is created)
        surface = std::make_unique<Surface>(instance->getInstance(), window);
        surface->create();

        // 3. Physical device (after surface exists)
        physicalDevice = std::make_unique<VulkanPhysicalDevice>(instance->getInstance(), *surface);

        // 4. Logical device (after physical device exists)
        logicalDevice = std::make_unique<VulkanLogicalDevice>(*physicalDevice);

        // 5. Swap Chain (after logical device exists)
        swapChain = std::make_unique<SwapChain>(*physicalDevice, *logicalDevice, *surface, window);
        swapChain->create();

        // 6. Render Pass
        renderPass = std::make_unique<RenderPass>(*logicalDevice, *swapChain);

        // 7. Graphics papiline
        graphicsPipeline = std::make_unique<GraphicsPipeline>(*logicalDevice, *swapChain, *renderPass);

        // 8. Image Views
        imageViews = std::make_unique<ImageViews>(
            logicalDevice->getDevice(),
            swapChain->getImages(),
            swapChain->getImageFormat());

        imageViews->create();

        // 9. Framebuffers (need: device, render pass, extent, image views)
        framebuffers = std::make_unique<Framebuffers>(
            *logicalDevice,
            *renderPass,
            *swapChain,
            *imageViews);
        framebuffers->create();

        // 10. Command pool & buffers
        commandPool = std::make_unique<CommandPool>(logicalDevice->getDevice(),
                                                    physicalDevice->getQueueFamilies().graphicsFamily.value());

        commandBuffers = std::make_unique<CommandBuffers>(logicalDevice->getDevice(),
                                                          *commandPool,
                                                          framebuffers->getFramebuffers().size());

        for (uint32_t i = 0; i < swapChain->getImages().size(); ++i)
        {
            commandBuffers->record(i, *renderPass, *framebuffers, *graphicsPipeline, *swapChain);
        }

        // 11. Sync objects
        syncObjects = std::make_unique<SyncObjects>(logicalDevice->getDevice(),
                                                    static_cast<uint32_t>(swapChain->getImages().size()));

        // 12. Renderer Context
        ctx = std::make_unique<RendererContext>(*logicalDevice, *swapChain, *commandBuffers, *syncObjects,
                                                *renderPass, *graphicsPipeline);

        // 13. Render
        frameRenderer = std::make_unique<FrameRenderer>(*ctx, *this);
    }

    void VulkanRenderer::mainLoop()
    {
        while (!window.shouldClose())
        {
            window.pollEvents();
            maybeRecreateSwapchain();
            if (window.getWidth() == 0 || window.getHeight() == 0)
                continue;
            frameRenderer->drawFrame();
        }
    }

    void VulkanRenderer::cleanup()
    {
        if (logicalDevice)
        {
            VK_CHECK(vkDeviceWaitIdle(logicalDevice->getDevice()));
        }

        frameRenderer.reset();
        ctx.reset();
        commandBuffers.reset();
        framebuffers.reset();
        imageViews.reset();
        graphicsPipeline.reset();
        renderPass.reset();
        syncObjects.reset();
        commandPool.reset();
        swapChain.reset();
        logicalDevice.reset();
        physicalDevice.reset();
        surface.reset();
        instance.reset();

        Logger::log(LogLevel::INFO, "VulkanRenderer shutting down");
    }

    void VulkanRenderer::run()
    {
        mainLoop();
    }

    void VulkanRenderer::recreateSwapChain()
    {
        // 0) Ignore resize while minimized (width/height == 0)
        int w = window.getWidth();
        int h = window.getHeight();
        if (w == 0 || h == 0)
            return;

        // 1) Wait for GPU idle before touching resources
        VK_CHECK(vkDeviceWaitIdle(logicalDevice->getDevice()));

        // 2) Drop objects that depend on swapchain (order matters!)
        frameRenderer.reset(); // first, release anything referencing old objects
        commandBuffers.reset();
        framebuffers.reset();
        imageViews.reset();
        graphicsPipeline.reset();
        renderPass.reset();

        // 3) Recreate swapchain internals
        swapChain->cleanup();
        swapChain->create();

        const uint32_t newImageCount = static_cast<uint32_t>(swapChain->getImages().size());
        if (newImageCount != syncObjects->getImageCount())
        {
            // Recreate per-image semaphores/fences if count changed
            syncObjects = std::make_unique<SyncObjects>(logicalDevice->getDevice(), newImageCount);
        }

        // 4) Recreate dependent objects
        renderPass = std::make_unique<RenderPass>(*logicalDevice, *swapChain);
        graphicsPipeline = std::make_unique<GraphicsPipeline>(*logicalDevice, *swapChain, *renderPass);

        imageViews = std::make_unique<ImageViews>(
            logicalDevice->getDevice(),
            swapChain->getImages(),
            swapChain->getImageFormat());
        imageViews->create();

        framebuffers = std::make_unique<Framebuffers>(
            *logicalDevice, *renderPass, *swapChain, *imageViews);
        framebuffers->create();

        commandBuffers = std::make_unique<CommandBuffers>(
            logicalDevice->getDevice(), *commandPool,
            framebuffers->getFramebuffers().size());

        // 5) Record new command buffers
        for (uint32_t i = 0; i < swapChain->getImages().size(); ++i)
        {
            commandBuffers->record(i, *renderPass, *framebuffers, *graphicsPipeline, *swapChain);
        }

        // 6) Recreate renderer context (it holds references to recreated objects)
        ctx = std::make_unique<RendererContext>(
            *logicalDevice,
            *swapChain,
            *commandBuffers,
            *syncObjects, // important: if syncObjects was recreated above, here comes new one
            *renderPass,
            *graphicsPipeline);

        // 7) Recreate frame renderer using fresh context
        frameRenderer = std::make_unique<FrameRenderer>(*ctx, *this);

        Logger::log(LogLevel::INFO, "SwapChain and dependent resources recreated");
    }
}

void Core::VulkanRenderer::maybeRecreateSwapchain()
{
    if (!swapchainDirty)
        return;

    // Don't recreate while minimized — keep the dirty flag set
    if (window.getWidth() == 0 || window.getHeight() == 0)
        return;

    VK_CHECK(vkDeviceWaitIdle(logicalDevice->getDevice()));
    recreateSwapChain();
    swapchainDirty = false;
}
