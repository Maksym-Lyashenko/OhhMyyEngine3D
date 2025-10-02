#include "rhi/vk/FrameRenderer.h"

#include "rhi/vk/RendererContext.h"
#include "rhi/vk/VulkanRenderer.h"

#include <rhi/vk/Common.h>

namespace Vk
{

    FrameRenderer::FrameRenderer(RendererContext &ctx,
                                 VulkanRenderer &vulkanRenderer)
        : ctx(ctx),
          vulkanRenderer(vulkanRenderer)
    {
        // init tab “image -> fence”
        imagesInFlight.resize(ctx.swapChain.getImages().size(), VK_NULL_HANDLE);
    }

    void FrameRenderer::drawFrame()
    {

        auto &device = ctx.device;
        auto &swapChain = ctx.swapChain;
        auto &syncObjects = ctx.syncObjects;
        auto &commandBuffers = ctx.commandBuffers;

        // 1) wait fence of current frame
        VkFence &inFlightFence = syncObjects.getInFlightFence(currentFrame);
        VK_CHECK(vkWaitForFences(device.getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX));

        // 2) acquire image
        uint32_t imageIndex = 0;
        VkResult result = vkAcquireNextImageKHR(
            device.getDevice(),
            swapChain.getSwapChain(),
            UINT64_MAX,
            syncObjects.getImageAvailableSemaphore(currentFrame),
            VK_NULL_HANDLE,
            &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            vulkanRenderer.markSwapchainDirty();
            return;
        }
        if (result == VK_SUBOPTIMAL_KHR)
        {
            vulkanRenderer.markSwapchainDirty();
        }
        else if (result != VK_SUCCESS)
        {
            VK_CHECK(result);
        }

        // wait fence if this image still in flight
        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
        {
            VK_CHECK(vkWaitForFences(device.getDevice(), 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX));
        }
        imagesInFlight[imageIndex] = inFlightFence;

        // reset fence
        VK_CHECK(vkResetFences(device.getDevice(), 1, &inFlightFence));

        // 3) submit
        VkSemaphore waitSemaphores[] = {syncObjects.getImageAvailableSemaphore(currentFrame)};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkCommandBuffer cmd = commandBuffers[imageIndex];
        VkSemaphore signalSemaphores[] = {syncObjects.getRenderFinishedSemaphoreForImage(imageIndex)};

        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VK_CHECK(vkQueueSubmit(device.getGraphicsQueue(), 1, &submitInfo, inFlightFence));

        // 4) present
        VkSwapchainKHR swapChains[] = {swapChain.getSwapChain()};
        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        VkResult resultPresent = vkQueuePresentKHR(device.getPresentQueue(), &presentInfo);

        if (resultPresent == VK_ERROR_OUT_OF_DATE_KHR)
        {
            vulkanRenderer.markSwapchainDirty();
            return;
        }
        if (resultPresent == VK_SUBOPTIMAL_KHR)
        {
            vulkanRenderer.markSwapchainDirty();
            // continue without throw
        }
        else if (resultPresent != VK_SUCCESS)
        {
            VK_CHECK(resultPresent);
        }

        // 5) next frame
        currentFrame = (currentFrame + 1) % syncObjects.getMaxFramesInFlight();
    }

} // namespace Vk
