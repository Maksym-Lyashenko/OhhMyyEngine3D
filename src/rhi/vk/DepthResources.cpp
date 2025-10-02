#include "rhi/vk/DepthResources.h"

#include "rhi/vk/vk_utils.h" // FindSupportedDepthFormat
#include <rhi/vk/Common.h>

#include <stdexcept>
#include <cstring>

namespace Vk
{

    static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags props)
    {
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
            if ((typeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
                return i;
        throw std::runtime_error("No suitable memory type for depth image");
    }

    void DepthResources::create(VkPhysicalDevice physicalDevice,
                                VkDevice inDevice,
                                VkExtent2D extent,
                                VkCommandPool commandPool,
                                VkQueue graphicsQueue)
    {
        device = inDevice;
        format = FindSupportedDepthFormat(physicalDevice);

        // 1) Image
        VkImageCreateInfo img{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        img.imageType = VK_IMAGE_TYPE_2D;
        img.extent = {extent.width, extent.height, 1};
        img.mipLevels = 1;
        img.arrayLayers = 1;
        img.format = format;
        img.tiling = VK_IMAGE_TILING_OPTIMAL;
        img.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        img.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        img.samples = VK_SAMPLE_COUNT_1_BIT;
        img.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &img, nullptr, &image) != VK_SUCCESS)
            throw std::runtime_error("Failed to create depth image");

        // 2) Memory
        VkMemoryRequirements memReq{};
        vkGetImageMemoryRequirements(device, image, &memReq);

        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize = memReq.size;
        alloc.memoryTypeIndex = findMemoryType(physicalDevice, memReq.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &alloc, nullptr, &memory) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate depth memory");

        vkBindImageMemory(device, image, memory, 0);

        // 3) View
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencil(format))
            aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS)
            throw std::runtime_error("Failed to create depth view");

        // 4) Layout → DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        transitionToAttachment(commandPool, graphicsQueue);
    }

    void DepthResources::transitionToAttachment(VkCommandPool commandPool, VkQueue queue)
    {
        // one-time command buffer
        VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandPool = commandPool;
        alloc.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device, &alloc, &cmd);

        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);

        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        barrier.srcAccessMask = 0;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.image = image;

        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencil(format))
            aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

        barrier.subresourceRange.aspectMask = aspect;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;

        vkCmdPipelineBarrier2(cmd, &dep);
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(device, commandPool, 1, &cmd);
    }

    void DepthResources::destroy()
    {
        if (!device)
            return;
        if (view)
            vkDestroyImageView(device, view, nullptr);
        if (image)
            vkDestroyImage(device, image, nullptr);
        if (memory)
            vkFreeMemory(device, memory, nullptr);
        view = VK_NULL_HANDLE;
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        device = VK_NULL_HANDLE;
    }

} // namespace Vk
