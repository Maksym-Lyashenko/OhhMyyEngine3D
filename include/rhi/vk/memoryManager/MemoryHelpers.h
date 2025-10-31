#pragma once
#include "VulkanAllocator.h"
#include "AllocatedResource.h"
#include "core/Logger.h"
#include "rhi/vk/Common.h"

namespace Vk
{
    inline AllocatedBuffer createBuffer(VmaAllocator allocator,
                                        VkDeviceSize size,
                                        VkBufferUsageFlags usage,
                                        VmaMemoryUsage memoryUsage)
    {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;

        AllocatedBuffer out{};
        VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &out.buffer, &out.allocation, nullptr));
        return out;
    }

    inline AllocatedImage createImage(VmaAllocator allocator,
                                      const VkImageCreateInfo &imgInfo,
                                      VmaMemoryUsage memoryUsage)
    {
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;

        AllocatedImage out{};
        VK_CHECK(vmaCreateImage(allocator, &imgInfo, &allocInfo, &out.image, &out.allocation, nullptr));
        return out;
    }
}
