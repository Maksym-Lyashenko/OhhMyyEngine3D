#include "rhi/vk/GraphicsPipeline.h"

#include "rhi/vk/VulkanLogicalDevice.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

#include "rhi/vk/gfx/Vertex.h"

#include <fstream>
#include <stdexcept>
#include <array>
#include <glm/mat4x4.hpp> // for sizeof(glm::mat4)

namespace Vk
{

    GraphicsPipeline::GraphicsPipeline(const VulkanLogicalDevice &device_,
                                       VkFormat colorFormat,
                                       VkFormat depthFormat)
        : device(device_)
    {
        // --- 1) Load SPIR-V ---
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

        // --- 2) Dynamic state (viewport/scissor) ---
        const std::array<VkDynamicState, 2> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicInfo.pDynamicStates = dynamicStates.data();

        // --- 3) Vertex input (pos, normal, uv) ---
        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        auto binding = Gfx::Vertex::binding();
        auto attrs = Gfx::Vertex::attributes();
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
        vertexInput.pVertexAttributeDescriptions = attrs.data();

        // --- 4) Input assembly ---
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // --- 5) Viewport state (counts only; actual values are dynamic) ---
        VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        // --- 6) Rasterization ---
        VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.lineWidth = 1.0f;

        // --- 7) Multisampling ---
        VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // --- 8) Depth/stencil ---
        VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        // --- 9) Color blend ---
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // --- 10) Descriptor set layouts ---
        // set = 0 (View UBO for VS)
        VkDescriptorSetLayoutBinding viewBinding{};
        viewBinding.binding = 0;
        viewBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        viewBinding.descriptorCount = 1;
        viewBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo viewDslCi{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        viewDslCi.bindingCount = 1;
        viewDslCi.pBindings = &viewBinding;
        VK_CHECK(vkCreateDescriptorSetLayout(device.getDevice(), &viewDslCi, nullptr, &viewSetLayout));

        // set = 1 (Material: 5 textures + 1 UBO for FS)
        std::array<VkDescriptorSetLayoutBinding, 6> matBindings{};

        auto makeImgBinding = [](uint32_t b)
        {
            VkDescriptorSetLayoutBinding x{};
            x.binding = b;
            x.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            x.descriptorCount = 1;
            x.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // читаем в FS
            return x;
        };

        // 0: baseColor (sRGB)
        matBindings[0] = makeImgBinding(0);
        // 1: normal (UNORM)
        matBindings[1] = makeImgBinding(1);
        // 2: metallic-roughness (UNORM)
        matBindings[2] = makeImgBinding(2);
        // 3: occlusion (UNORM)
        matBindings[3] = makeImgBinding(3);
        // 4: emissive (sRGB)
        matBindings[4] = makeImgBinding(4);

        // 5: material params UBO
        matBindings[5].binding = 5;
        matBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        matBindings[5].descriptorCount = 1;
        matBindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // параметры шейдера FS

        VkDescriptorSetLayoutCreateInfo matDslCi{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        matDslCi.bindingCount = static_cast<uint32_t>(matBindings.size());
        matDslCi.pBindings = matBindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(device.getDevice(), &matDslCi, nullptr, &materialSetLayout));

        // --- 11) Push constants: mat4 model (VS) ---
        VkPushConstantRange pcRange{};
        pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pcRange.offset = 0;
        pcRange.size = static_cast<uint32_t>(sizeof(glm::mat4));

        // --- 12) Pipeline layout with two set layouts ---
        const VkDescriptorSetLayout setLayouts[] = {viewSetLayout, materialSetLayout};
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(std::size(setLayouts));
        pipelineLayoutInfo.pSetLayouts = setLayouts;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pcRange;

        VK_CHECK(vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout));

        // Debug: ensure depthFormat is valid
        if (depthFormat == VK_FORMAT_UNDEFINED)
        {
            Core::Logger::log(Core::LogLevel::ERROR, "GraphicsPipeline: depthFormat is VK_FORMAT_UNDEFINED! Aborting pipeline creation.");
            throw std::runtime_error("GraphicsPipeline: depthFormat is VK_FORMAT_UNDEFINED");
        }
        else
        {
            Core::Logger::log(Core::LogLevel::INFO, "GraphicsPipeline: creating with colorFormat=" + std::to_string(colorFormat) + " depthFormat=" + std::to_string(depthFormat));
        }

        // --- 13) DYNAMIC RENDERING ---
        VkPipelineRenderingCreateInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachmentFormats = &colorFormat;
        renderingInfo.depthAttachmentFormat = depthFormat;

        // --- 14) Graphics pipeline with dynamic rendering ---
        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.pNext = &renderingInfo; // Добавляем dynamic rendering info
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
        pipelineInfo.renderPass = VK_NULL_HANDLE; // Обязательно VK_NULL_HANDLE для dynamic rendering!
        pipelineInfo.subpass = 0;

        VK_CHECK(vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

        // --- 15) Cleanup shader modules ---
        vkDestroyShaderModule(device.getDevice(), vertShaderModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), fragShaderModule, nullptr);

        Core::Logger::log(Core::LogLevel::INFO, "Graphics pipeline created successfully (Dynamic Rendering)");
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
        if (materialSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(device.getDevice(), materialSetLayout, nullptr);
        }
    }

    VkShaderModule GraphicsPipeline::createShaderModule(const std::vector<char> &code) const
    {
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        ci.codeSize = code.size();
        ci.pCode = reinterpret_cast<const uint32_t *>(code.data());

        VkShaderModule m = VK_NULL_HANDLE;
        VK_CHECK(vkCreateShaderModule(device.getDevice(), &ci, nullptr, &m));
        return m;
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