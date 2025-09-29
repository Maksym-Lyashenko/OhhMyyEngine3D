#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace Core
{

    class VulkanLogicalDevice;
    class SwapChain;
    class RenderPass;

    class GraphicsPipeline
    {
    public:
        GraphicsPipeline(const VulkanLogicalDevice &device,
                         const SwapChain &swapChain,
                         const RenderPass &renderPass);
        ~GraphicsPipeline();

        GraphicsPipeline(const GraphicsPipeline &) = delete;
        GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;

        VkPipeline getPipeline() const { return pipeline; }
        VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }

    private:
        const VulkanLogicalDevice &device;
        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline pipeline{VK_NULL_HANDLE};

        VkShaderModule createShaderModule(const std::vector<char> &code) const;
        std::vector<char> readFile(const std::string &filename) const;
    };

} // namespace Core
