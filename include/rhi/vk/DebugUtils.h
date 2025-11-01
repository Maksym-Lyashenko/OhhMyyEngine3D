#pragma once

#include <vulkan/vulkan.h>

#include <cstdarg>
#include <string>

namespace Vk
{

    inline PFN_vkSetDebugUtilsObjectNameEXT pfnSetName = nullptr;

    inline void init(VkDevice device)
    {
        pfnSetName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
    }

    inline void setName(VkDevice device, VkObjectType type, uint64_t handle, const char *name)
    {
        if (!pfnSetName || !handle || !name)
            return;
        VkDebugUtilsObjectNameInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        info.objectType = type;
        info.objectHandle = handle;
        info.pObjectName = name;
        pfnSetName(device, &info);
    }

    // helpers
    inline void nameBuffer(VkDevice d, VkBuffer b, const char *n) { setName(d, VK_OBJECT_TYPE_BUFFER, (uint64_t)b, n); }
    inline void nameImage(VkDevice d, VkImage i, const char *n) { setName(d, VK_OBJECT_TYPE_IMAGE, (uint64_t)i, n); }
    inline void nameImageView(VkDevice d, VkImageView v, const char *n) { setName(d, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)v, n); }
    inline void nameSampler(VkDevice d, VkSampler s, const char *n) { setName(d, VK_OBJECT_TYPE_SAMPLER, (uint64_t)s, n); }
    inline void nameDS(VkDevice d, VkDescriptorSet s, const char *n) { setName(d, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)s, n); }
    inline void namePipeline(VkDevice d, VkPipeline p, const char *n) { setName(d, VK_OBJECT_TYPE_PIPELINE, (uint64_t)p, n); }
    inline void nameFB(VkDevice d, VkFramebuffer f, const char *n) { setName(d, VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)f, n); }
    inline void nameRP(VkDevice d, VkRenderPass r, const char *n) { setName(d, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)r, n); }
    inline void nameCmdBuf(VkDevice d, VkCommandBuffer c, const char *n) { setName(d, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)c, n); }
    inline void nameFence(VkDevice d, VkFence f, const char *n) { setName(d, VK_OBJECT_TYPE_FENCE, (uint64_t)f, n); }
    inline void nameSemaphore(VkDevice d, VkSemaphore s, const char *n) { setName(d, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)s, n); }
    inline void nameSwapchain(VkDevice d, VkSwapchainKHR sc, const char *n) { setName(d, VK_OBJECT_TYPE_SWAPCHAIN_KHR, (uint64_t)sc, n); }

} // namespace Vk
