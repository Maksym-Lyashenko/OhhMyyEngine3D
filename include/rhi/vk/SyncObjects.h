#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace Vk
{

    class SyncObjects
    {
    public:
        static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

        // Create imageCount to allocate semaphores for every image
        SyncObjects(VkDevice device, uint32_t imageCount);
        ~SyncObjects();

        SyncObjects(const SyncObjects &) = delete;
        SyncObjects &operator=(const SyncObjects &) = delete;

        // Per-frame
        VkFence &getInFlightFence(size_t frame) { return inFlightFences[frame]; }
        VkSemaphore getImageAvailableSemaphore(size_t frame) const { return imageAvailableSemaphores[frame]; }

        // Per-image
        VkSemaphore getRenderFinishedSemaphoreForImage(uint32_t imageIndex) const { return renderFinishedPerImage[imageIndex]; }

        size_t getMaxFramesInFlight() const { return maxFramesInFlight; }
        uint32_t getImageCount() const { return imageCount; }

    private:
        VkDevice device{};
        size_t maxFramesInFlight{MAX_FRAMES_IN_FLIGHT};
        uint32_t imageCount{0};

        // per-frame
        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkFence> inFlightFences;

        // per-image
        std::vector<VkSemaphore> renderFinishedPerImage;

        void create();
        void destroy();
    };

} // namespace Vk
