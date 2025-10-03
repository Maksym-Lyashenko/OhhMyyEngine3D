#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace Vk::Gfx
{

    /**
     * @brief Simple RAII Vulkan buffer + device memory pair.
     *
     * Lifetime:
     *   - create(...)
     *   - optional upload(...) if memory is HOST_VISIBLE
     *   - destroy() (also called by destructor)
     *
     * Notes:
     *   - Not copyable, but movable.
     *   - upload() validates bounds and requires HOST_VISIBLE memory.
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

        /// Create a Vulkan buffer and allocate/bind its memory.
        void create(VkPhysicalDevice phys, VkDevice dev,
                    VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags props);

        /// Destroy buffer + memory (safe to call multiple times).
        void destroy() noexcept;

        /// Upload data to mapped memory (offset = 0). Requires HOST_VISIBLE memory.
        void upload(const void *data, size_t bytes);

        /// Upload with explicit byte offset. Requires HOST_VISIBLE memory.
        void upload(VkDeviceSize offset, const void *data, size_t bytes);

        [[nodiscard]] VkBuffer get() const noexcept { return buffer_; }
        [[nodiscard]] VkDeviceSize getSize() const noexcept { return size_; }
        [[nodiscard]] VkDevice device() const noexcept { return device_; }

    private:
        static uint32_t findMemoryType(VkPhysicalDevice phys,
                                       uint32_t typeFilter,
                                       VkMemoryPropertyFlags props);

        void moveFrom(Buffer &other) noexcept
        {
            device_ = other.device_;
            other.device_ = VK_NULL_HANDLE;
            buffer_ = other.buffer_;
            other.buffer_ = VK_NULL_HANDLE;
            memory_ = other.memory_;
            other.memory_ = VK_NULL_HANDLE;
            size_ = other.size_;
            other.size_ = 0;
            props_ = other.props_;
            other.props_ = 0;
        }

        VkDevice device_{VK_NULL_HANDLE};
        VkBuffer buffer_{VK_NULL_HANDLE};
        VkDeviceMemory memory_{VK_NULL_HANDLE};
        VkDeviceSize size_{0};
        VkMemoryPropertyFlags props_{0};
    };

} // namespace Vk::Gfx
