#include "rhi/vk/RendererContext.h"
#include "core/Logger.h"
#include "rhi/vk/Common.h"

#include "render/ViewUniforms.h"

#include <cstring> // std::memcpy
#include <stdexcept>

namespace
{

    // Local helper to find a suitable memory type.
    uint32_t findMemoryType(VkPhysicalDevice phys,
                            uint32_t typeBits,
                            VkMemoryPropertyFlags props)
    {
        VkPhysicalDeviceMemoryProperties mp{};
        vkGetPhysicalDeviceMemoryProperties(phys, &mp);
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        {
            const bool typeOk = (typeBits & (1u << i)) != 0;
            const bool propsOk = (mp.memoryTypes[i].propertyFlags & props) == props;
            if (typeOk && propsOk)
                return i;
        }
        throw std::runtime_error("RendererContext: no suitable memory type for UBO");
    }

} // namespace

namespace Vk
{

    void RendererContext::createViewResources(VkPhysicalDevice physDev)
    {
        // Cleanup if called twice
        destroyViewResources();

        const uint32_t imageCount = static_cast<uint32_t>(swapChain.getImages().size());
        viewUbos.resize(imageCount, VK_NULL_HANDLE);
        viewUboMem.resize(imageCount, VK_NULL_HANDLE);
        viewSets.resize(imageCount, VK_NULL_HANDLE);

        VkDevice dev = device.getDevice();
        const VkDeviceSize uboSize = sizeof(Render::ViewUniforms);

        // 1) Descriptor pool (only uniform buffers for the view UBO)
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = imageCount;

        VkDescriptorPoolCreateInfo poolCi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolCi.poolSizeCount = 1;
        poolCi.pPoolSizes = &poolSize;
        poolCi.maxSets = imageCount;

        VK_CHECK(vkCreateDescriptorPool(dev, &poolCi, nullptr, &descPool));

        // 2) Allocate descriptor sets (set=0 uses pipeline's layout)
        const VkDescriptorSetLayout viewLayout = graphicsPipeline.getViewSetLayout();
        std::vector<VkDescriptorSetLayout> layouts(imageCount, viewLayout);

        VkDescriptorSetAllocateInfo dsAi{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dsAi.descriptorPool = descPool;
        dsAi.descriptorSetCount = imageCount;
        dsAi.pSetLayouts = layouts.data();

        VK_CHECK(vkAllocateDescriptorSets(dev, &dsAi, viewSets.data()));

        // 3) Create per-image UBOs
        for (uint32_t i = 0; i < imageCount; ++i)
        {
            VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bi.size = uboSize;
            bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VK_CHECK(vkCreateBuffer(dev, &bi, nullptr, &viewUbos[i]));

            VkMemoryRequirements req{};
            vkGetBufferMemoryRequirements(dev, viewUbos[i], &req);

            VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            ai.allocationSize = req.size;
            ai.memoryTypeIndex = findMemoryType(physDev, req.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            VK_CHECK(vkAllocateMemory(dev, &ai, nullptr, &viewUboMem[i]));
            VK_CHECK(vkBindBufferMemory(dev, viewUbos[i], viewUboMem[i], 0));

            // 4) Write descriptors
            VkDescriptorBufferInfo dbi{};
            dbi.buffer = viewUbos[i];
            dbi.offset = 0;
            dbi.range = uboSize;

            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = viewSets[i];
            write.dstBinding = 0;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &dbi;

            vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
        }

        Core::Logger::log(Core::LogLevel::INFO, "RendererContext: view UBO resources created");
    }

    void RendererContext::updateViewUbo(uint32_t imageIndex, const Render::ViewUniforms &uboData) const
    {
        VkDevice dev = device.getDevice();

        void *mapped = nullptr;
        VK_CHECK(vkMapMemory(dev, viewUboMem[imageIndex], 0, sizeof(Render::ViewUniforms), 0, &mapped));
        std::memcpy(mapped, &uboData, sizeof(Render::ViewUniforms));
        vkUnmapMemory(dev, viewUboMem[imageIndex]);
    }

    void RendererContext::destroyViewResources() noexcept
    {
        VkDevice dev = device.getDevice();

        for (uint32_t i = 0; i < viewUbos.size(); ++i)
        {
            if (viewUbos[i])
                vkDestroyBuffer(dev, viewUbos[i], nullptr);
            if (viewUboMem[i])
                vkFreeMemory(dev, viewUboMem[i], nullptr);
        }
        viewUbos.clear();
        viewUboMem.clear();

        if (descPool)
        {
            vkDestroyDescriptorPool(dev, descPool, nullptr);
            descPool = VK_NULL_HANDLE;
        }
        viewSets.clear();
    }

} // namespace Vk
