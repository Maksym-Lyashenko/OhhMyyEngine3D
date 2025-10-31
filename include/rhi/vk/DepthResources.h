#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Vk
{
    /**
     * @brief RAII wrapper for a depth attachment (image + allocation + view).
     *
     * Responsibilities:
     *  - Pick a supported depth format (with/without stencil).
     *  - Create VkImage via VMA, keep VmaAllocation, and create VkImageView.
     *  - Transition the image to VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
     *
     * Notes:
     *  - OPTIMAL tiling; usage includes DEPTH_STENCIL_ATTACHMENT_BIT.
     *  - Call recreate() on resize/MSAA change; old resources are freed first.
     */
    class DepthResources final
    {
    public:
        DepthResources() = default;
        ~DepthResources() { destroy(); }

        DepthResources(const DepthResources &) = delete;
        DepthResources &operator=(const DepthResources &) = delete;

        DepthResources(DepthResources &&other) noexcept { moveFrom(other); }
        DepthResources &operator=(DepthResources &&other) noexcept
        {
            if (this != &other)
            {
                destroy();
                moveFrom(other);
            }
            return *this;
        }

        /// Create depth resources. Finds a supported depth format internally.
        void create(VkPhysicalDevice physicalDevice,
                    VkDevice device,
                    VmaAllocator allocator,
                    VkExtent2D extent,
                    VkCommandPool commandPool,
                    VkQueue graphicsQueue,
                    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

        /// Destroy all Vulkan/VMA objects (safe to call multiple times).
        void destroy();

        /// Convenience recreate (destroy + create).
        void recreate(VkPhysicalDevice physicalDevice,
                      VkDevice device,
                      VmaAllocator allocator,
                      VkExtent2D extent,
                      VkCommandPool commandPool,
                      VkQueue graphicsQueue,
                      VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT)
        {
            destroy();
            create(physicalDevice, device, allocator, extent, commandPool, graphicsQueue, samples);
        }

        VkFormat getFormat() const noexcept { return format_; }
        VkImage getImage() const noexcept { return image_; }
        VkImageView getView() const noexcept { return view_; }

    private:
        // Owned handles
        VkDevice device_ = VK_NULL_HANDLE;
        VmaAllocator allocator_ = VK_NULL_HANDLE;
        VkImage image_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        VkImageView view_ = VK_NULL_HANDLE;

        // Metadata
        VkFormat format_ = VK_FORMAT_D32_SFLOAT;
        VkSampleCountFlagBits samples_ = VK_SAMPLE_COUNT_1_BIT;

        static bool hasStencil(VkFormat f) noexcept
        {
            return f == VK_FORMAT_D32_SFLOAT_S8_UINT || f == VK_FORMAT_D24_UNORM_S8_UINT;
        }

        /// Transition the depth image to DEPTH_STENCIL_ATTACHMENT_OPTIMAL using a one-time CB.
        void transitionToAttachment(VkCommandPool commandPool, VkQueue queue);

        /// Move-state helper.
        void moveFrom(DepthResources &other) noexcept
        {
            device_ = other.device_;
            other.device_ = VK_NULL_HANDLE;
            allocator_ = other.allocator_;
            other.allocator_ = VK_NULL_HANDLE;
            image_ = other.image_;
            other.image_ = VK_NULL_HANDLE;
            allocation_ = other.allocation_;
            other.allocation_ = VK_NULL_HANDLE;
            view_ = other.view_;
            other.view_ = VK_NULL_HANDLE;
            format_ = other.format_;
            samples_ = other.samples_;
        }
    };

} // namespace Vk
