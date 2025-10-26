#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace Vk
{

    class VulkanLogicalDevice;
    class RenderPass;

    /**
     * @brief Basic graphics pipeline for the mesh pass (with textures).
     *
     * Descriptor set layout:
     *   set = 0, binding = 0 : UBO with Render::ViewUniforms (view / proj / viewProj / cameraPos) — VS
     *   set = 1, binding = 0 : combined image sampler (albedo) — FS
     *
     * Push constants:
     *   - vertex stage: mat4 model (64 bytes)
     *
     * NOTE (TEMPORARY):
     *   Exposing descriptor set layouts here only to let RendererContext allocate sets.
     *   We'll move this to a material/resource module later.
     */
    class GraphicsPipeline
    {
    public:
        GraphicsPipeline(const VulkanLogicalDevice &device,
                         const RenderPass &renderPass);
        ~GraphicsPipeline();

        GraphicsPipeline(const GraphicsPipeline &) = delete;
        GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;

        VkPipeline getPipeline() const { return pipeline; }
        VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }

        // external access for context to allocate sets
        VkDescriptorSetLayout getViewSetLayout() const noexcept { return viewSetLayout; } // set=0
        VkDescriptorSetLayout getMaterialSetLayout() const { return materialSetLayout; }  // set=1

    private:
        const VulkanLogicalDevice &device;
        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline pipeline{VK_NULL_HANDLE};

        // set layouts
        VkDescriptorSetLayout viewSetLayout{VK_NULL_HANDLE};     // set=0 (VS UBO)
        VkDescriptorSetLayout materialSetLayout{VK_NULL_HANDLE}; // set=1 (FS albedo sampler)

        VkShaderModule createShaderModule(const std::vector<char> &code) const;
        std::vector<char> readFile(const std::string &filename) const;
    };

} // namespace Vk
