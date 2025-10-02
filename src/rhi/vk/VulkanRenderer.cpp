#include "rhi/vk/VulkanRenderer.h"

#include "core/Logger.h"

#include "rhi/vk/VulkanInstance.h"
#include "rhi/vk/Surface.h"
#include "rhi/vk/VulkanPhysicalDevice.h"
#include "rhi/vk/VulkanLogicalDevice.h"
#include "rhi/vk/SwapChain.h"
#include "rhi/vk/ImageViews.h"
#include "rhi/vk/RenderPass.h"
#include "rhi/vk/Framebuffers.h"
#include "rhi/vk/GraphicsPipeline.h"
#include "rhi/vk/CommandPool.h"
#include "rhi/vk/CommandBuffers.h"
#include "rhi/vk/SyncObjects.h"
#include "rhi/vk/RendererContext.h"
#include "rhi/vk/FrameRenderer.h"
#include <rhi/vk/Common.h>
#include "rhi/vk/DepthResources.h"

#include "rhi/vk/gfx/Mesh.h"
#include "rhi/vk/gfx/Vertex.h"
#include "render/OrbitCamera.h"
#include "render/ViewUniforms.h"
#include "asset/io/GltfLoader.h"

#include "core/math/MathUtils.h"
#include "rhi/vk/gfx/utils/MeshUtils.h"

using namespace Core;
using namespace Core::MathUtils;
using Vk::Gfx::Utils::computeWorldAABB;

#include <glm/glm.hpp>

namespace Vk
{
    VulkanRenderer::VulkanRenderer()
        : glfwInitGuard(), window(800, 600, "OhhMyyEngine3D")
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

        // 6. Command Pool
        commandPool = std::make_unique<CommandPool>(logicalDevice->getDevice(),
                                                    physicalDevice->getQueueFamilies().graphicsFamily.value());

        // 7. Depth
        depth = std::make_unique<DepthResources>();
        depth->create(physicalDevice->getDevice(),
                      logicalDevice->getDevice(),
                      swapChain->getExtent(),
                      commandPool->get(),
                      logicalDevice->getGraphicsQueue());

        // 8. Render Pass
        renderPass = std::make_unique<RenderPass>(*logicalDevice, *swapChain, depth->getFormat());

        // 9. Graphics papiline
        graphicsPipeline = std::make_unique<GraphicsPipeline>(*logicalDevice, *swapChain, *renderPass);

        // 10. Image Views
        imageViews = std::make_unique<ImageViews>(
            logicalDevice->getDevice(),
            swapChain->getImages(),
            swapChain->getImageFormat());

        imageViews->create();

        // 11. Framebuffers (need: device, render pass, extent, image views)
        framebuffers = std::make_unique<Framebuffers>(
            *logicalDevice,
            *renderPass,
            *swapChain,
            *imageViews,
            depth->getView());
        framebuffers->create();

        // 12. Command Buffers

        commandBuffers = std::make_unique<CommandBuffers>(logicalDevice->getDevice(),
                                                          *commandPool,
                                                          framebuffers->getFramebuffers().size());

        // 13. Sync objects
        syncObjects = std::make_unique<SyncObjects>(logicalDevice->getDevice(),
                                                    static_cast<uint32_t>(swapChain->getImages().size()));

        // Load glTF and create Mesh

        // --- Loading glTF and create Mesh ---
        VK_CHECK(vkDeviceWaitIdle(logicalDevice->getDevice()));
        gpuMeshes.clear();
        drawList.clear();

        auto meshDatas = Asset::GltfLoader::loadMeshes("assets/ford_mustang_1965/scene.gltf");
        gpuMeshes.reserve(meshDatas.size());
        drawList.reserve(meshDatas.size());

        for (auto &md : meshDatas)
        {
            std::vector<Gfx::Vertex> vertices;
            vertices.reserve(md.positions.size() / 3);
            for (size_t i = 0; i < md.positions.size(); i += 3)
            {
                Gfx::Vertex v{};
                v.pos = {md.positions[i + 0], md.positions[i + 1], md.positions[i + 2]};
                if (md.normals.size() == md.positions.size())
                {
                    v.normal = {md.normals[i + 0], md.normals[i + 1], md.normals[i + 2]};
                }
                else
                {
                    // на всякий случай (если нормалей нет) — единичная вверх
                    v.normal = {0.0f, 1.0f, 0.0f};
                }
                vertices.push_back(v);
            }

            auto m = std::make_unique<Gfx::Mesh>();
            m->create(physicalDevice->getDevice(), logicalDevice->getDevice(),
                      vertices, md.indices, md.localTransform);

            drawList.push_back(m.get());
            gpuMeshes.push_back(std::move(m));
        }

        // 13. Renderer Context
        ctx = std::make_unique<RendererContext>(*logicalDevice, *swapChain, *commandBuffers, *syncObjects,
                                                *renderPass, *graphicsPipeline);
        ctx->drawList = drawList;

        const AABB box = computeWorldAABB(ctx->drawList);

        if (!orbitCamera)
        {
            orbitCamera = std::make_unique<Render::OrbitCamera>();
        }

        orbitCamera->setAzimuth(225.0f);
        orbitCamera->setElevation(-20.0f);
        orbitCamera->setRoll(-2.5f);

        float aspect = swapChain->getExtent().width / (float)swapChain->getExtent().height;
        orbitCamera->frameToBox(box.min, box.max, /*fovY_deg=*/32.0f, aspect, /*pad=*/1.05f, /*lift=*/0.06f);

        // Record Command Buffers
        for (uint32_t i = 0; i < swapChain->getImages().size(); ++i)
        {
            Render::ViewUniforms u{};
            u.view = orbitCamera->view();
            u.proj = orbitCamera->proj();
            u.viewProj = u.proj * u.view;
            u.cameraPos = orbitCamera->position();
            commandBuffers->record(i, *renderPass, *framebuffers, *graphicsPipeline, *swapChain, ctx->drawList, u);
        }

        // 14. Render
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
        gpuMeshes.clear();
        framebuffers.reset();
        imageViews.reset();
        graphicsPipeline.reset();
        renderPass.reset();
        syncObjects.reset();
        depth.reset();
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
        depth.reset();

        // 3) Recreate swapchain internals
        swapChain->cleanup();
        swapChain->create();

        orbitCamera->setAspect(swapChain->getExtent().width / float(swapChain->getExtent().height));

        // recreate depth
        depth = std::make_unique<DepthResources>();
        depth->create(physicalDevice->getDevice(), logicalDevice->getDevice(),
                      swapChain->getExtent(), commandPool->get(), logicalDevice->getGraphicsQueue());

        const uint32_t newImageCount = static_cast<uint32_t>(swapChain->getImages().size());
        if (newImageCount != syncObjects->getImageCount())
        {
            // Recreate per-image semaphores/fences if count changed
            syncObjects = std::make_unique<SyncObjects>(logicalDevice->getDevice(), newImageCount);
        }

        // 4) Recreate dependent objects
        renderPass = std::make_unique<RenderPass>(*logicalDevice, *swapChain, depth->getFormat());
        graphicsPipeline = std::make_unique<GraphicsPipeline>(*logicalDevice, *swapChain, *renderPass);

        imageViews = std::make_unique<ImageViews>(
            logicalDevice->getDevice(),
            swapChain->getImages(),
            swapChain->getImageFormat());
        imageViews->create();

        framebuffers = std::make_unique<Framebuffers>(
            *logicalDevice, *renderPass, *swapChain, *imageViews, depth->getView());
        framebuffers->create();

        commandBuffers = std::make_unique<CommandBuffers>(
            logicalDevice->getDevice(), *commandPool,
            framebuffers->getFramebuffers().size());

        // 5) Recreate renderer context (it holds references to recreated objects)
        ctx = std::make_unique<RendererContext>(
            *logicalDevice,
            *swapChain,
            *commandBuffers,
            *syncObjects, // important: if syncObjects was recreated above, here comes new one
            *renderPass,
            *graphicsPipeline);

        ctx->drawList = drawList;

        // 6) Record new command buffers
        for (uint32_t i = 0; i < swapChain->getImages().size(); ++i)
        {
            Render::ViewUniforms u{};
            u.view = orbitCamera->view();
            u.proj = orbitCamera->proj();
            u.viewProj = u.proj * u.view;
            u.cameraPos = orbitCamera->position();
            commandBuffers->record(i, *renderPass, *framebuffers, *graphicsPipeline, *swapChain, ctx->drawList, u);
        }

        // 7) Recreate frame renderer using fresh context
        frameRenderer = std::make_unique<FrameRenderer>(*ctx, *this);

        Logger::log(LogLevel::INFO, "SwapChain and dependent resources recreated");
    }

    void Vk::VulkanRenderer::maybeRecreateSwapchain()
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
} // namespace Vk
