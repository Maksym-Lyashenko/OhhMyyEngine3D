#pragma once
#include <vk_mem_alloc.h>

namespace Vk
{
    struct AllocatedBuffer
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;

        void destroy(VmaAllocator allocator)
        {
            if (buffer)
            {
                vmaDestroyBuffer(allocator, buffer, allocation);
                buffer = VK_NULL_HANDLE;
                allocation = VK_NULL_HANDLE;
            }
        }
    };

    struct AllocatedImage
    {
        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;

        void destroy(VmaAllocator allocator)
        {
            if (image)
            {
                vmaDestroyImage(allocator, image, allocation);
                image = VK_NULL_HANDLE;
                allocation = VK_NULL_HANDLE;
            }
        }
    };
}
