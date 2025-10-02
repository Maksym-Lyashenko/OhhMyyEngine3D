#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace Vk::Gfx
{

    class Buffer
    {
    public:
        Buffer() = default;
        ~Buffer() { destroy(); }

        Buffer(const Buffer &) = delete;
        Buffer &operator=(const Buffer &) = delete;

        void create(VkPhysicalDevice phys, VkDevice dev,
                    VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags props);

        void destroy();

        void upload(const void *data, size_t bytes); // map & memcpy

        VkBuffer get() const { return buffer; }
        VkDeviceSize getSize() const { return size; }

    private:
        static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props);

        VkDevice device{VK_NULL_HANDLE};
        VkBuffer buffer{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkDeviceSize size{0};
    };

} // namespace Vk::Gfx
