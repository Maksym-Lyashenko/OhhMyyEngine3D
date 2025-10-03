#include "rhi/vk/GraphicsPipeline.h"

#include "rhi/vk/VulkanLogicalDevice.h"
#include "rhi/vk/SwapChain.h"
#include "rhi/vk/RenderPass.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

#include "rhi/vk/gfx/Vertex.h"

#include <fstream>
#include <stdexcept>
#include <array>
#include <glm/mat4x4.hpp> // for sizeof(glm::mat4) in push-constant range

namespace Vk
{

    GraphicsPipeline::GraphicsPipeline(const VulkanLogicalDevice &device_,
                                       const RenderPass &renderPass)
        : device(device_)
    {
        // 1) Load SPIR-V
        auto vertShaderCode = readFile("shaders/vert.spv");
        auto fragShaderCode = readFile("shaders/frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertShaderModule;
        vertStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragShaderModule;
        fragStage.pName = "main";

        VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

        // 2) Dynamic state (viewport, scissor are set at record time)
        const std::array<VkDynamicState, 2> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicInfo.pDynamicStates = dynamicStates.data();

        // 3) Vertex input
        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        auto binding = Gfx::Vertex::binding();
        auto attrs = Gfx::Vertex::attributes();

        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
        vertexInput.pVertexAttributeDescriptions = attrs.data();

        // 4) Input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // 5) Viewport state (counts only; actual viewport/scissor are dynamic)
        VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        // 6) Rasterization
        VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.lineWidth = 1.0f;

        // 7) Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // 8) Depth/stencil
        VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        // 9) Color blend
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // 10) Descriptor set layout (set = 0, binding = 0) for View UBO
        VkDescriptorSetLayoutBinding viewBinding{};
        viewBinding.binding = 0;
        viewBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        viewBinding.descriptorCount = 1;
        viewBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        viewBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo dslCi{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dslCi.bindingCount = 1;
        dslCi.pBindings = &viewBinding;

        VK_CHECK(vkCreateDescriptorSetLayout(device.getDevice(), &dslCi, nullptr, &viewSetLayout));

        // 11) Push constants: ONLY mat4 model (64 bytes)
        VkPushConstantRange pcRange{};
        pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pcRange.offset = 0;
        pcRange.size = static_cast<uint32_t>(sizeof(glm::mat4));

        // 12) Pipeline layout = { descriptor set layout(s), push-constant range(s) }
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &viewSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pcRange;

        VK_CHECK(vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout));

        // 13) Graphics pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pDynamicState = &dynamicInfo;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass.get();
        pipelineInfo.subpass = 0;

        VK_CHECK(vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

        // 14) Cleanup shader modules
        vkDestroyShaderModule(device.getDevice(), vertShaderModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), fragShaderModule, nullptr);

        Core::Logger::log(Core::LogLevel::INFO, "Graphics pipeline created successfully (UBO+PC)");
    }

    GraphicsPipeline::~GraphicsPipeline()
    {
        if (pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
            Core::Logger::log(Core::LogLevel::INFO, "Graphics pipeline destroyed");
        }
        if (pipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
        }
        if (viewSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(device.getDevice(), viewSetLayout, nullptr);
        }
    }

    VkShaderModule GraphicsPipeline::createShaderModule(const std::vector<char> &code) const
    {
        VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        VK_CHECK(vkCreateShaderModule(device.getDevice(), &createInfo, nullptr, &shaderModule));
        return shaderModule;
    }

    std::vector<char> GraphicsPipeline::readFile(const std::string &filename) const
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error("Failed to open file " + filename);

        const size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        return buffer;
    }

} // namespace Vk
