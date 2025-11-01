#include "rhi/vk/gfx/Texture2D.h"
#include "rhi/vk/Common.h"
#include "core/StringUtils.h" // Core::Str::assetNameFromPath
#include "rhi/vk/DebugUtils.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#ifdef OME3D_USE_STB
#include <stb_image.h>
#endif

namespace Vk::Gfx
{
    // ------------------------------------------------------------------------
    // One-time command buffer utilities
    // ------------------------------------------------------------------------
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

    // ------------------------------------------------------------------------
    // Layout transition helper
    // ------------------------------------------------------------------------
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

        VkPipelineStageFlags srcStage{};
        VkPipelineStageFlags dstStage{};

        // Determine source/destination access masks and pipeline stages
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
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
            // Fallback case for uncommon transitions
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = 0;
            srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // ------------------------------------------------------------------------
    // Image view and sampler creation
    // ------------------------------------------------------------------------
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
        si.anisotropyEnable = VK_FALSE; // enable if GPU supports anisotropy
        si.maxAnisotropy = 1.0f;
        si.compareEnable = VK_FALSE;
        si.minLod = 0.0f;
        si.maxLod = float(mipLevels_);
        si.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        si.unnormalizedCoordinates = VK_FALSE;

        VK_CHECK(vkCreateSampler(device_, &si, nullptr, &sampler_));
    }

    // ------------------------------------------------------------------------
    // GPU mipmap generation (linear blit between mip levels)
    // ------------------------------------------------------------------------
    void Texture2D::generateMipmaps(VkCommandPool pool, VkQueue queue)
    {
        VkCommandBuffer cmd = beginOneTimeCommands(device_, pool);

        int32_t mipW = int32_t(width_);
        int32_t mipH = int32_t(height_);

        for (uint32_t i = 1; i < mipLevels_; ++i)
        {
            // Previous level: make it a source
            transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, i - 1, 1);

            VkImageBlit blit{};
            blit.srcOffsets[1] = {mipW, mipH, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.layerCount = 1;

            int32_t dstW = std::max(1, mipW / 2);
            int32_t dstH = std::max(1, mipH / 2);

            blit.dstOffsets[1] = {dstW, dstH, 1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(cmd,
                           image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit, VK_FILTER_LINEAR);

            mipW = dstW;
            mipH = dstH;
        }

        // Final layout: all mip levels -> SHADER_READ_ONLY_OPTIMAL
        // last level (still DST) -> SHADER_READ
        transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels_ - 1, 1);
        // all previous levels (currently SRC) -> SHADER_READ
        if (mipLevels_ > 1)
            transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, mipLevels_ - 1);

        endOneTimeCommands(device_, queue, pool, cmd);
    }

    // ------------------------------------------------------------------------
    // Main creation API (VMA)
    // ------------------------------------------------------------------------
    void Texture2D::createFromRGBA8(
        VmaAllocator allocator,
        VkDevice device,
        VkCommandPool cmdPool,
        VkQueue graphicsQueue,
        const void *pixels,
        uint32_t w,
        uint32_t h,
        bool generateMips,
        VkFormat format,
        const char *debugName)
    {
        destroy(); // ensure previous resources are freed

        allocator_ = allocator;
        device_ = device;
        width_ = w;
        height_ = h;
        format_ = format;
        mipLevels_ = generateMips ? (1u + uint32_t(std::floor(std::log2(std::max(w, h))))) : 1u;

        // 1) Allocate GPU image with VMA
        VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.extent = {w, h, 1};
        ii.mipLevels = mipLevels_;
        ii.arrayLayers = 1;
        ii.format = format_;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                   (mipLevels_ > 1 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo aci{};
        aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        VK_CHECK(vmaCreateImage(allocator_, &ii, &aci, &image_, &imageAlloc_, nullptr));
        if (debugName && *debugName)
        {
            vmaSetAllocationName(allocator_, imageAlloc_, debugName);
            nameImage(device_, image_, debugName);
        }

        // 2) Create staging buffer (CPU → GPU)
        VkDeviceSize byteSize = VkDeviceSize(size_t(w) * h * 4);
        VkBuffer staging = VK_NULL_HANDLE;
        VmaAllocation stagingAlloc = VK_NULL_HANDLE;

        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = byteSize;
        bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo sbAci{};
        sbAci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VK_CHECK(vmaCreateBuffer(allocator_, &bi, &sbAci, &staging, &stagingAlloc, nullptr));
        if (debugName && *debugName)
        {
            std::string stgName = std::string(debugName) + " Staging";
            vmaSetAllocationName(allocator_, stagingAlloc, stgName.c_str());
            nameBuffer(device_, staging, stgName.c_str());
        }

        // Upload pixels
        void *mapped = nullptr;
        VK_CHECK(vmaMapMemory(allocator_, stagingAlloc, &mapped));
        std::memcpy(mapped, pixels, size_t(byteSize));
        vmaUnmapMemory(allocator_, stagingAlloc);

        // 3) Copy staging buffer → GPU image
        VkCommandBuffer cmd = beginOneTimeCommands(device_, cmdPool);
        transition(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, mipLevels_);

        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {width_, height_, 1};

        vkCmdCopyBufferToImage(cmd, staging, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        endOneTimeCommands(device_, graphicsQueue, cmdPool, cmd);

        // Free staging buffer
        vmaDestroyBuffer(allocator_, staging, stagingAlloc);

        // 4) Generate mipmaps or transition to shader layout
        if (mipLevels_ > 1)
            generateMipmaps(cmdPool, graphicsQueue);
        else
        {
            VkCommandBuffer cmd2 = beginOneTimeCommands(device_, cmdPool);
            transition(cmd2, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 1);
            endOneTimeCommands(device_, graphicsQueue, cmdPool, cmd2);
        }

        // 5) Create view and sampler
        createImageView();
        if (debugName && *debugName)
        {
            std::string n = std::string(debugName) + " View";
            nameImageView(device_, imageView_, n.c_str());
        }
        createSampler();
        if (debugName && *debugName)
        {
            std::string n = std::string(debugName) + " Sampler";
            nameSampler(device_, sampler_, n.c_str());
        }
    }

    void Texture2D::loadFromFile(
        VmaAllocator allocator,
        VkDevice device,
        VkCommandPool cmdPool,
        VkQueue graphicsQueue,
        const std::string &path,
        bool genMips,
        VkFormat fmt)
    {
#ifndef OME3D_USE_STB
        throw std::runtime_error("Texture2D::loadFromFile requires OME3D_USE_STB defined and stb_image linked.");
#else
        int w = 0, h = 0, comp = 0;
        stbi_uc *data = stbi_load(path.c_str(), &w, &h, &comp, STBI_rgb_alpha);
        if (!data)
            throw std::runtime_error("Failed to load image via stb: " + path);

        const std::string debugName = Core::Str::assetNameFromPath(path);

        createFromRGBA8(allocator, device, cmdPool, graphicsQueue, data, uint32_t(w), uint32_t(h), genMips, fmt, debugName.c_str());
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
            vmaDestroyImage(allocator_, image_, imageAlloc_);
            image_ = VK_NULL_HANDLE;
            imageAlloc_ = VK_NULL_HANDLE;
        }

        width_ = height_ = 0;
        mipLevels_ = 1;
        device_ = VK_NULL_HANDLE;
        allocator_ = VK_NULL_HANDLE;
    }
} // namespace Vk::Gfx
