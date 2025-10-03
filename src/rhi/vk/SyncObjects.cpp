#include "rhi/vk/SyncObjects.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

namespace Vk
{

    SyncObjects::SyncObjects(VkDevice device_, uint32_t imageCount_)
        : device(device_), imageCount(imageCount_)
    {
        create();
    }

    SyncObjects::~SyncObjects() noexcept
    {
        destroy();
    }

    void SyncObjects::reinit(uint32_t newImageCount)
    {
        if (newImageCount == imageCount && !imageAvailableSemaphores.empty())
            return; // nothing to do

        destroy();
        imageCount = newImageCount;
        create();
    }

    void SyncObjects::create()
    {
        imageAvailableSemaphores.resize(maxFramesInFlight, VK_NULL_HANDLE);
        inFlightFences.resize(maxFramesInFlight, VK_NULL_HANDLE);
        renderFinishedPerImage.resize(imageCount, VK_NULL_HANDLE);

        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

        // Start fences as signaled so the very first vkWaitForFences doesn't stall
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (std::size_t i = 0; i < maxFramesInFlight; ++i)
        {
            VK_CHECK(vkCreateSemaphore(device, &si, nullptr, &imageAvailableSemaphores[i]));
            VK_CHECK(vkCreateFence(device, &fi, nullptr, &inFlightFences[i]));
        }
        for (uint32_t i = 0; i < imageCount; ++i)
        {
            VK_CHECK(vkCreateSemaphore(device, &si, nullptr, &renderFinishedPerImage[i]));
        }

        Core::Logger::log(Core::LogLevel::INFO, "Sync objects created");
    }

    void SyncObjects::destroy() noexcept
    {
        // Destroy per-frame
        for (auto &s : imageAvailableSemaphores)
        {
            if (s != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device, s, nullptr);
                s = VK_NULL_HANDLE;
            }
        }
        for (auto &f : inFlightFences)
        {
            if (f != VK_NULL_HANDLE)
            {
                vkDestroyFence(device, f, nullptr);
                f = VK_NULL_HANDLE;
            }
        }

        // Destroy per-image
        for (auto &s : renderFinishedPerImage)
        {
            if (s != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device, s, nullptr);
                s = VK_NULL_HANDLE;
            }
        }

        imageAvailableSemaphores.clear();
        inFlightFences.clear();
        renderFinishedPerImage.clear();
    }

} // namespace Vk
