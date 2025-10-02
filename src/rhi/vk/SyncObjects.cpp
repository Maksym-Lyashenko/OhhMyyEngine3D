#include "rhi/vk/SyncObjects.h"

#include "core/Logger.h"
#include <rhi/vk/Common.h>

using namespace Core;

namespace Vk
{

    SyncObjects::SyncObjects(VkDevice device, uint32_t imageCount)
        : device(device), imageCount(imageCount)
    {
        create();
    }

    SyncObjects::~SyncObjects()
    {
        destroy();
    }

    void SyncObjects::create()
    {
        imageAvailableSemaphores.resize(maxFramesInFlight);
        inFlightFences.resize(maxFramesInFlight);
        renderFinishedPerImage.resize(imageCount); // per-image

        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fi.flags = VK_FENCE_CREATE_SIGNALED_BIT; // so that the first frame doesn't freeze

        for (size_t i = 0; i < maxFramesInFlight; ++i)
        {
            VK_CHECK(vkCreateSemaphore(device, &si, nullptr, &imageAvailableSemaphores[i]));
            VK_CHECK(vkCreateFence(device, &fi, nullptr, &inFlightFences[i]));
        }
        for (uint32_t i = 0; i < imageCount; ++i)
        {
            VK_CHECK(vkCreateSemaphore(device, &si, nullptr, &renderFinishedPerImage[i]));
        }

        Logger::log(LogLevel::INFO, "Sync objects created");
    }

    void SyncObjects::destroy()
    {
        // destroy in Renderer::cleanup() AFTER vkDeviceWaitIdle
        for (auto s : imageAvailableSemaphores)
        {
            if (s)
                vkDestroySemaphore(device, s, nullptr);
        }
        for (auto f : inFlightFences)
        {
            if (f)
                vkDestroyFence(device, f, nullptr);
        }
        for (auto s : renderFinishedPerImage)
        {
            if (s)
                vkDestroySemaphore(device, s, nullptr);
        }
    }

} // namespace Vk
