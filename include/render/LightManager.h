#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <string>

#include "rhi/vk/memoryManager/VulkanAllocator.h"
#include "rhi/vk/gfx/Buffer.h"
#include "render/LightingGPU.h"

namespace Render
{

    class LightManager
    {
    public:
        // CPU-side arrays you manipulate from Scene/Renderer
        std::vector<DirectionalLightGPU> dir;
        std::vector<PointLightGPU> point;
        std::vector<SpotLightGPU> spot;

        // Init/destroy GPU resources
        void init(VmaAllocator alloc, VkDevice device,
                  VkDescriptorSetLayout lightingSetLayout);
        void destroy();

        // Upload GPU arrays to GPUbuffers + update UBO counts/ambient/flags
        void upload(VkCommandPool cmdPool, VkQueue queue,
                    const glm::vec3 &ambientRGB,
                    uint32_t flags);

        // Descriptor set (bind this at set=2)
        VkDescriptorSet lightingSet() const { return set_; }

    private:
        // GPU Resources
        VmaAllocator allocator_{VK_NULL_HANDLE};
        VkDevice device_{VK_NULL_HANDLE};
        VkDescriptorPool pool_{VK_NULL_HANDLE};
        VkDescriptorSet set_{VK_NULL_HANDLE};

        Vk::Gfx::Buffer uboCounts_;
        Vk::Gfx::Buffer ssboDir_;
        Vk::Gfx::Buffer ssboPoint_;
        Vk::Gfx::Buffer ssboSpot_;

        // helper to (re)alloc SSBO of a least "minBytes"
        void ensureBufferCapacity(Vk::Gfx::Buffer &buf,
                                  VkDeviceSize minBytes,
                                  VkBufferUsageFlags usage,
                                  const char *debugName);
    };

} // namespace Render
