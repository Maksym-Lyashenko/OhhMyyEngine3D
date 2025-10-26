#include "rhi/vk/RendererContext.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

#include "render/ViewUniforms.h"

#include <cstring> // std::memcpy
#include <stdexcept>

namespace Vk
{
    VkDescriptorPool viewDescPool = VK_NULL_HANDLE;
    std::vector<Vk::FrameResources> frameResources;

    void RendererContext::createViewResources(VkPhysicalDevice physDev)
    {
        destroyViewResources(); // safe no-op if empty

        // Create descriptor pool for view sets (small)
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = static_cast<uint32_t>(swapChain.getImages().size());

        VkDescriptorPoolCreateInfo poolCi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolCi.poolSizeCount = 1;
        poolCi.pPoolSizes = &poolSize;
        poolCi.maxSets = static_cast<uint32_t>(swapChain.getImages().size());

        VK_CHECK(vkCreateDescriptorPool(device.getDevice(), &poolCi, nullptr, &viewDescPool));

        // Ensure frameResourses has correct size
        const size_t imageCount = swapChain.getImages().size();
        frameResources.resize(imageCount);

        for (size_t i = 0; i < imageCount; i++)
        {
            frameResources[i].createForImage(
                physDev,
                device.getDevice(),
                viewDescPool,
                graphicsPipeline.getViewSetLayout());
            // Note: graphicsPipeline.getViewSetLayout() should return VkDescriptorSetLayout for set = 0
        }

        Core::Logger::log(Core::LogLevel::INFO, "RendererContext: (TEMP) view UBO resources created");
    }

    void RendererContext::updateViewUbo(uint32_t imageIndex, const Render::ViewUniforms &uboData) const
    {
        if (imageIndex >= frameResources.size())
            return;
        frameResources[imageIndex].updateViewUbo(device.getDevice(), uboData);
    }

    void RendererContext::destroyViewResources() noexcept
    {
        if (!device.getDevice())
            return;

        for (auto &fr : frameResources)
        {
            fr.destroy(device.getDevice());
        }
        frameResources.clear();

        if (viewDescPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(device.getDevice(), viewDescPool, nullptr);
            viewDescPool = VK_NULL_HANDLE;
        }
    }

} // namespace Vk
