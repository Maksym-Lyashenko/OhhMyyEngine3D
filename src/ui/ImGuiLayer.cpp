// Simple ImGui integration for Vulkan + GLFW.
// - Creates descriptor pool for ImGui
// - Initializes ImGui context + Vulkan backend
// - Uploads fonts using a one-shot command buffer
// - Provides beginFrame() / render(cmd) / onSwapchainRecreate() / shutdown()
//
// Note: This implementation expects:
//  - a Vulkan logical device accessible via renderer.logicalDevice (or through API below)
//  - a VkRenderPass compatible with the swapchain (we use the same render pass as your main pass)
//  - a valid GLFW window pointer for ImGui_ImplGlfw init

#include "ui/ImGuiLayer.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "rhi/vk/VulkanLogicalDevice.h"
#include "rhi/vk/RendererContext.h"
#include "platform/WindowManager.h"
#include "rhi/vk/DepthResources.h"
#include "rhi/vk/CommandPool.h"
#include "rhi/vk/memoryManager/VulkanAllocator.h"
#include "rhi/vk/Common.h" // VK_CHECK

#include <vector>
#include <stdexcept>
#include <array>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace UI
{

    // Helper: allocate a short-lived command buffer and submit (used for font upload)
    static VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue)
    {
        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = pool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &cmd));

        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &bi));
        return cmd;
    }

    static void endSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd)
    {
        VK_CHECK(vkEndCommandBuffer(cmd));
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(queue));
        vkFreeCommandBuffers(device, pool, 1, &cmd);
    }

    // Simple helper for ImGui Vulkan backend error checking
    static void check_vk_result(VkResult err)
    {
        if (err == VK_SUCCESS)
            return;
        fprintf(stderr, "[ImGui][Vulkan] Error: VkResult = %d\n", err);
        if (err < 0)
            abort();
    }

    // Constructor stores references; nothing heavy performed here.
    ImGuiLayer::ImGuiLayer(Vk::RendererContext &context_, Platform::WindowManager &window_)
        : context(context_), windowManager(window_), device(VK_NULL_HANDLE), descriptorPool(VK_NULL_HANDLE), initialized(false)
    {
        // We'll fetch device/queues when initialized()
        (void)window_; // stored in setupImGuiContext
    }

    // Destructor ensures shutdown if user forgot
    ImGuiLayer::~ImGuiLayer() noexcept
    {
        shutdown();
    }

    // Create Vulkan descriptor pool used by ImGui backend
    void ImGuiLayer::createDescriptorPool()
    {
        // Typical pool sizes recommended by ImGui examples
        std::vector<VkDescriptorPoolSize> poolSizes = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1000 * static_cast<uint32_t>(poolSizes.size());
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));
    }

    // Setup ImGui context and backends. Must be called after Vulkan device & swapchain exist.
    // renderPass is the render pass used for UI rendering (compatible with swapchain color format).

    void ImGuiLayer::initialize()
    {
        if (initialized)
            return;

        device = context.device.getDevice();
        VkPhysicalDevice phys = context.physDevice.getDevice();
        VkQueue graphicsQueue = context.device.getGraphicsQueue();
        VkCommandPool cmdPool = context.commandPool.get();

        // 1) Create ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.Fonts->AddFontDefault();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

        // 2) Setup style
        ImGui::StyleColorsDark();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGuiStyle &style = ImGui::GetStyle();
            style.WindowRounding = 0.0f;
        }

        // 3) Setup Platform/Renderer bindings
        ImGui_ImplGlfw_InitForVulkan(windowManager.getWindow(), true);

        // 4) Create descriptor pool for ImGui
        createDescriptorPool();

        // 5) Get formats from swapchain and depth
        VkFormat colorFormat = context.swapChain.getImageFormat();
        VkFormat depthFormat = context.depth.getFormat();

        // 6) Setup Vulkan backend for DYNAMIC RENDERING
        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.ApiVersion = VK_API_VERSION_1_4;
        init_info.Instance = context.instance.getInstance();
        init_info.PhysicalDevice = phys;
        init_info.Device = device;
        init_info.QueueFamily = context.device.getGraphicsQueueFamilyIndex();
        init_info.Queue = graphicsQueue;
        init_info.DescriptorPool = descriptorPool;
        init_info.MinImageCount = 2;
        init_info.ImageCount = static_cast<uint32_t>(context.swapChain.getImages().size());
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.Allocator = nullptr;
        init_info.CheckVkResultFn = check_vk_result;

        // Настройка динамического рендеринга
        init_info.UseDynamicRendering = true;

        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.PipelineInfoMain.Subpass = 0;

        init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
        init_info.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;
        init_info.PipelineInfoMain.PipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        init_info.PipelineInfoForViewports = init_info.PipelineInfoMain;

        // Инициализация ImGui Vulkan
        ImGui_ImplVulkan_Init(&init_info);

        initialized = true;
    }

    void ImGuiLayer::beginFrame()
    {
        if (!initialized)
            return;

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::render(VkCommandBuffer cmd)
    {
        if (!initialized)
            return;

        // Рендерим ImGui
        ImGui::Render();
        ImDrawData *draw_data = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
    }

    static const char *HeapFlagsToString(VkMemoryHeapFlags f)
    {
        // Для справки — сейчас нас больше интересует DEVICE_LOCAL, но оставим универсально.
        return (f & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? "DEVICE_LOCAL" : "";
    }

    void ImGuiLayer::drawVmaPanel(Vk::VulkanAllocator &allocator)
    {
        if (!ImGui::Begin("Memory / VMA"))
        {
            ImGui::End();
            return;
        }

        static bool detailedDump = true;

        // Controls
        ImGui::Checkbox("Detailed JSON map", &detailedDump);
        ImGui::SameLine();
        if (ImGui::Button("Dump VMA JSON"))
        {
            // timestamped filename
            auto now = std::time(nullptr);
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &now);
#else
            localtime_r(&now, &tm);
#endif
            std::ostringstream oss;
            oss << "vma_stats_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << (detailedDump ? "_detailed" : "") << ".json";
            allocator.dumpStatsToFile(oss.str(), detailedDump);
        }

        ImGui::Separator();

        // Budgets table
        std::vector<VmaBudget> budgets;
        VkPhysicalDeviceMemoryProperties memProps{};
        allocator.getBudgets(budgets, memProps);

        if (ImGui::BeginTable("VMAHeaps", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28.f);
            ImGui::TableSetupColumn("Flags");
            ImGui::TableSetupColumn("Heap Size (MB)");
            ImGui::TableSetupColumn("Budget (MB)");
            ImGui::TableSetupColumn("Usage (MB)");
            ImGui::TableSetupColumn("Usage %");
            ImGui::TableHeadersRow();

            for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i)
            {
                const auto &heap = memProps.memoryHeaps[i];
                const auto &b = budgets[i];

                const double heapMB = heap.size / (1024.0 * 1024.0);
                const double budMB = b.budget / (1024.0 * 1024.0);
                const double useMB = b.usage / (1024.0 * 1024.0);
                const double percent = (budMB > 0.0) ? (100.0 * useMB / budMB) : 0.0;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", i);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(HeapFlagsToString(heap.flags));
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.1f", heapMB);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.1f", budMB);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.1f", useMB);
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%.1f%%", percent);
            }

            ImGui::EndTable();
        }

        ImGui::End();
    }

    void ImGuiLayer::endFrame()
    {
        if (!initialized)
            return;

        // Если используете многопоточность, эта функция должна вызываться в основном потоке
        // после завершения рендеринга всех ImGui окон
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            // Эта функция уже вызывается в render(), но если нужно отдельное завершение фрейма
            // ImGui::UpdatePlatformWindows();
            // ImGui::RenderPlatformWindowsDefault();
        }
    }

    // Swapchain recreate: if render pass or image count changed, inform ImGui backend.
    void ImGuiLayer::onSwapchainRecreate()
    {
        if (!initialized)
            return;

        // Recreate ImGui descriptor pool? Not necessary in many cases.
        // We just need to call Init with new renderPass if it changed.
        ImGui_ImplVulkan_SetMinImageCount(static_cast<uint32_t>(context.swapChain.getImages().size()));
        // Re-init call expects the same init_info; easiest approach is to shutdown Vulkan backend and re-init:
        ImGui_ImplVulkan_Shutdown();

        // Recreate descriptor pool if you destroyed it on swapchain recreation; here we keep it.
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = context.instance.getInstance();
        init_info.PhysicalDevice = context.physDevice.getDevice();
        init_info.Device = device;
        init_info.QueueFamily = context.device.getGraphicsQueueFamilyIndex();
        init_info.Queue = context.device.getGraphicsQueue();
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = descriptorPool;
        init_info.MinImageCount = static_cast<uint32_t>(context.swapChain.getImages().size());
        init_info.ImageCount = static_cast<uint32_t>(context.swapChain.getImages().size());
        // init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = nullptr;
        init_info.CheckVkResultFn = nullptr;

        ImGui_ImplVulkan_Init(&init_info);

        // Reupload fonts if necessary (usually not).
        // If you get rendering issues after recreate, re-upload fonts similarly to initialize().
    }

    // Clean up ImGui resources
    void ImGuiLayer::shutdown() noexcept
    {
        if (!initialized)
            return;

        vkDeviceWaitIdle(device);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (descriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }

        initialized = false;
    }

} // namespace UI
