#include "rhi/vk/gfx/Texture2D.h"
#include "rhi/vk/Common.h" // VK_CHECK + logger

#include <algorithm>
#include <cmath>
#include <cstring> // memcpy
#include <vector>

#ifdef OME3D_USE_STB
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

namespace Vk::Gfx
{
    // --- util --------------------------------------------------------------------

    uint32_t Texture2D::findMemoryType(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags props)
    {
        VkPhysicalDeviceMemoryProperties mem{};
        vkGetPhysicalDeviceMemoryProperties(phys, &mem);
        for (uint32_t i = 0; i < mem.memoryTypeCount; ++i)
        {
            if ((typeBits & (1u << i)) && (mem.memoryTypes[i].propertyFlags & props) == props)
                return i;
        }
        throw std::runtime_error("Texture2D: no suitable memory type");
    }

    VkCommandBuffer Texture2D::beginOneTimeCommands(VkDevice device, VkCommandPool pool)
    {
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;

        VkCommandBuffer cmd{};
        VK_CHECK(vkAllocateCommandBuffers(device, &ai, &cmd));

        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &bi));
        return cmd;
    }

    void Texture2D::endOneTimeCommands(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer cmd)
    {
        VK_CHECK(vkEndCommandBuffer(cmd));
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        VK_CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(queue));
        vkFreeCommandBuffers(device, pool, 1, &cmd);
    }

    // Transition image layouts using PipelineBarrier (sync2 not required here)
    void Texture2D::transition(VkCommandBuffer cmd,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout,
                               uint32_t baseMip,
                               uint32_t mipCount)
    {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.image = image_;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = baseMip;
        barrier.subresourceRange.levelCount = mipCount;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        // simple src/dst masks for common paths
        VkPipelineStageFlags srcStage{};
        VkPipelineStageFlags dstStage{};
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            // fallback (could be extended if needed)
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = 0;
            srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // --- creation ----------------------------------------------------------------

    void Texture2D::createImage(VkPhysicalDevice phys,
                                uint32_t w, uint32_t h, uint32_t mips,
                                VkFormat fmt, VkImageUsageFlags usage)
    {
        width_ = w;
        height_ = h;
        mipLevels_ = mips;
        format_ = fmt;

        VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.extent = {w, h, 1};
        ii.mipLevels = mips;
        ii.arrayLayers = 1;
        ii.format = fmt;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ii.usage = usage;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateImage(device_, &ii, nullptr, &image_));

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(device_, image_, &req);

        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = findMemoryType(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK(vkAllocateMemory(device_, &ai, nullptr, &memory_));
        VK_CHECK(vkBindImageMemory(device_, image_, memory_, 0));
    }

    void Texture2D::createImageView()
    {
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = image_;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = format_;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.baseMipLevel = 0;
        vi.subresourceRange.levelCount = mipLevels_;
        vi.subresourceRange.baseArrayLayer = 0;
        vi.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device_, &vi, nullptr, &imageView_));
    }

    void Texture2D::createSampler()
    {
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        si.magFilter = VK_FILTER_LINEAR;
        si.minFilter = VK_FILTER_LINEAR;
        si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.mipLodBias = 0.0f;
        si.anisotropyEnable = VK_FALSE; // we didn't enable the feature on device
        si.maxAnisotropy = 1.0f;
        si.compareEnable = VK_FALSE;
        si.minLod = 0.0f;
        si.maxLod = float(mipLevels_);
        si.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        si.unnormalizedCoordinates = VK_FALSE;

        VK_CHECK(vkCreateSampler(device_, &si, nullptr, &sampler_));
    }

    void Texture2D::copyBufferToImage(VkDevice device,
                                      VkPhysicalDevice phys,
                                      VkCommandPool pool,
                                      VkQueue queue,
                                      const void *pixels,
                                      size_t byteSize)
    {
        // staging buffer
        VkBuffer staging{};
        VkDeviceMemory stagingMem{};

        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = byteSize;
        bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(device, &bi, nullptr, &staging));

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, staging, &req);

        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = findMemoryType(phys, req.memoryTypeBits,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK(vkAllocateMemory(device, &ai, nullptr, &stagingMem));
        VK_CHECK(vkBindBufferMemory(device, staging, stagingMem, 0));

        // upload
        void *mapped = nullptr;
        VK_CHECK(vkMapMemory(device, stagingMem, 0, ai.allocationSize, 0, &mapped));
        std::memcpy(mapped, pixels, byteSize);
        vkUnmapMemory(device, stagingMem);

        // copy buffer -> image (mip 0)
        VkCommandBuffer cmd = beginOneTimeCommands(device, pool);

        transition(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, mipLevels_);

        VkBufferImageCopy copy{};
        copy.bufferOffset = 0;
        copy.bufferRowLength = 0; // tightly packed
        copy.bufferImageHeight = 0;
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 1;
        copy.imageOffset = {0, 0, 0};
        copy.imageExtent = {width_, height_, 1};

        vkCmdCopyBufferToImage(cmd, staging, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        endOneTimeCommands(device, queue, pool, cmd);

        // destroy staging
        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
    }

    void Texture2D::generateMipmaps(VkPhysicalDevice /*phys*/,
                                    VkCommandPool pool,
                                    VkQueue queue)
    {
        // linear blits through mip chain
        VkCommandBuffer cmd = beginOneTimeCommands(device_, pool);

        int32_t mipW = int32_t(width_);
        int32_t mipH = int32_t(height_);

        for (uint32_t i = 1; i < mipLevels_; ++i)
        {
            // transition src (i-1) to TRANSFER_SRC
            transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, i - 1, 1);

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipW, mipH, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;

            int32_t dstW = std::max(1, mipW / 2);
            int32_t dstH = std::max(1, mipH / 2);

            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {dstW, dstH, 1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(cmd,
                           image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit, VK_FILTER_LINEAR);

            // after blit, we’ll transition the previous level to shader read at the end
            mipW = dstW;
            mipH = dstH;
        }

        // transition the last level (still DST) to SHADER_READ
        transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels_ - 1, 1);
        // transition all previous levels (now SRC) to SHADER_READ
        transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, mipLevels_ - 1);

        endOneTimeCommands(device_, queue, pool, cmd);
    }

    // --- public API ---------------------------------------------------------------

    void Texture2D::createFromRGBA8(VkPhysicalDevice phys,
                                    VkDevice device,
                                    VkCommandPool cmdPool,
                                    VkQueue graphicsQueue,
                                    const void *pixels,
                                    uint32_t w,
                                    uint32_t h,
                                    bool generateMips,
                                    VkFormat format)
    {
        destroy(); // just in case

        device_ = device;
        width_ = w;
        height_ = h;
        format_ = format;
        mipLevels_ = generateMips ? (1u + uint32_t(std::floor(std::log2(std::max(w, h))))) : 1u;

        // create image with TRANSFER_DST for upload; TRANSFER_SRC if we generate mips
        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (mipLevels_ > 1)
            usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        createImage(phys, w, h, mipLevels_, format, usage);
        copyBufferToImage(device_, phys, cmdPool, graphicsQueue, pixels, size_t(w) * h * 4);

        if (mipLevels_ > 1)
        {
            generateMipmaps(phys, cmdPool, graphicsQueue);
        }
        else
        {
            // transition to shader read when no mips
            VkCommandBuffer cmd = beginOneTimeCommands(device_, cmdPool);
            transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 1);
            endOneTimeCommands(device_, graphicsQueue, cmdPool, cmd);
        }

        createImageView();
        createSampler();
    }

    void Texture2D::loadFromFile(VkPhysicalDevice phys,
                                 VkDevice device,
                                 VkCommandPool cmdPool,
                                 VkQueue graphicsQueue,
                                 const std::string &path,
                                 bool genMips,
                                 VkFormat fmt)
    {
#ifndef OME3D_USE_STB
        throw std::runtime_error("Texture2D::loadFromFile: define OME3D_USE_STB and link stb_image to use this helper");
#else
        int w = 0, h = 0, comp = 0;
        stbi_uc *data = stbi_load(path.c_str(), &w, &h, &comp, STBI_rgb_alpha);
        if (!data)
            throw std::runtime_error("stb_image failed to load: " + path);

        createFromRGBA8(phys, device, cmdPool, graphicsQueue, data, uint32_t(w), uint32_t(h), genMips, fmt);
        stbi_image_free(data);
#endif
    }

    void Texture2D::destroy()
    {
        if (!device_)
            return;

        if (sampler_)
        {
            vkDestroySampler(device_, sampler_, nullptr);
            sampler_ = VK_NULL_HANDLE;
        }
        if (imageView_)
        {
            vkDestroyImageView(device_, imageView_, nullptr);
            imageView_ = VK_NULL_HANDLE;
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

        width_ = height_ = 0;
        mipLevels_ = 1;
        device_ = VK_NULL_HANDLE;
    }
} // namespace Vk::Gfx
