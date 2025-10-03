#include "rhi/vk/RendererContext.h"
#include "core/Logger.h"
#include "rhi/vk/Common.h"

#include "render/ViewUniforms.h"

#include <cstring> // std::memcpy
#include <stdexcept>

namespace
{
    // Local helper to find a suitable memory type.
    // (TEMPORARY) duplicated in several places — later unify in a small vk_utils header.
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
        // (TEMPORARY) monolithic creation; later: split into DescriptorAllocator + UboRing.
        destroyViewResources(); // safe no-op if empty

        const uint32_t imageCount = static_cast<uint32_t>(swapChain.getImages().size());
        viewUbos.resize(imageCount, VK_NULL_HANDLE);
        viewUboMem.resize(imageCount, VK_NULL_HANDLE);
        viewSets.resize(imageCount, VK_NULL_HANDLE);

        VkDevice dev = device.getDevice();
        const VkDeviceSize uboSize = sizeof(Render::ViewUniforms);

        // 1) (TEMPORARY) Descriptor pool: only UNIFORM_BUFFER descriptors for set=0
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = imageCount;

        VkDescriptorPoolCreateInfo poolCi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolCi.poolSizeCount = 1;
        poolCi.pPoolSizes = &poolSize;
        poolCi.maxSets = imageCount;

        VK_CHECK(vkCreateDescriptorPool(dev, &poolCi, nullptr, &descPool));

        // 2) Allocate descriptor sets (set=0 uses pipeline's view set layout)
        const VkDescriptorSetLayout viewLayout = graphicsPipeline.getViewSetLayout(); // provided by pipeline
        std::vector<VkDescriptorSetLayout> layouts(imageCount, viewLayout);

        VkDescriptorSetAllocateInfo dsAi{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dsAi.descriptorPool = descPool;
        dsAi.descriptorSetCount = imageCount;
        dsAi.pSetLayouts = layouts.data();

        VK_CHECK(vkAllocateDescriptorSets(dev, &dsAi, viewSets.data()));

        // 3) Per-image UBO buffers (+ memory) — HOST_VISIBLE | HOST_COHERENT for simplicity
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
            write.dstBinding = 0; // binding=0 in the view set layout
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &dbi;

            vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
        }

        Core::Logger::log(Core::LogLevel::INFO, "RendererContext: (TEMP) view UBO resources created");
    }

    void RendererContext::updateViewUbo(uint32_t imageIndex, const Render::ViewUniforms &uboData) const
    {
        // (TEMPORARY) naive map/memcpy/unmap; later consider persistently mapped ranges or a ring buffer.
        VkDevice dev = device.getDevice();

        void *mapped = nullptr;
        VK_CHECK(vkMapMemory(dev, viewUboMem[imageIndex], 0, sizeof(Render::ViewUniforms), 0, &mapped));
        std::memcpy(mapped, &uboData, sizeof(Render::ViewUniforms));
        vkUnmapMemory(dev, viewUboMem[imageIndex]);
    }

    // NEW (TEMPORARY)
    void RendererContext::createMaterialSet(VkImageView imageView, VkSampler sampler, VkImageLayout layout)
    {
        // Drop previous TEMP material set if any
        if (materialPool)
        {
            vkDestroyDescriptorPool(device.getDevice(), materialPool, nullptr);
            materialPool = VK_NULL_HANDLE;
            materialSet = VK_NULL_HANDLE;
        }

        // A tiny pool for 1 combined image sampler
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolCi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolCi.poolSizeCount = 1;
        poolCi.pPoolSizes = &poolSize;
        poolCi.maxSets = 1;

        VK_CHECK(vkCreateDescriptorPool(device.getDevice(), &poolCi, nullptr, &materialPool));

        // Allocate using pipeline's set=1 layout
        VkDescriptorSetLayout matLayout = graphicsPipeline.getMaterialSetLayout();
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = materialPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &matLayout;

        VK_CHECK(vkAllocateDescriptorSets(device.getDevice(), &ai, &materialSet));

        // Write image+sampler
        VkDescriptorImageInfo di{};
        di.sampler = sampler;
        di.imageView = imageView;
        di.imageLayout = layout;

        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = materialSet;
        write.dstBinding = 0; // binding 0 in set=1
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &di;

        vkUpdateDescriptorSets(device.getDevice(), 1, &write, 0, nullptr);
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

        // TEMP material set/pool
        if (materialPool)
        {
            vkDestroyDescriptorPool(dev, materialPool, nullptr);
            materialPool = VK_NULL_HANDLE;
            materialSet = VK_NULL_HANDLE;
        }

        // (TEMPORARY) After textures land, this block should move out of RendererContext.
    }

} // namespace Vk
