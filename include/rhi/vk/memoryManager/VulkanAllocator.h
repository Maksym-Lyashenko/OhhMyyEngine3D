#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "rhi/vk/Common.h"

namespace Vk
{
    class VulkanAllocator
    {
    public:
        VulkanAllocator() = default;
        ~VulkanAllocator() { destroy(); }

        void init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
        {
            VmaVulkanFunctions funcs{};
            funcs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
            funcs.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

            VmaAllocatorCreateInfo info{};
            info.instance = instance;
            info.physicalDevice = physicalDevice;
            info.device = device;
            info.pVulkanFunctions = &funcs;
            info.vulkanApiVersion = VK_API_VERSION_1_4;

            VK_CHECK(vmaCreateAllocator(&info, &allocator));
        }

        void destroy()
        {
            if (allocator != VK_NULL_HANDLE)
            {
                vmaDestroyAllocator(allocator);
                allocator = VK_NULL_HANDLE;
            }
        }

        [[nodiscard]] VmaAllocator get() const { return allocator; }

    private:
        VmaAllocator allocator = VK_NULL_HANDLE;
    };
}
