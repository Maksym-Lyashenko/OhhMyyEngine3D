#include "render/LightManager.h"

#include "rhi/vk/DebugUtils.h"

#include <algorithm>
#include <array>

namespace Render
{
    void LightManager::init(VmaAllocator alloc, VkDevice device,
                            VkDescriptorSetLayout lightingSetLayout)
    {
        allocator_ = alloc;
        device_ = device;

        // --- 1. Create a small descriptor pool for the lighting set (set = 2)
        // It needs 1 UBO + 3 SSBOs.
        // Enough for 1 lighting set
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
        poolSizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3};

        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 1; // one lighting set
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool_));

        // Create buffers with some initial capacity; we will grow if needed.
        uboCounts_.create(allocator_, device_,
                          sizeof(LightingCountsUBO),
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          VMA_MEMORY_USAGE_CPU_TO_GPU,
                          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                              VMA_ALLOCATION_CREATE_MAPPED_BIT,
                          "LightingCountsUBO");

        ssboDir_.create(allocator_, device_, sizeof(DirectionalLightGPU) * 1,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VMA_MEMORY_USAGE_GPU_ONLY, 0, "SSBO_Directional");

        ssboPoint_.create(allocator_, device_, sizeof(PointLightGPU) * 1,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VMA_MEMORY_USAGE_GPU_ONLY, 0, "SSBO_Point");

        ssboSpot_.create(allocator_, device_, sizeof(SpotLightGPU) * 1,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VMA_MEMORY_USAGE_GPU_ONLY, 0, "SSBO_Spot");

        // Allocate descriptor set
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = pool_;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &lightingSetLayout;
        VK_CHECK(vkAllocateDescriptorSets(device_, &ai, &set_));

        // Write descriptors
        VkDescriptorBufferInfo uboInfo{uboCounts_.get(), 0, sizeof(LightingCountsUBO)};
        VkDescriptorBufferInfo dirInfo{ssboDir_.get(), 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo ptInfo{ssboPoint_.get(), 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo spInfo{ssboSpot_.get(), 0, VK_WHOLE_SIZE};

        std::array<VkWriteDescriptorSet, 4> writes{};
        writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
                     set_, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &uboInfo, nullptr};
        writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
                     set_, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &dirInfo, nullptr};
        writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
                     set_, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &ptInfo, nullptr};
        writes[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
                     set_, 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &spInfo, nullptr};

        vkUpdateDescriptorSets(device_, (uint32_t)writes.size(), writes.data(), 0, nullptr);
    }

    void LightManager::ensureBufferCapacity(Vk::Gfx::Buffer &buf,
                                            VkDeviceSize minBytes,
                                            VkBufferUsageFlags usage,
                                            const char *debugName)
    {
        // If current buffer size is less than required, re-create with a growth factor.
        VkDeviceSize curr = buf.size();
        if (curr >= minBytes)
            return;

        VkDeviceSize newSize = std::max(minBytes, curr * 2 + 1024); // grow
        buf.destroy();
        buf.create(allocator_, device_, newSize,
                   usage,
                   VMA_MEMORY_USAGE_GPU_ONLY, 0, debugName);
    }

    void LightManager::upload(VkCommandPool cmdPool, VkQueue queue,
                              const glm::vec3 &ambientRGB, uint32_t flags)
    {
        // (1) UBO counts/ambient
        LightingCountsUBO counts{};
        counts.counts_flags = glm::uvec4((uint32_t)dir.size(),
                                         (uint32_t)point.size(),
                                         (uint32_t)spot.size(),
                                         flags);
        counts.ambientRGBA = glm::vec4(ambientRGB, 0.0f);
        uboCounts_.upload(&counts, sizeof(counts));

        // (2) Directional lights -> SSBO
        if (!dir.empty())
        {
            VkDeviceSize bytes = sizeof(DirectionalLightGPU) * dir.size();
            ensureBufferCapacity(ssboDir_, bytes,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 "SSBO_Directional");
            // transient staging copy into GPU-only SSBO
            Vk::Gfx::Buffer staging;
            staging.create(allocator_, device_, bytes,
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VMA_MEMORY_USAGE_CPU_TO_GPU,
                           VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                               VMA_ALLOCATION_CREATE_MAPPED_BIT,
                           "SSBO_Directional Staging");
            staging.upload(dir.data(), (size_t)bytes);
            Vk::Gfx::Buffer::copyBuffer(device_, cmdPool, queue, staging.get(), ssboDir_.get(), bytes);
            staging.destroy();
        }

        // (3) Point lights -> SSBO
        if (!point.empty())
        {
            VkDeviceSize bytes = sizeof(PointLightGPU) * point.size();
            ensureBufferCapacity(ssboPoint_, bytes,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 "SSBO_Point");
            Vk::Gfx::Buffer staging;
            staging.create(allocator_, device_, bytes,
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VMA_MEMORY_USAGE_CPU_TO_GPU,
                           VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                               VMA_ALLOCATION_CREATE_MAPPED_BIT,
                           "SSBO_Point Staging");
            staging.upload(point.data(), (size_t)bytes);
            Vk::Gfx::Buffer::copyBuffer(device_, cmdPool, queue, staging.get(), ssboPoint_.get(), bytes);
            staging.destroy();
        }

        // (4) Spot lights -> SSBO
        if (!spot.empty())
        {
            VkDeviceSize bytes = sizeof(SpotLightGPU) * spot.size();
            ensureBufferCapacity(ssboSpot_, bytes,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 "SSBO_Spot");
            Vk::Gfx::Buffer staging;
            staging.create(allocator_, device_, bytes,
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VMA_MEMORY_USAGE_CPU_TO_GPU,
                           VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                               VMA_ALLOCATION_CREATE_MAPPED_BIT,
                           "SSBO_Spot Staging");
            staging.upload(spot.data(), (size_t)bytes);
            Vk::Gfx::Buffer::copyBuffer(device_, cmdPool, queue, staging.get(), ssboSpot_.get(), bytes);
            staging.destroy();
        }

        // Descriptors still valid (we didn't change buffer handles if capacity was enough).
        // If we re-created ssbo* buffers, we should re-write descriptors once:
        // (safe path) Always re-write the three storage buffer descriptors:
        VkDescriptorBufferInfo uboInfo{uboCounts_.get(), 0, sizeof(LightingCountsUBO)};
        VkDescriptorBufferInfo dirInfo{ssboDir_.get(), 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo ptInfo{ssboPoint_.get(), 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo spInfo{ssboSpot_.get(), 0, VK_WHOLE_SIZE};

        std::array<VkWriteDescriptorSet, 3> writes{};
        writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
                     set_, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &dirInfo, nullptr};
        writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
                     set_, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &ptInfo, nullptr};
        writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
                     set_, 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &spInfo, nullptr};
        vkUpdateDescriptorSets(device_, (uint32_t)writes.size(), writes.data(), 0, nullptr);
    }

    void LightManager::destroy()
    {
        ssboSpot_.destroy();
        ssboPoint_.destroy();
        ssboDir_.destroy();
        uboCounts_.destroy();

        if (pool_ != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(device_, pool_, nullptr);
            pool_ = VK_NULL_HANDLE;
        }

        set_ = VK_NULL_HANDLE;
        allocator_ = VK_NULL_HANDLE;
        device_ = VK_NULL_HANDLE;
    }

} // namespace Render
