#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Core
{
    class ImageViews
    {
    public:
        ImageViews(VkDevice device,
                   const std::vector<VkImage> &swapChainImages,
                   VkFormat swapChainImageFormat);
        ~ImageViews();

        ImageViews(const ImageViews &) = delete;
        ImageViews &operator=(const ImageViews &) = delete;

        void create();
        void cleanup();

        const std::vector<VkImageView> &getViews() const { return imageViews; }

    private:
        VkDevice device{VK_NULL_HANDLE};
        std::vector<VkImage> swapChainImages;
        VkFormat swapChainImageFormat{};
        std::vector<VkImageView> imageViews;
    };
}
