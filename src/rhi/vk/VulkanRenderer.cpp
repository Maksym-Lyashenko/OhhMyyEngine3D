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
#include "rhi/vk/DepthResources.h"
#include "rhi/vk/Common.h"

#include "rhi/vk/gfx/Mesh.h"
#include "rhi/vk/gfx/Vertex.h"
#include "rhi/vk/gfx/Texture2D.h"

#include "render/OrbitCamera.h"
#include "render/ViewUniforms.h"

#include "asset/io/GltfLoader.h"
#include "asset/processing/MeshOptimize.h"

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

    VulkanRenderer::~VulkanRenderer() { cleanup(); }

    void VulkanRenderer::init()
    {
        Logger::log(LogLevel::INFO, "VulkanRenderer initialized");

        // Defer swapchain recreation to the beginning of a frame
        window.onFramebufferResize = [&](int /*w*/, int /*h*/)
        { markSwapchainDirty(); };

        // --- Device & surface chain ------------------------------------------------
        instance = std::make_unique<VulkanInstance>(window, /*validation*/ true);
        surface = std::make_unique<Surface>(instance->getInstance(), window);
        surface->create();
        physicalDevice = std::make_unique<VulkanPhysicalDevice>(instance->getInstance(), *surface);
        logicalDevice = std::make_unique<VulkanLogicalDevice>(*physicalDevice);

        // --- Swapchain & GPU-local targets ----------------------------------------
        swapChain = std::make_unique<SwapChain>(*physicalDevice, *logicalDevice, *surface, window);
        swapChain->create();

        // Command pool tied to graphics queue family (primary CBs)
        commandPool = std::make_unique<CommandPool>(logicalDevice->getDevice(),
                                                    physicalDevice->getQueueFamilies().graphicsFamily.value());

        // Depth buffer (image + memory + view) + layout transition
        depth = std::make_unique<DepthResources>();
        depth->create(physicalDevice->getDevice(), logicalDevice->getDevice(),
                      swapChain->getExtent(), commandPool->get(), logicalDevice->getGraphicsQueue());

        // Render pass that matches color+depth attachments
        renderPass = std::make_unique<RenderPass>(*logicalDevice, *swapChain, depth->getFormat());

        // Graphics pipeline (no baked viewport/scissor — dynamic; layout has set=0 View UBO + PC for model)
        graphicsPipeline = std::make_unique<GraphicsPipeline>(*logicalDevice, *renderPass);

        // Per-swapchain-image color views
        imageViews = std::make_unique<ImageViews>(logicalDevice->getDevice(),
                                                  swapChain->getImages(), swapChain->getImageFormat());
        imageViews->create();

        // One framebuffer per swapchain image + shared depth
        framebuffers = std::make_unique<Framebuffers>(*logicalDevice, *renderPass,
                                                      *swapChain, *imageViews, depth->getView());
        framebuffers->create();

        // One primary command buffer per swapchain image
        commandBuffers = std::make_unique<CommandBuffers>(logicalDevice->getDevice(),
                                                          *commandPool, framebuffers->getFramebuffers().size());

        // Sync: per-frame fences + per-image present semaphores
        syncObjects = std::make_unique<SyncObjects>(logicalDevice->getDevice(),
                                                    static_cast<uint32_t>(swapChain->getImages().size()));

        // --- Assets → GPU meshes ---------------------------------------------------
        // Make sure nothing is executing while we (re)upload meshes
        VK_CHECK(vkDeviceWaitIdle(logicalDevice->getDevice()));
        gpuMeshes.clear();
        drawList.clear();

        // TODO: replace hardcoded path with asset manager/root config
        auto meshDatas = Asset::GltfLoader::loadMeshes("assets/makarov/scene.gltf");

        // ---- Optimize meshes (meshoptimizer) & collect stats ----
        struct MeshStats
        {
            size_t vertices{};
            size_t indices{};
            size_t triangles() const { return indices / 3; }
        };

        auto collectStats = [](const std::vector<Asset::MeshData> &mds)
        {
            MeshStats s{};
            for (auto &m : mds)
            {
                s.vertices += m.positions.size() / 3;
                s.indices += m.indices.size();
            }
            return s;
        };

        MeshStats before = collectStats(meshDatas);

        // Tune settings if needed
        Asset::Processing::OptimizeSettings opt{};
        opt.optimizeCache = true;
        opt.optimizeOverdraw = true;
        opt.overdrawThreshold = 1.05f;
        opt.optimizeFetch = true;
        opt.simplify = true;            // включить LOD-упрощение
        opt.simplifyTargetRatio = 0.6f; // до 60% от исходных индексов
        opt.simplifyError = 1e-2f;

        for (auto &md : meshDatas)
        {
            Asset::Processing::OptimizeMeshInPlace(md, opt);
        }

        MeshStats after = collectStats(meshDatas);

        CORE_LOG_INFO(
            "Mesh optimize: vertices " + std::to_string(before.vertices) + " -> " + std::to_string(after.vertices) +
            ", indices " + std::to_string(before.indices) + " -> " + std::to_string(after.indices) +
            ", tris " + std::to_string(before.triangles()) + " -> " + std::to_string(after.triangles()));

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
                    // Fallback if normals are missing
                    v.normal = {0.0f, 1.0f, 0.0f};
                }
                if (md.texcoords.size() / 2 == md.positions.size() / 3)
                {
                    v.uv = {md.texcoords[(i / 3) * 2 + 0],
                            md.texcoords[(i / 3) * 2 + 1]};
                }
                else
                {
                    v.uv = {0.0f, 0.0f};
                }

                vertices.push_back(v);
            }

            auto m = std::make_unique<Gfx::Mesh>();
            m->create(physicalDevice->getDevice(), logicalDevice->getDevice(),
                      vertices, md.indices, md.localTransform);

            drawList.push_back(m.get());
            gpuMeshes.push_back(std::move(m));
        }

        // --- Renderer context + per-image View UBO/sets ---------------------------
        ctx = std::make_unique<RendererContext>(*logicalDevice, *swapChain, *commandBuffers,
                                                *syncObjects, *renderPass, *graphicsPipeline);
        ctx->drawList = drawList;

        // Allocates UBO buffers and descriptor sets (set=0)
        ctx->createViewResources(physicalDevice->getDevice());

        // --- Texture (TEMP) -----------------------------------------------------------
        albedoTex = std::make_unique<Gfx::Texture2D>();
        albedoTex->loadFromFile(
            physicalDevice->getDevice(),
            logicalDevice->getDevice(),
            commandPool->get(),
            logicalDevice->getGraphicsQueue(),
            "assets/makarov/textures/makarov_baseColor.png", // TODO: взять из материала glTF
            /*genMips*/ true,
            /*fmt*/ VK_FORMAT_R8G8B8A8_SRGB);

        // (TEMP) One material set for the whole scene
        ctx->createMaterialSet(albedoTex->view(), albedoTex->sampler(),
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // --- Camera fitting --------------------------------------------------------
        const AABB box = computeWorldAABB(ctx->drawList);

        if (!orbitCamera)
        {
            orbitCamera = std::make_unique<Render::OrbitCamera>();
        }
        orbitCamera->setAzimuth(225.0f);
        orbitCamera->setElevation(-20.0f);
        orbitCamera->setRoll(-2.5f);

        const float aspect = swapChain->getExtent().width / float(swapChain->getExtent().height);
        orbitCamera->frameToBox(box.min, box.max, /*fovY_deg=*/32.0f, aspect,
                                /*pad=*/1.05f, /*lift=*/0.06f);

        // --- Record command buffers ----------------------------------------------
        for (uint32_t i = 0; i < swapChain->getImages().size(); ++i)
        {
            // Update per-image View UBO
            Render::ViewUniforms u{};
            u.view = orbitCamera->view();
            u.proj = orbitCamera->proj();
            u.viewProj = u.proj * u.view;
            u.cameraPos = glm::vec4(orbitCamera->position(), 1.0f); // std140-friendly

            ctx->updateViewUbo(i, u);

            // Record for image i: pass descriptor set (set=0)
            commandBuffers->record(i, *renderPass, *framebuffers, *graphicsPipeline,
                                   *swapChain, ctx->drawList, ctx->viewSet(i), ctx->getMaterialSet());
        }

        // --- Frame loop driver -----------------------------------------------------
        frameRenderer = std::make_unique<FrameRenderer>(*ctx, *this);
    }

    void VulkanRenderer::mainLoop()
    {
        while (!window.shouldClose())
        {
            window.pollEvents();
            maybeRecreateSwapchain();
            if (window.width() == 0 || window.height() == 0) // minimized
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

        if (ctx)
        {
            // owns per-image UBOs/descriptor sets
            ctx->destroyViewResources();
            ctx.reset();
        }

        albedoTex.reset();

        commandBuffers.reset();
        gpuMeshes.clear(); // destroy VBO/IBO via Mesh destructors
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

    void VulkanRenderer::run() { mainLoop(); }

    void VulkanRenderer::recreateSwapChain()
    {
        // 0) Skip while minimized
        if (window.width() == 0 || window.height() == 0)
            return;

        // 1) Be safe
        VK_CHECK(vkDeviceWaitIdle(logicalDevice->getDevice()));

        // 2) Destroy/Reset everything that depends on the swapchain (order matters)
        frameRenderer.reset();
        commandBuffers.reset();

        if (ctx)
        {
            // Drop UBOs/sets that referenced old swapchain images/layouts
            ctx->destroyViewResources();
        }

        framebuffers.reset();
        imageViews.reset();
        graphicsPipeline.reset();
        renderPass.reset();
        depth.reset();

        // 3) Recreate swapchain and depth
        swapChain->cleanup();
        swapChain->create();

        orbitCamera->setAspect(swapChain->getExtent().width / float(swapChain->getExtent().height));

        depth = std::make_unique<DepthResources>();
        depth->create(physicalDevice->getDevice(), logicalDevice->getDevice(),
                      swapChain->getExtent(), commandPool->get(), logicalDevice->getGraphicsQueue());

        const uint32_t newImageCount = static_cast<uint32_t>(swapChain->getImages().size());
        if (newImageCount != syncObjects->getImageCount())
        {
            // Recreate per-image semaphores/fences only if the count changed
            syncObjects = std::make_unique<SyncObjects>(logicalDevice->getDevice(), newImageCount);
        }

        // 4) Recreate dependent objects
        renderPass = std::make_unique<RenderPass>(*logicalDevice, *swapChain, depth->getFormat());
        graphicsPipeline = std::make_unique<GraphicsPipeline>(*logicalDevice, *renderPass);

        imageViews = std::make_unique<ImageViews>(logicalDevice->getDevice(),
                                                  swapChain->getImages(), swapChain->getImageFormat());
        imageViews->create();

        framebuffers = std::make_unique<Framebuffers>(*logicalDevice, *renderPass,
                                                      *swapChain, *imageViews, depth->getView());
        framebuffers->create();

        commandBuffers = std::make_unique<CommandBuffers>(logicalDevice->getDevice(),
                                                          *commandPool, framebuffers->getFramebuffers().size());

        // 5) Recreate renderer context (pipeline/layout might have changed)
        ctx = std::make_unique<RendererContext>(*logicalDevice, *swapChain, *commandBuffers,
                                                *syncObjects, *renderPass, *graphicsPipeline);
        ctx->drawList = drawList;
        ctx->createViewResources(physicalDevice->getDevice());

        ctx->createMaterialSet(albedoTex->view(), albedoTex->sampler(),
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // 6) Record new command buffers (and refresh UBOs)
        for (uint32_t i = 0; i < swapChain->getImages().size(); ++i)
        {
            Render::ViewUniforms u{};
            u.view = orbitCamera->view();
            u.proj = orbitCamera->proj();
            u.viewProj = u.proj * u.view;
            u.cameraPos = glm::vec4(orbitCamera->position(), 1.0f);

            ctx->updateViewUbo(i, u);

            commandBuffers->record(i, *renderPass, *framebuffers, *graphicsPipeline,
                                   *swapChain, ctx->drawList, ctx->viewSet(i), ctx->getMaterialSet());
        }

        // 7) Recreate driver
        frameRenderer = std::make_unique<FrameRenderer>(*ctx, *this);

        Logger::log(LogLevel::INFO, "SwapChain and dependent resources recreated");
    }

    void VulkanRenderer::maybeRecreateSwapchain()
    {
        if (!swapchainDirty)
            return;
        if (window.width() == 0 || window.height() == 0)
            return; // keep dirty flag set

        VK_CHECK(vkDeviceWaitIdle(logicalDevice->getDevice()));
        recreateSwapChain();
        swapchainDirty = false;
    }

} // namespace Vk
