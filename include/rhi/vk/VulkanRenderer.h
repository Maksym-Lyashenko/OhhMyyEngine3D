#pragma once

#include "platform/WindowManager.h"
#include "platform/guards/GLFWInitializer.h"

#include <memory>
#include <vector>

namespace Render
{
    class OrbitCamera;
}

namespace Vk
{

    namespace Gfx
    {
        class Texture2D;
    }

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
    class DepthResources;

    namespace Gfx
    {
        class Mesh;
    }

    /**
     * @brief High-level Vulkan application driver.
     *
     * Responsibilities:
     *  - Owns platform window and top-level Vulkan objects.
     *  - Initializes the rendering stack (instance → device → swapchain → pipeline).
     *  - Loads content (meshes), sets up camera & per-frame resources (RendererContext).
     *  - Runs the main loop and delegates per-frame work to FrameRenderer.
     *  - Handles swapchain recreation on window resize/minimize/format change.
     *
     * Lifecycle:
     *  ctor → init() → run() [mainLoop()] → cleanup() → dtor
     */
    class VulkanRenderer
    {
    public:
        VulkanRenderer();
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer &) = delete;
        VulkanRenderer &operator=(const VulkanRenderer &) = delete;

        /// Enter the main loop (poll events, render frames, handle resize).
        void run();

        /// Recreate swapchain and all dependent resources (safe to call when needed).
        void recreateSwapChain();

        /// External hook for GLFW callback (not strictly needed with markSwapchainDirty()).
        void setFramebufferResized(bool resized) { framebufferResized = resized; }

        /// Mark swapchain as dirty (resize/surface change). Actual recreation is deferred.
        void markSwapchainDirty() { swapchainDirty = true; }

        /// If swapchain is dirty and window is not minimized, recreate it now.
        void maybeRecreateSwapchain();

    private:
        // ---- Platform guards / window ----
        /// RAII guard for glfwInit()/glfwTerminate().
        Platform::Guards::GLFWInitializer glfwInitGuard;

        /// Window wrapper (GLFW, Vulkan-compatible).
        Platform::WindowManager window;

        // ---- Core Vulkan objects (creation order matters) ----
        std::unique_ptr<VulkanInstance> instance;             // VkInstance + validation/extensions
        std::unique_ptr<Surface> surface;                     // VkSurfaceKHR (from GLFW window)
        std::unique_ptr<VulkanPhysicalDevice> physicalDevice; // Chosen GPU + queue families
        std::unique_ptr<VulkanLogicalDevice> logicalDevice;   // VkDevice + queues

        // ---- Swapchain domain ----
        std::unique_ptr<SwapChain> swapChain;       // Images + format + extent
        std::unique_ptr<ImageViews> imageViews;     // Per-image views
        std::unique_ptr<DepthResources> depth;      // Depth image/view/format
        std::unique_ptr<RenderPass> renderPass;     // Color+depth attachments/subpasses
        std::unique_ptr<Framebuffers> framebuffers; // One framebuffer per swapchain image

        // ---- GPU pipeline & command subsystem ----
        std::unique_ptr<GraphicsPipeline> graphicsPipeline; // VS/FS, fixed states, layout (UBO+PC)
        std::unique_ptr<CommandPool> commandPool;           // Graphics command pool
        std::unique_ptr<CommandBuffers> commandBuffers;     // One primary CB per swapchain image
        std::unique_ptr<SyncObjects> syncObjects;           // Semaphores/fences per frame

        // ---- Render orchestration ----
        std::unique_ptr<RendererContext> ctx;         // Per-image UBOs, descriptor sets, shared refs
        std::unique_ptr<FrameRenderer> frameRenderer; // Acquire → submit → present per frame

        // ---- Content ----
        std::vector<std::unique_ptr<Gfx::Mesh>> gpuMeshes; // Owned GPU meshes
        std::vector<const Gfx::Mesh *> drawList;           // Non-owning pointers to meshes to draw

        // ---- Camera ----
        std::unique_ptr<Render::OrbitCamera> orbitCamera; // Simple orbit camera for first view

        std::unique_ptr<Gfx::Texture2D> albedoTex; // TODO:TEMPORARY

        // ---- State flags ----
        bool framebufferResized = false; // Legacy flag (can be driven by GLFW callback)
        bool swapchainDirty = false;     // Set on resize/surface invalidation, checked in maybeRecreateSwapchain()

        // ---- Lifecycle helpers ----
        /// Create all resources in the correct order, load content, record command buffers.
        void init();

        /// Poll events, optionally recreate swapchain, and render frames until window closes.
        void mainLoop();

        /// Destroy resources in reverse order; waits for device idle when safe.
        void cleanup();
    };

} // namespace Vk
