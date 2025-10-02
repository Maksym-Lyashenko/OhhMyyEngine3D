#include "rhi/vk/ImageViews.h"

#include "core/Logger.h"
#include <rhi/vk/Common.h>

using namespace Core;

namespace Vk
{
    ImageViews::ImageViews(VkDevice device,
                           const std::vector<VkImage> &swapChainImages,
                           VkFormat swapChainImageFormat)
        : device(device),
          swapChainImages(swapChainImages),
          swapChainImageFormat(swapChainImageFormat)
    {
    }

    ImageViews::~ImageViews()
    {
        cleanup();
    }

    void ImageViews::create()
    {
        cleanup(); // just in case re-create

        imageViews.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); ++i)
        {
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = swapChainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = swapChainImageFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &imageViews[i]));
        }

        Logger::log(LogLevel::INFO,
                    "Created " + std::to_string(imageViews.size()) + " swapchain image views");
    }

    void ImageViews::cleanup()
    {
        if (!imageViews.empty())
        {
            for (auto view : imageViews)
            {
                vkDestroyImageView(device, view, nullptr);
            }
            Logger::log(LogLevel::INFO,
                        "Destroyed " + std::to_string(imageViews.size()) + " image views");
            imageViews.clear();
        }
    }
} // namespace Vk
