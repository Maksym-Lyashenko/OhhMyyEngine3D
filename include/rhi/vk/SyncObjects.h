#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace Vk
{

    /**
     * @brief Synchronization bundle for rendering:
     *  - Per-frame: imageAvailable semaphore + inFlight fence
     *  - Per-image: renderFinished semaphore (one per swapchain image)
     *
     * Notes:
     *  - Fences start in SIGNALED state to avoid stalling on the first frame.
     *  - If swapchain image count changes, call reinit(newImageCount) or create a new SyncObjects.
     */
    class SyncObjects
    {
    public:
        static constexpr std::size_t MAX_FRAMES_IN_FLIGHT = 2;

        /// Create semaphores/fences for the given swapchain imageCount.
        SyncObjects(VkDevice device, uint32_t imageCount);
        ~SyncObjects() noexcept;

        SyncObjects(const SyncObjects &) = delete;
        SyncObjects &operator=(const SyncObjects &) = delete;

        /// Recreate internal semaphores/fences for a new image count.
        void reinit(uint32_t newImageCount);

        // Per-frame accessors
        VkFence &getInFlightFence(std::size_t frame) noexcept { return inFlightFences[frame]; }
        VkSemaphore getImageAvailableSemaphore(std::size_t frame) const noexcept { return imageAvailableSemaphores[frame]; }

        // Per-image accessor
        VkSemaphore getRenderFinishedSemaphoreForImage(uint32_t imageIndex) const noexcept { return renderFinishedPerImage[imageIndex]; }

        std::size_t getMaxFramesInFlight() const noexcept { return maxFramesInFlight; }
        uint32_t getImageCount() const noexcept { return imageCount; }

    private:
        VkDevice device{VK_NULL_HANDLE};
        std::size_t maxFramesInFlight{MAX_FRAMES_IN_FLIGHT};
        uint32_t imageCount{0};

        // Per-frame
        std::vector<VkSemaphore> imageAvailableSemaphores; // signaled when image is acquired
        std::vector<VkFence> inFlightFences;               // GPU work completion for each frame-in-flight

        // Per-image
        std::vector<VkSemaphore> renderFinishedPerImage; // signaled when rendering of a specific image is done

        void create();
        void destroy() noexcept;
    };

} // namespace Vk
