#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <cstddef>

namespace Vk
{

    /**
     * @brief Creates one VkImageView per swapchain VkImage.
     *
     * Requirements:
     *  - The VkDevice and the swapchain images must outlive this object.
     *  - Recreate this after swapchain re-creation (extent/format/images may change).
     */
    class ImageViews
    {
    public:
        ImageViews(VkDevice device,
                   const std::vector<VkImage> &swapChainImages,
                   VkFormat swapChainImageFormat);
        ~ImageViews() noexcept;

        ImageViews(const ImageViews &) = delete;
        ImageViews &operator=(const ImageViews &) = delete;

        /// Destroy existing views and create new ones for the current images/format.
        void create();
        /// Destroy all image views (safe to call multiple times).
        void cleanup() noexcept;

        /// Convenience: destroy + update images/format + create.
        void recreate(const std::vector<VkImage> &images, VkFormat format)
        {
            updateImages(images, format);
            create();
        }

        const std::vector<VkImageView> &getViews() const noexcept { return imageViews; }
        VkImageView get(std::size_t i) const noexcept { return imageViews[i]; }
        std::size_t count() const noexcept { return imageViews.size(); }

        /// Update the source images/format without creating views (call create() after).
        void updateImages(const std::vector<VkImage> &images, VkFormat format)
        {
            swapChainImages = images;
            swapChainImageFormat = format;
        }

    private:
        VkDevice device{VK_NULL_HANDLE};
        std::vector<VkImage> swapChainImages;
        VkFormat swapChainImageFormat{};
        std::vector<VkImageView> imageViews;
    };

} // namespace Vk
