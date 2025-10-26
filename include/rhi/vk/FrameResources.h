#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

#include "core/Logger.h"
#include "rhi/vk/Common.h"

#include "render/ViewUniforms.h" // CPU-side UBO layout

namespace Vk
{

    /**
     * Per-swapchain-image resources: UBO buffer + memory + descriptor set.
     * Owned by RendererContext (or FrameResourcesManager).
     */
    struct FrameResources
    {
        VkBuffer viewUbo = VK_NULL_HANDLE;
        VkDeviceMemory viewUboMem = VK_NULL_HANDLE;
        VkDescriptorSet viewSet = VK_NULL_HANDLE;

        // Create per-image resources:
        // - create buffer (HOST_VISIBLE | HOST_COHERENT)
        // - allocate descriptor set from provided pool using viewSetLayout
        // - write descriptor set with buffer info
        //
        // phys = VkPhysicalDevice (for memory selection)
        // device = VkDevice
        // descPool = descriptor pool to allocate from
        // viewSetLayout = layout for set = 0 binding 0
        void createForImage(VkPhysicalDevice phys,
                            VkDevice device,
                            VkDescriptorPool descPool,
                            VkDescriptorSetLayout viewSetLayout)
        {
            // create buffer
            VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bi.size = sizeof(Render::ViewUniforms);
            bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            VK_CHECK(vkCreateBuffer(device, &bi, nullptr, &viewUbo));

            VkMemoryRequirements req{};
            vkGetBufferMemoryRequirements(device, viewUbo, &req);

            VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            ai.allocationSize = req.size;

            // choose memory type (helper below)
            ai.memoryTypeIndex = findMemoryTypeIndex(phys, req.memoryTypeBits,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            VK_CHECK(vkAllocateMemory(device, &ai, nullptr, &viewUboMem));
            VK_CHECK(vkBindBufferMemory(device, viewUbo, viewUboMem, 0));

            // allocate descriptor set
            VkDescriptorSetAllocateInfo dsAi{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            dsAi.descriptorPool = descPool;
            dsAi.descriptorSetCount = 1;
            dsAi.pSetLayouts = &viewSetLayout;
            VK_CHECK(vkAllocateDescriptorSets(device, &dsAi, &viewSet));

            // write descriptor set
            VkDescriptorBufferInfo bufInfo{};
            bufInfo.buffer = viewUbo;
            bufInfo.offset = 0;
            bufInfo.range = sizeof(Render::ViewUniforms);

            VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            w.dstSet = viewSet;
            w.dstBinding = 0;
            w.dstArrayElement = 0;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w.pBufferInfo = &bufInfo;

            vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
        }

        // update UBO with host-visible memory
        void updateViewUbo(VkDevice device, const Render::ViewUniforms &u)
        {
            void *mapped = nullptr;
            VK_CHECK(vkMapMemory(device, viewUboMem, 0, sizeof(Render::ViewUniforms), 0, &mapped));
            std::memcpy(mapped, &u, sizeof(Render::ViewUniforms));
            vkUnmapMemory(device, viewUboMem);
        }

        void destroy(VkDevice device) noexcept
        {
            if (viewSet != VK_NULL_HANDLE)
                viewSet = VK_NULL_HANDLE; // freed with pool
            if (viewUbo)
            {
                vkDestroyBuffer(device, viewUbo, nullptr);
                viewUbo = VK_NULL_HANDLE;
            }
            if (viewUboMem)
            {
                vkFreeMemory(device, viewUboMem, nullptr);
                viewUboMem = VK_NULL_HANDLE;
            }
        }

    private:
        // helper (local) to pick memory type index
        static uint32_t findMemoryTypeIndex(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags props)
        {
            VkPhysicalDeviceMemoryProperties mp{};
            vkGetPhysicalDeviceMemoryProperties(phys, &mp);
            for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
                if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props)
                    return i;
            throw std::runtime_error("FrameResources: no suitable memory type");
        }
    };

} // namespace Vk
