#pragma once

#include <vulkan/vulkan.h>
#include <imgui.h>

namespace Vk
{
    struct RendererContext;
    class VulkanAllocator;
}

namespace Platform
{
    class WindowManager;
}

namespace UI
{
    /**
     * @brief Manages Dear ImGui integration with Vulkan renderer.
     *
     * Responsibilities:
     *  - Initialize ImGui context and Vulkan backend.
     *  - Handle per-frame updates (new frame / render / cleanup).
     *  - Manage descriptor pool and font uploads.
     */

    class ImGuiLayer
    {
    public:
        ImGuiLayer(Vk::RendererContext &context, Platform::WindowManager &window);
        ~ImGuiLayer() noexcept;

        // Initializes ImGui + Vulkan integration (must be called after swapchain creation).
        void initialize();

        // Starts a new ImGui frame (call once per frame before any ImGui:: calls)
        void beginFrame();

        void endFrame();

        // Ends the frame and records ImGui draw commands into a Vulkan command buffer.
        void render(VkCommandBuffer cmd);

        void drawVmaPanel(Vk::VulkanAllocator &allocator);

        // Recreates ImGui Vulkan resources (e.g., on swapchain resize)
        void onSwapchainRecreate();

        // Shutdown and cleanup
        void shutdown() noexcept;

    private:
        void createDescriptorPool();

        Vk::RendererContext &context;
        Platform::WindowManager &windowManager;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        bool initialized = false;
    };

} // namespace UI
