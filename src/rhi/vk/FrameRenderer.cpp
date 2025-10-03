#include "rhi/vk/FrameRenderer.h"

#include "rhi/vk/RendererContext.h"
#include "rhi/vk/VulkanRenderer.h"

#include "rhi/vk/Common.h" // VK_CHECK, etc.

#include <array>

namespace Vk
{

    FrameRenderer::FrameRenderer(RendererContext &ctx_, VulkanRenderer &vulkanRenderer_)
        : ctx(ctx_), vulkanRenderer(vulkanRenderer_)
    {
        // Map swapchain image -> fence that currently owns it (or VK_NULL_HANDLE if free).
        imagesInFlight.resize(ctx.swapChain.getImages().size(), VK_NULL_HANDLE);
    }

    void FrameRenderer::drawFrame()
    {
        auto &device = ctx.device;
        auto &swapChain = ctx.swapChain;
        auto &syncObjects = ctx.syncObjects;
        auto &commandBuffers = ctx.commandBuffers;

        // 1) Wait for the fence of the current frame (CPU/GPU sync for "frames in flight")
        VkFence &inFlightFence = syncObjects.getInFlightFence(currentFrame);
        VK_CHECK(vkWaitForFences(device.getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX));

        // 2) Acquire next swapchain image
        uint32_t imageIndex = 0;
        VkResult acquireRes = vkAcquireNextImageKHR(
            device.getDevice(),
            swapChain.getSwapChain(),
            UINT64_MAX,
            syncObjects.getImageAvailableSemaphore(currentFrame), // signaled when image is ready
            VK_NULL_HANDLE,
            &imageIndex);

        if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // Surface changed (resize, etc.) — trigger swapchain recreation.
            vulkanRenderer.markSwapchainDirty();
            return;
        }
        if (acquireRes == VK_SUBOPTIMAL_KHR)
        {
            // Still usable, but should recreate soon.
            vulkanRenderer.markSwapchainDirty();
        }
        else if (acquireRes != VK_SUCCESS)
        {
            // Any other error — escalate via VK_CHECK to keep diagnostics consistent.
            VK_CHECK(acquireRes);
        }

        // If this image is already in use by an earlier frame, wait for that frame to finish.
        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
        {
            VK_CHECK(vkWaitForFences(device.getDevice(), 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX));
        }
        // From now on, this image will be guarded by the current frame's fence.
        imagesInFlight[imageIndex] = inFlightFence;

        // Reset the fence for the current frame before submitting new work to the queue.
        VK_CHECK(vkResetFences(device.getDevice(), 1, &inFlightFence));

        // 3) Submit the recorded command buffer for this image
        const VkCommandBuffer cmd = commandBuffers[imageIndex];

        const std::array<VkSemaphore, 1> waitSemaphores{syncObjects.getImageAvailableSemaphore(currentFrame)};
        const std::array<VkPipelineStageFlags, 1> waitStages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        const std::array<VkSemaphore, 1> signalSemaphores{syncObjects.getRenderFinishedSemaphoreForImage(imageIndex)};

        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
        submitInfo.pWaitSemaphores = waitSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages.data();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
        submitInfo.pSignalSemaphores = signalSemaphores.data();

        VK_CHECK(vkQueueSubmit(device.getGraphicsQueue(), 1, &submitInfo, inFlightFence));

        // 4) Present
        const std::array<VkSwapchainKHR, 1> swapChains{swapChain.getSwapChain()};

        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
        presentInfo.pWaitSemaphores = signalSemaphores.data();
        presentInfo.swapchainCount = static_cast<uint32_t>(swapChains.size());
        presentInfo.pSwapchains = swapChains.data();
        presentInfo.pImageIndices = &imageIndex;

        VkResult presentRes = vkQueuePresentKHR(device.getPresentQueue(), &presentInfo);
        if (presentRes == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // Presenting failed due to outdated swapchain — recreate.
            vulkanRenderer.markSwapchainDirty();
            return;
        }
        if (presentRes == VK_SUBOPTIMAL_KHR)
        {
            // Usable but suboptimal — flag for recreation.
            vulkanRenderer.markSwapchainDirty();
        }
        else if (presentRes != VK_SUCCESS)
        {
            VK_CHECK(presentRes);
        }

        // 5) Advance frame index (ring-buffer over max frames in flight)
        currentFrame = (currentFrame + 1) % syncObjects.getMaxFramesInFlight();
    }

} // namespace Vk
