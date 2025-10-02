#include "rhi/vk/gfx/Buffer.h"
#include "core/Logger.h"

#include "rhi/vk/Common.h"

#include <cstring>

namespace Vk::Gfx
{

    uint32_t Buffer::findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props)
    {
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        {
            if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
                return i;
        }
        throw std::runtime_error("No suitable memory type found");
    }

    void Buffer::create(VkPhysicalDevice phys, VkDevice dev,
                        VkDeviceSize sz, VkBufferUsageFlags usage,
                        VkMemoryPropertyFlags props)
    {
        device = dev;
        size = sz;

        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = sz;
        bi.usage = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(device, &bi, nullptr, &buffer));

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, buffer, &req);

        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = findMemoryType(phys, req.memoryTypeBits, props);

        VK_CHECK(vkAllocateMemory(device, &ai, nullptr, &memory));
        VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0));
    }

    void Buffer::upload(const void *data, size_t bytes)
    {
        void *mapped = nullptr;
        VK_CHECK(vkMapMemory(device, memory, 0, bytes, 0, &mapped));
        std::memcpy(mapped, data, bytes);
        vkUnmapMemory(device, memory);
    }

    void Buffer::destroy()
    {
        if (buffer)
        {
            vkDestroyBuffer(device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
        if (memory)
        {
            vkFreeMemory(device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
        size = 0;
    }

} // namespace Vk::Gfx
