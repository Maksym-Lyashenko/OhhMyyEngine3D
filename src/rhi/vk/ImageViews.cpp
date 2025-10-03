#include "rhi/vk/ImageViews.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

namespace Vk
{

    ImageViews::ImageViews(VkDevice device_,
                           const std::vector<VkImage> &swapChainImages_,
                           VkFormat swapChainImageFormat_)
        : device(device_), swapChainImages(swapChainImages_), swapChainImageFormat(swapChainImageFormat_)
    {
    }

    ImageViews::~ImageViews() noexcept
    {
        cleanup();
    }

    void ImageViews::create()
    {
        // idempotent: start clean
        cleanup();

        imageViews.resize(swapChainImages.size());

        for (std::size_t i = 0; i < swapChainImages.size(); ++i)
        {
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = swapChainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = swapChainImageFormat;

            // Explicit identity swizzle (good habit, some tools rely on this being sane)
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &imageViews[i]));
        }

        Core::Logger::log(Core::LogLevel::INFO,
                          "Created " + std::to_string(imageViews.size()) + " swapchain image views");
    }

    void ImageViews::cleanup() noexcept
    {
        if (imageViews.empty())
            return;

        for (auto view : imageViews)
        {
            if (view != VK_NULL_HANDLE)
            {
                vkDestroyImageView(device, view, nullptr);
            }
        }
        Core::Logger::log(Core::LogLevel::INFO,
                          "Destroyed " + std::to_string(imageViews.size()) + " image views");
        imageViews.clear();
    }

} // namespace Vk
