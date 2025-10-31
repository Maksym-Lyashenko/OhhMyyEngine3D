#include "rhi/vk/DepthResources.h"

#include "rhi/vk/vk_utils.h" // FindSupportedDepthFormat(phys)
#include "rhi/vk/Common.h"   // VK_CHECK

#include <stdexcept>

namespace Vk
{
    void DepthResources::create(VkPhysicalDevice physicalDevice,
                                VkDevice inDevice,
                                VmaAllocator inAllocator,
                                VkExtent2D extent,
                                VkCommandPool commandPool,
                                VkQueue graphicsQueue,
                                VkSampleCountFlagBits samples)
    {
        // Save context
        device_ = inDevice;
        allocator_ = inAllocator;
        samples_ = samples;

        // Pick a supported depth format (D32 preferred, fallbacks w/ stencil if needed)
        format_ = FindSupportedDepthFormat(physicalDevice);

        // 1) Create depth image via VMA (GPU-only)
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

        VmaAllocationCreateInfo aci{};
        aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateImage(allocator_, &img, &aci, &image_, &allocation_, nullptr));

        // 2) Create image view
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

        // 3) Transition to DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        transitionToAttachment(commandPool, graphicsQueue);
    }

    void DepthResources::transitionToAttachment(VkCommandPool commandPool, VkQueue queue)
    {
        // Allocate a one-time command buffer
        VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandPool = commandPool;
        alloc.commandBufferCount = 1;

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateCommandBuffers(device_, &alloc, &cmd));

        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

        // Synchronization2 barrier: UNDEFINED -> DEPTH_STENCIL_ATTACHMENT_OPTIMAL
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

        // Submit and wait (simple path)
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
            // Release VkImage and its allocation together
            vmaDestroyImage(allocator_, image_, allocation_);
            image_ = VK_NULL_HANDLE;
            allocation_ = VK_NULL_HANDLE;
        }

        device_ = VK_NULL_HANDLE;
        allocator_ = VK_NULL_HANDLE;
        format_ = VK_FORMAT_D32_SFLOAT;
        samples_ = VK_SAMPLE_COUNT_1_BIT;
    }

} // namespace Vk
