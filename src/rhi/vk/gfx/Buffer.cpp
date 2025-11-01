#include "rhi/vk/gfx/Buffer.h"
#include "rhi/vk/Common.h" // VK_CHECK
#include "rhi/vk/DebugUtils.h"

#include <cstring> // std::memcpy>
#include <algorithm>

namespace Vk::Gfx
{
    void Buffer::create(VmaAllocator allocator,
                        VkDevice device,
                        VkDeviceSize sz,
                        VkBufferUsageFlags usage,
                        VmaMemoryUsage memUsage,
                        VmaAllocationCreateFlags allocFlags,
                        const char *debugName)
    {
        destroy();

        allocator_ = allocator;
        device_ = device;
        size_ = sz;

        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = sz;
        bi.usage = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo aci{};
        aci.usage = memUsage;
        aci.flags = allocFlags; // e.g. VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT

        VmaAllocationInfo info{};
        VK_CHECK(vmaCreateBuffer(allocator_, &bi, &aci, &buffer_, &allocation_, &info));

        // If created with MAPPED flag, VMA returns persistent mapping in info.pMappedData
        mapped_ = info.pMappedData;

        if (debugName && *debugName)
        {
            vmaSetAllocationName(allocator_, allocation_, debugName);
            nameBuffer(device_, buffer_, debugName);
        }
    }

    void Buffer::destroy() noexcept
    {
        if (buffer_)
        {
            vmaDestroyBuffer(allocator_, buffer_, allocation_);
            buffer_ = VK_NULL_HANDLE;
            allocation_ = VK_NULL_HANDLE;
        }
        device_ = VK_NULL_HANDLE;
        allocator_ = VK_NULL_HANDLE;
        size_ = 0;
        mapped_ = nullptr;
    }

    void *Buffer::map()
    {
        if (!allocation_)
            throw std::runtime_error("Buffer::map(): allocation is null");
        if (mapped_)
            return mapped_; // already mapped (persistent mapping)
        VK_CHECK(vmaMapMemory(allocator_, allocation_, &mapped_));
        return mapped_;
    }

    void Buffer::unmap()
    {
        if (allocation_ && mapped_)
        {
            vmaUnmapMemory(allocator_, allocation_);
            mapped_ = nullptr;
        }
    }

    void Buffer::upload(const void *data, size_t bytes, VkDeviceSize dstOffset)
    {
        if (!data || bytes == 0)
            return;
        if (dstOffset > size_)
            throw std::runtime_error("Buffer::upload(): offset out of range");
        const VkDeviceSize end = dstOffset + static_cast<VkDeviceSize>(bytes);
        if (end > size_)
            throw std::runtime_error("Buffer::upload(): write exceeds buffer size");

        // Map (persistent if created with MAPPED flag)
        void *ptr = map();
        std::memcpy(static_cast<std::byte *>(ptr) + dstOffset, data, bytes);
        // For non-coherent memory, VMA handles flushing when you use HOST_ACCESS flags;
        // otherwise, you would need vmaFlushAllocation (not required for HOST_COHERENT).
        // We keep it simple and rely on HOST_ACCESS flags for staging buffers.
    }

    void Buffer::copyBuffer(VkDevice device,
                            VkCommandPool cmdPool,
                            VkQueue queue,
                            VkBuffer src,
                            VkBuffer dst,
                            VkDeviceSize bytes)
    {
        // Allocate a one-time command buffer
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = cmdPool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateCommandBuffers(device, &ai, &cmd));

        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &bi));

        VkBufferCopy region{};
        region.srcOffset = 0;
        region.dstOffset = 0;
        region.size = bytes;
        vkCmdCopyBuffer(cmd, src, dst, 1, &region);

        VK_CHECK(vkEndCommandBuffer(cmd));

        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;

        VK_CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(queue));

        vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
    }

    void Buffer::createDeviceLocalWithData(VmaAllocator allocator,
                                           VkDevice device,
                                           VkCommandPool cmdPool,
                                           VkQueue queue,
                                           const void *data,
                                           VkDeviceSize bytes,
                                           VkBufferUsageFlags usage,
                                           const char *debugName)
    {
        // 1) Create GPU-only destination
        create(allocator, device, bytes, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, 0, debugName);

        // 2) Create transient staging buffer (host visible + persistently mapped)
        Buffer staging;
        std::string stagingName = (debugName && *debugName) ? (std::string(debugName) + " Staging") : std::string();
        staging.create(allocator, device, bytes,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VMA_MEMORY_USAGE_CPU_TO_GPU,
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                           VMA_ALLOCATION_CREATE_MAPPED_BIT,
                       stagingName.empty() ? nullptr : stagingName.c_str());
        staging.upload(data, static_cast<size_t>(bytes));

        // 3) Issue copy and drop staging
        copyBuffer(device, cmdPool, queue, staging.get(), get(), bytes);
        staging.destroy();
    }

} // namespace Vk::Gfx
