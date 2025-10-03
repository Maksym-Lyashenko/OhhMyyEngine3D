#include "rhi/vk/DepthResources.h"

#include "rhi/vk/vk_utils.h" // FindSupportedDepthFormat
#include "rhi/vk/Common.h"   // VK_CHECK
#include <stdexcept>
#include <cstring>

namespace
{

    // Local helper: find a memory type index for the requested properties.
    uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags props)
    {
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        {
            const bool typeOk = (typeBits & (1u << i)) != 0;
            const bool propsOk = (memProps.memoryTypes[i].propertyFlags & props) == props;
            if (typeOk && propsOk)
                return i;
        }
        throw std::runtime_error("DepthResources: no suitable memory type");
    }

} // namespace

namespace Vk
{

    void DepthResources::create(VkPhysicalDevice physicalDevice,
                                VkDevice inDevice,
                                VkExtent2D extent,
                                VkCommandPool commandPool,
                                VkQueue graphicsQueue,
                                VkSampleCountFlagBits samples)
    {
        device_ = inDevice;
        samples_ = samples;
        format_ = FindSupportedDepthFormat(physicalDevice); // chooses best available depth format

        // 1) Create image
        VkImageCreateInfo img{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        img.imageType = VK_IMAGE_TYPE_2D;
        img.extent = {extent.width, extent.height, 1u};
        img.mipLevels = 1;
        img.arrayLayers = 1;
        img.format = format_;
        img.tiling = VK_IMAGE_TILING_OPTIMAL;
        img.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        img.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        img.samples = samples_;
        img.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateImage(device_, &img, nullptr, &image_));

        // 2) Allocate & bind memory
        VkMemoryRequirements memReq{};
        vkGetImageMemoryRequirements(device_, image_, &memReq);

        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize = memReq.size;
        alloc.memoryTypeIndex = findMemoryType(physicalDevice, memReq.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK(vkAllocateMemory(device_, &alloc, nullptr, &memory_));
        VK_CHECK(vkBindImageMemory(device_, image_, memory_, 0));

        // 3) Create image view
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencil(format_))
            aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format_;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &view_));

        // 4) Transition to DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        transitionToAttachment(commandPool, graphicsQueue);
    }

    void DepthResources::transitionToAttachment(VkCommandPool commandPool, VkQueue queue)
    {
        // One-time command buffer
        VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandPool = commandPool;
        alloc.commandBufferCount = 1;

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateCommandBuffers(device_, &alloc, &cmd));

        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

        // Use synchronization2 barrier to set the optimal layout for depth attachment
        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        barrier.srcAccessMask = 0;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.image = image_;

        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencil(format_))
            aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

        barrier.subresourceRange.aspectMask = aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;

        vkCmdPipelineBarrier2(cmd, &dep);
        VK_CHECK(vkEndCommandBuffer(cmd));

        // Submit and wait (simple path; you could use vkQueueSubmit2 if preferred)
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(queue));

        vkFreeCommandBuffers(device_, commandPool, 1, &cmd);
    }

    void DepthResources::destroy()
    {
        if (!device_)
            return;

        if (view_)
        {
            vkDestroyImageView(device_, view_, nullptr);
            view_ = VK_NULL_HANDLE;
        }
        if (image_)
        {
            vkDestroyImage(device_, image_, nullptr);
            image_ = VK_NULL_HANDLE;
        }
        if (memory_)
        {
            vkFreeMemory(device_, memory_, nullptr);
            memory_ = VK_NULL_HANDLE;
        }

        device_ = VK_NULL_HANDLE;
        format_ = VK_FORMAT_D32_SFLOAT;
        samples_ = VK_SAMPLE_COUNT_1_BIT;
    }

} // namespace Vk
