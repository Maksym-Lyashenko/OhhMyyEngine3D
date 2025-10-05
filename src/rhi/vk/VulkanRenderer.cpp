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

        CORE_LOG_DEBUG(
            "Mesh optimize: vertices " + std::to_string(before.vertices) + " -> " + std::to_string(after.vertices) +
            ", indices " + std::to_string(before.indices) + " -> " + std::to_string(after.indices) +
            ", tris " + std::to_string(before.triangles()) + " -> " + std::to_string(after.triangles()));

        gpuMeshes.reserve(meshDatas.size());
        drawList.reserve(meshDatas.size());

        for (auto &md : meshDatas)
        {
            std::vector<Gfx::Vertex> vertices;
            vertices.reserve(md.positions.size() / 3);

            const size_t vertCount = md.positions.size() / 3;
            const bool hasNormals = (md.normals.size() == md.positions.size());
            const bool hasUVs = (md.texcoords.size() / 2 == vertCount);
            const bool hasTangents = (md.tangents.size() == vertCount * 4); // glTF tangent vec4 per-vertex

            for (size_t i = 0; i < vertCount; ++i)
            {
                Gfx::Vertex v{};

                // position
                v.pos = {
                    md.positions[i * 3 + 0],
                    md.positions[i * 3 + 1],
                    md.positions[i * 3 + 2]};

                // normal (fallback to up if missing)
                if (hasNormals)
                {
                    v.normal = {
                        md.normals[i * 3 + 0],
                        md.normals[i * 3 + 1],
                        md.normals[i * 3 + 2]};
                }
                else
                {
                    v.normal = {0.0f, 1.0f, 0.0f};
                }

                // uv (fallback to 0,0)
                if (hasUVs)
                {
                    v.uv = {
                        md.texcoords[i * 2 + 0],
                        md.texcoords[i * 2 + 1]};
                }
                else
                {
                    v.uv = {0.0f, 0.0f};
                }

                // tangent (xyz = tangent, w = bitangent sign). If missing, build a cheap safe fallback.
                if (hasTangents)
                {
                    v.tangent = {
                        md.tangents[i * 4 + 0],
                        md.tangents[i * 4 + 1],
                        md.tangents[i * 4 + 2],
                        md.tangents[i * 4 + 3] // +1 or -1
                    };
                }
                else
                {
                    // --- Fallback tangent generation (very cheap, per-vertex, not per-triangle accurate) ---
                    // Pick some axis not parallel to the normal, build a tangent orthonormal to the normal.
                    // This avoids NaNs and makes the normal map behave like a flat normal (mostly).
                    glm::vec3 n = glm::normalize(v.normal);
                    glm::vec3 arbitrary = (std::abs(n.y) < 0.999f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
                    glm::vec3 t = glm::normalize(arbitrary - n * glm::dot(n, arbitrary));
                    v.tangent = glm::vec4(t, 1.0f); // assume positive bitangent sign
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

        // --- Materials (create one albedo-only material for the whole scene) -------------
        Render::MaterialDesc md{};
        md.baseColorPath = "assets/makarov/textures/makarov_baseColor.png"; // TODO: read from glTF
        md.normalPath = "assets/makarov/textures/makarov_normal.png";
        md.mrPath = "assets/makarov/textures/makarov_metallicRoughness.png";

        auto mat = materials->createMaterial(md); // shared_ptr<IMaterial>

        sceneMaterials.clear();
        sceneMaterials.push_back(mat);

        // Build drawItems from meshes + single material
        drawItems.clear();
        drawItems.reserve(gpuMeshes.size());
        for (auto &m : gpuMeshes)
        {
            drawItems.push_back(Vk::Gfx::DrawItem{
                /*mesh*/ m.get(),
                /*material*/ mat.get() // non-owning
            });
        }

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
                                   *swapChain, drawItems, ctx->viewSet(i));
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

        sceneMaterials.clear();
        materials.reset();

        if (ctx)
        {
            // owns per-image UBOs/descriptor sets
            ctx->destroyViewResources();
            ctx.reset();
        }

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
                                   *swapChain, drawItems, ctx->viewSet(i));
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
