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

#include "render/OrbitCamera.h"
#include "render/FreeCamera.h"
#include "render/Scene.h"
#include "render/ViewUniforms.h"
#include "render/materials/Material.h"
#include "render/materials/MaterialSystem.h"

#include "asset/io/GltfLoader.h"
#include "asset/processing/MeshOptimize.h"

#include "core/math/MathUtils.h"
#include "rhi/vk/gfx/utils/MeshUtils.h"
#include "rhi/vk/gfx/DrawItem.h"

using namespace Core;
using namespace Core::MathUtils;
using Vk::Gfx::Utils::computeWorldAABB;

#include <glm/glm.hpp>
#include <chrono>

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

        // Materials
        materials = std::make_unique<Render::MaterialSystem>();
        // TEMP: Forwarding one-time commands for texture upload
        materials->setUploadCmd(commandPool->get(), logicalDevice->getGraphicsQueue()); // TEMP
        materials->init(physicalDevice->getDevice(),
                        logicalDevice->getDevice(),
                        graphicsPipeline->getMaterialSetLayout(),
                        /*maxMaterials*/ 128);

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

        // --- Materials system (we still create it here, because it depends on VkDevice etc.) -----------------
        materials = std::make_unique<Render::MaterialSystem>();
        materials->setUploadCmd(commandPool->get(), logicalDevice->getGraphicsQueue());
        materials->init(
            physicalDevice->getDevice(),
            logicalDevice->getDevice(),
            graphicsPipeline->getMaterialSetLayout(),
            /*maxMaterials*/ 128);

        // --- Create Scene and load content ------------------------------------
        scene = std::make_unique<Render::Scene>();

        scene->loadModel(
            "assets/makarov/scene.gltf",
            physicalDevice->getDevice(),
            logicalDevice->getDevice(),
            *materials);

        // scene now contains:
        // - gpu meshes
        // - materials
        // - draw items
        // - world bounds

        // --- Renderer context + per-image View UBO/sets ---------------------------
        ctx = std::make_unique<RendererContext>(*logicalDevice, *swapChain, *commandBuffers,
                                                *syncObjects, *renderPass, *graphicsPipeline);
        // The RendererContext only needs the list of meshes to draw (of DrawItems).
        // Currently RendererContext::drawList was std::vector<const Mesh*>.
        // We can still give it the meshes indirectly via scene->drawItems().
        //
        // Option A: keep ctx->drawList == vector<const Mesh*> for now:
        {
            ctx->drawList.clear();
            ctx->drawList.reserve(scene->drawItems().size());
            for (auto &di : scene->drawItems())
            {
                ctx->drawList.push_back(di.mesh); // di.mesh is Mesh*
            }
        }

        // Allocates UBO buffers and descriptor sets (set=0)
        ctx->createViewResources(physicalDevice->getDevice());

        // --- Camera setup using Scene bounds --------------------------------------------------------
        const Core::MathUtils::AABB &box = scene->worldBounds();

        // for freeCamera
        glm::vec3 center = 0.5f * (box.min + box.max);
        glm::vec3 extents = (box.max - box.min) * 0.5f;
        float radius = glm::length(extents);

        // start distance for freeCamera
        float dist = radius * 2.0f;

        // put freeCamera a bit from top and under degree
        glm::vec3 eye = center + glm::vec3(dist * 0.5f, dist * 0.3f, dist * 1.0f);

        // create freeCamera
        if (!freeCamera)
        {
            freeCamera = std::make_unique<Render::FreeCamera>(
                eye,
                /*yawDeg   */ 0.0f,
                /*pitchDeg */ 0.0f,
                /*fovYDeg  */ 32.0f,
                /*aspect   */ swapChain->getExtent().width /
                    float(swapChain->getExtent().height),
                /*near*/ 0.05f,
                /*far */ 2000.0f);

            freeCamera->lookAt(eye, center);
        }
        else
        {
            freeCamera->setAspect(
                swapChain->getExtent().width /
                float(swapChain->getExtent().height));
        }

        if (freeCamera)
        {
            freeCamera->setAspect(
                swapChain->getExtent().width /
                float(swapChain->getExtent().height));
        }

        // orbitCamera is now optional, can keep or drop later
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
        // drawItems we now get from Scene
        const auto &drawItemsRef = scene->drawItems();

        for (uint32_t i = 0; i < swapChain->getImages().size(); ++i)
        {
            // Update per-image View UBO
            Render::ViewUniforms u{};
            u.view = freeCamera->view();
            u.proj = freeCamera->proj();
            u.viewProj = u.proj * u.view;
            u.cameraPos = glm::vec4(freeCamera->position(), 1.0f); // std140-friendly

            ctx->updateViewUbo(i, u);

            // Record for image i: pass descriptor set (set=0)
            commandBuffers->record(i, *renderPass, *framebuffers, *graphicsPipeline,
                                   *swapChain, drawItemsRef, ctx->viewSet(i));
        }

        // --- Frame loop driver -----------------------------------------------------
        frameRenderer = std::make_unique<FrameRenderer>(*ctx, *this);
    }

    void VulkanRenderer::mainLoop()
    {
        using clock = std::chrono::steady_clock;
        auto prev = clock::now();

        // for FPS statistic
        double fpsTimerAcc = 0.0;
        int fpsFrameAcc = 0;
        float smoothedFps = 0.0f;

        while (!window.shouldClose())
        {
            auto now = clock::now();
            float dt = std::chrono::duration<float>(now - prev).count();
            prev = now;
            if (dt > 0.1f)
                dt = 0.1f;

            // for FPS
            fpsTimerAcc += dt;
            fpsFrameAcc += 1;
            // once in 0.25sec update smooth FPS
            if (fpsFrameAcc >= 0.25)
            {
                smoothedFps = float(fpsFrameAcc / fpsTimerAcc);
                fpsTimerAcc = 0.0;
                fpsFrameAcc = 0;
            }

            window.pollEvents();

            if (window.wasKeyPressed(GLFW_KEY_F1))
            {
                // переключатель захвата мыши
                static bool cap = false;
                cap = !cap;
                window.captureMouse(cap);
            }

            if (window.isMouseDown(GLFW_MOUSE_BUTTON_RIGHT))
            {
                glm::vec2 md = window.mouseDelta();
                freeCamera->addYawPitch(md.x * 0.1f, -md.y * 0.1f);
            }

            glm::vec3 v(0);
            const float spd = 100.0f;
            if (window.isKeyDown(GLFW_KEY_W))
                v.z += spd * dt;
            if (window.isKeyDown(GLFW_KEY_S))
                v.z -= spd * dt;
            if (window.isKeyDown(GLFW_KEY_A))
                v.x -= spd * dt;
            if (window.isKeyDown(GLFW_KEY_D))
                v.x += spd * dt;
            if (window.isKeyDown(GLFW_KEY_E))
                v.y += spd * dt;
            if (window.isKeyDown(GLFW_KEY_Q))
                v.y -= spd * dt;
            freeCamera->moveLocal(v);

            Render::ViewUniforms u{};
            u.view = freeCamera->view();
            u.proj = freeCamera->proj();
            u.viewProj = u.proj * u.view;
            u.cameraPos = glm::vec4(freeCamera->position(), 1.0f);

            for (uint32_t i = 0; i < swapChain->getImages().size(); ++i)
            {
                ctx->updateViewUbo(i, u);
            }

            // --- DEBUG HEADER --- //
            {
                glm::vec3 p = freeCamera->position();

                char titleBuf[512];
                std::snprintf(
                    titleBuf,
                    sizeof(titleBuf),
                    "FPS %.1f | Cam (%.2f %.2f %.2f) yaw %.1f pitch %.1f | zN %.2f zF %.1f | %dx%d | %s",
                    smoothedFps,
                    p.x, p.y, p.z,
                    freeCamera->yawDeg(), freeCamera->pitchDeg(),
                    freeCamera->zNear(), freeCamera->zFar(),
                    window.width(), window.height(),
                    swapChain->presentModeName().c_str());

                window.setTitle(titleBuf);
            }

            maybeRecreateSwapchain();
            if (window.width() == 0 || window.height() == 0) // minimized
                continue;
            frameRenderer->drawFrame();
        }
    }

    void VulkanRenderer::cleanup()
    {
        // 1) Wait for device idle before destroing GPU resources.
        if (logicalDevice)
        {
            VK_CHECK(vkDeviceWaitIdle(logicalDevice->getDevice()));
        }

        // 2) Stop the frame thread / renderer driver (it may submit).
        frameRenderer.reset();

        // 3) Destroy scene and materials early - they own Mesh/Textures/VkBuffer/VkImage etc.
        // This ensures Mesh destructors free their Vulkan handles while device is valid.
        scene.reset();
        materials.reset();

        // 4) Destroy context-held resources (per-image UBO's, descriptor pools/sets).
        if (ctx)
        {
            // owns per-image UBOs/descriptor sets
            ctx->destroyViewResources();
            ctx.reset();
        }

        // 5) Command buffers, frame buffers, image views, pipelines, renderpass, depth, sync
        commandBuffers.reset(); // frees primary command buffers
        framebuffers.reset();
        imageViews.reset();
        graphicsPipeline.reset();
        renderPass.reset();
        depth.reset();

        // 6) Sync objects, command pool, swapchain
        syncObjects.reset();
        commandPool.reset();
        if (swapChain)
        {
            swapChain->cleanup(); // explicity cleanup swapchain internals
            swapChain.reset();
        }

        // 7) Destroy logical device last (after all children freed)
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

        freeCamera->setAspect(swapChain->getExtent().width / float(swapChain->getExtent().height));

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
        {
            ctx->drawList.clear();
            ctx->drawList.reserve(scene->drawItems().size());
            for (auto &di : scene->drawItems())
            {
                ctx->drawList.push_back(di.mesh); // di.mesh is Mesh*
            }
        }
        ctx->createViewResources(physicalDevice->getDevice());

        // 6) Record new command buffers (and refresh UBOs)
        const auto &drawItemsRef = scene->drawItems();
        for (uint32_t i = 0; i < swapChain->getImages().size(); ++i)
        {
            Render::ViewUniforms u{};
            u.view = freeCamera->view();
            u.proj = freeCamera->proj();
            u.viewProj = u.proj * u.view;
            u.cameraPos = glm::vec4(freeCamera->position(), 1.0f);

            ctx->updateViewUbo(i, u);

            commandBuffers->record(i, *renderPass, *framebuffers, *graphicsPipeline,
                                   *swapChain, drawItemsRef, ctx->viewSet(i));
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
