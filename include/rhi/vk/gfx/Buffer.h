#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <cstdint>
#include <stdexcept>

namespace Vk::Gfx
{
    /**
     * @brief RAII wrapper for a Vulkan buffer allocated with VMA.
     *
     * Highlights:
     *  - No manual vkAllocateMemory / memory type picking.
     *  - Optional host mapping via vmaMapMemory (when HOST_VISIBLE).
     *  - Helpers to create device-local buffers with a staging upload.
     *
     * Not copyable, but movable.
     */
    class Buffer
    {
    public:
        Buffer() = default;
        ~Buffer() noexcept { destroy(); }

        Buffer(const Buffer &) = delete;
        Buffer &operator=(const Buffer &) = delete;

        Buffer(Buffer &&other) noexcept { moveFrom(other); }
        Buffer &operator=(Buffer &&other) noexcept
        {
            if (this != &other)
            {
                destroy();
                moveFrom(other);
            }
            return *this;
        }

        /**
         * @brief Creates a buffer via VMA.
         * @param allocator   VMA allocator
         * @param device      VkDevice (kept for commands where needed)
         * @param size        Buffer size in bytes
         * @param usage       VkBufferUsageFlags (e.g. VERTEX|TRANSFER_DST)
         * @param memUsage    VMA memory usage hint (e.g. VMA_MEMORY_USAGE_GPU_ONLY)
         * @param allocFlags  Optional VMA allocation flags (e.g. HOST_ACCESS_* | MAPPED)
         */
        void create(VmaAllocator allocator,
                    VkDevice device,
                    VkDeviceSize size,
                    VkBufferUsageFlags usage,
                    VmaMemoryUsage memUsage,
                    VmaAllocationCreateFlags allocFlags = 0);

        /// Destroys the buffer + VMA allocation (safe to call multiple times).
        void destroy() noexcept;

        /// Returns the VkBuffer handle.
        [[nodiscard]] VkBuffer get() const noexcept { return buffer_; }
        /// Returns the size in bytes.
        [[nodiscard]] VkDeviceSize size() const noexcept { return size_; }
        /// Returns the VkDevice this buffer was created with.
        [[nodiscard]] VkDevice device() const noexcept { return device_; }
        /// Returns the VMA allocation (may be useful for advanced queries).
        [[nodiscard]] VmaAllocation allocation() const noexcept { return allocation_; }

        /**
         * @brief Maps the allocation and returns a pointer (only if HOST_VISIBLE).
         *        Unmap with unmap(). Safe to call multiple times; returns the same mapping.
         */
        void *map();
        /// Unmaps the allocation (no-op if not mapped/host-visible).
        void unmap();

        /**
         * @brief Uploads data to the buffer (requires host-visible allocation).
         *        If allocation was created with MAPPED flag, this will just memcpy.
         */
        void upload(const void *data, size_t bytes, VkDeviceSize dstOffset = 0);

        /**
         * @brief One-shot helper to copy entire buffer contents using a one-time command buffer.
         *        Requires src.size() <= dst.size() and dst has TRANSFER_DST usage.
         */
        static void copyBuffer(VkDevice device,
                               VkCommandPool cmdPool,
                               VkQueue queue,
                               VkBuffer src,
                               VkBuffer dst,
                               VkDeviceSize bytes);

        /**
         * @brief Convenience: create a GPU-only buffer and fill it via staging.
         *        Internally creates a transient host buffer, uploads, copies, then frees it.
         *
         * @param allocator   VMA allocator
         * @param device      VkDevice
         * @param cmdPool     Command pool for the one-time CB
         * @param queue       Graphics/transfer queue for submission
         * @param data        Source data pointer
         * @param bytes       Data size
         * @param usage       Destination buffer usage flags (must include TRANSFER_DST if upload is needed)
         */
        void createDeviceLocalWithData(VmaAllocator allocator,
                                       VkDevice device,
                                       VkCommandPool cmdPool,
                                       VkQueue queue,
                                       const void *data,
                                       VkDeviceSize bytes,
                                       VkBufferUsageFlags usage);

    private:
        void moveFrom(Buffer &other) noexcept
        {
            device_ = other.device_;
            other.device_ = VK_NULL_HANDLE;
            allocator_ = other.allocator_;
            other.allocator_ = VK_NULL_HANDLE;
            buffer_ = other.buffer_;
            other.buffer_ = VK_NULL_HANDLE;
            allocation_ = other.allocation_;
            other.allocation_ = VK_NULL_HANDLE;
            size_ = other.size_;
            other.size_ = 0;
            mapped_ = other.mapped_;
            other.mapped_ = nullptr;
        }

        VkDevice device_{VK_NULL_HANDLE};
        VmaAllocator allocator_{VK_NULL_HANDLE};
        VkBuffer buffer_{VK_NULL_HANDLE};
        VmaAllocation allocation_{VK_NULL_HANDLE};
        VkDeviceSize size_{0};
        void *mapped_{nullptr};
    };

} // namespace Vk::Gfx
