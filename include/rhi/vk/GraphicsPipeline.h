#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace Vk
{

    class VulkanLogicalDevice;
    class SwapChain;
    class RenderPass;

    /**
     * @brief Basic graphics pipeline for the mesh pass.
     *
     * Layout:
     *   set = 0, binding = 0 : UBO with Render::ViewUniforms (view / proj / viewProj / cameraPos)
     *   push-constants (vertex stage) : mat4 model (64 bytes)
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
        VkDescriptorSetLayout getViewSetLayout() const { return viewSetLayout; }

    private:
        const VulkanLogicalDevice &device;
        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline pipeline{VK_NULL_HANDLE};
        VkDescriptorSetLayout viewSetLayout{VK_NULL_HANDLE}; // set=0 layout for View UBO

        VkShaderModule createShaderModule(const std::vector<char> &code) const;
        std::vector<char> readFile(const std::string &filename) const;
    };

} // namespace Vk
