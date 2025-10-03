#include "rhi/vk/gfx/Buffer.h"
#include "core/Logger.h"
#include "rhi/vk/Common.h"

#include <cstring>   // std::memcpy
#include <algorithm> // std::min

using Core::Logger;
using Core::LogLevel;

namespace Vk::Gfx
{

    uint32_t Buffer::findMemoryType(VkPhysicalDevice phys,
                                    uint32_t typeFilter,
                                    VkMemoryPropertyFlags props)
    {
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(phys, &memProps);

        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        {
            const bool typeMatch = (typeFilter & (1u << i)) != 0;
            const bool propsOk = (memProps.memoryTypes[i].propertyFlags & props) == props;
            if (typeMatch && propsOk)
                return i;
        }
        throw std::runtime_error("Vk::Gfx::Buffer: no suitable memory type found");
    }

    void Buffer::create(VkPhysicalDevice phys, VkDevice dev,
                        VkDeviceSize sz, VkBufferUsageFlags usage,
                        VkMemoryPropertyFlags props)
    {
        // Clean any previous resources
        destroy();

        device_ = dev;
        size_ = sz;
        props_ = props;

        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = sz;
        bi.usage = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(device_, &bi, nullptr, &buffer_));

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device_, buffer_, &req);

        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = findMemoryType(phys, req.memoryTypeBits, props);

        VK_CHECK(vkAllocateMemory(device_, &ai, nullptr, &memory_));
        VK_CHECK(vkBindBufferMemory(device_, buffer_, memory_, 0));
    }

    void Buffer::upload(const void *data, size_t bytes)
    {
        upload(/*offset=*/0, data, bytes);
    }

    void Buffer::upload(VkDeviceSize offset, const void *data, size_t bytes)
    {
        if (!memory_)
        {
            throw std::runtime_error("Vk::Gfx::Buffer::upload(): buffer not created / memory null");
        }
        if ((props_ & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
        {
            Logger::log(LogLevel::ERROR, "Vk::Gfx::Buffer::upload(): memory is not HOST_VISIBLE");
            throw std::runtime_error("upload() requires HOST_VISIBLE memory");
        }
        if (!data || bytes == 0)
        {
            return; // nothing to copy
        }
        if (offset > size_)
        {
            throw std::runtime_error("Vk::Gfx::Buffer::upload(): offset out of range");
        }

        const VkDeviceSize maxBytes = size_ - offset;
        const VkDeviceSize toCopy = std::min<VkDeviceSize>(maxBytes, bytes);
        if (toCopy != bytes)
        {
            Logger::log(LogLevel::WARNING, "Vk::Gfx::Buffer::upload(): clamping write size to buffer capacity");
        }

        void *mapped = nullptr;
        VK_CHECK(vkMapMemory(device_, memory_, offset, toCopy, 0, &mapped));
        std::memcpy(mapped, data, static_cast<size_t>(toCopy));
        vkUnmapMemory(device_, memory_);

        // If memory is non-coherent, the caller should flush (not handled here).
        // Typical pattern: use HOST_VISIBLE | HOST_COHERENT for staging buffers.
    }

    void Buffer::destroy() noexcept
    {
        if (buffer_)
        {
            vkDestroyBuffer(device_, buffer_, nullptr);
            buffer_ = VK_NULL_HANDLE;
        }
        if (memory_)
        {
            vkFreeMemory(device_, memory_, nullptr);
            memory_ = VK_NULL_HANDLE;
        }
        size_ = 0;
        props_ = 0;
        // device_ intentionally left as-is (device handle lifetime is managed elsewhere)
    }

} // namespace Vk::Gfx
