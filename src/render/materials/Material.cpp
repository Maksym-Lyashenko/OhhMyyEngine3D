#include "render/materials/Material.h"
#include "rhi/vk/Common.h" // VK_CHECK + logger

#ifdef OME3D_USE_STB
// Texture2D::loadFromFile() already uses stb; we don't need to include here.
#endif

namespace Render
{

    void Material::createUbo()
    {
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = sizeof(MaterialParams);
        bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo aci{};
        aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        aci.usage = VMA_MEMORY_USAGE_AUTO;

        VmaAllocationInfo info{};
        VK_CHECK(vmaCreateBuffer(allocator_, &bi, &aci, &ubo_, &uboAlloc_, &info));
    }

    void Material::updateUbo(const MaterialParams &p)
    {
        void *mapped = nullptr;
        VK_CHECK(vmaMapMemory(allocator_, uboAlloc_, &mapped));
        std::memcpy(mapped, &p, sizeof(MaterialParams));
        vmaUnmapMemory(allocator_, uboAlloc_);
    }

    // --- main ---------------------------------------------------------------

    void Material::create(VmaAllocator allocator, VkDevice dev,
                          VkDescriptorPool pool, VkDescriptorSetLayout layout,
                          VkCommandPool uploadPool,
                          VkQueue uploadQueue,
                          const MaterialDesc &desc,
                          // fallback textures (not owned)
                          Vk::Gfx::Texture2D *white,
                          Vk::Gfx::Texture2D *flatNormal,
                          Vk::Gfx::Texture2D *black)
    {
        destroy();

        allocator_ = allocator;
        device_ = dev;

        // Load textures if paths are provided, otherwise use fallbacks.
        auto loadTex = [&](std::unique_ptr<Vk::Gfx::Texture2D> &dst,
                           const std::string &path,
                           VkFormat fmt)
        {
            if (path.empty())
                return false;
            dst = std::make_unique<Vk::Gfx::Texture2D>();
            dst->loadFromFile(allocator_, device_, /*cmdPool*/ uploadPool, /*queue*/ uploadQueue,
                              path, /*genMips*/ true, fmt); // NOTE: if you need cmdPool/queue, pass via MaterialSystem
            return true;
        };

        // IMPORTANT (TEMPORARY):
        // If your Texture2D::loadFromFile() requires a valid command pool/queue,
        // create materials via MaterialSystem that passes those in. For brevity,
        // this snippet assumes Texture2D has a file-based helper like your createFromRGBA8
        // variant. If not, refactor to call createFromRGBA8 with loaded pixels.

        // Bind views/samplers to fallback first
        albedoView_ = white->view();
        albedoSampler_ = white->sampler();
        normalView_ = flatNormal->view();
        normalSampler_ = flatNormal->sampler();
        mrView_ = black->view();
        mrSampler_ = black->sampler();
        aoView_ = black->view();
        aoSampler_ = black->sampler();
        emissiveView_ = black->view();
        emissiveSampler_ = black->sampler();

        uint32_t flags = 0;

        // BaseColor (sRGB)
        if (!desc.baseColorPath.empty())
        {
            baseColor_ = std::make_unique<Vk::Gfx::Texture2D>();
            baseColor_->loadFromFile(allocator_, device_,
                                     /*cmdPool*/ uploadPool, /*queue*/ uploadQueue,
                                     desc.baseColorPath, /*genMips*/ true, VK_FORMAT_R8G8B8A8_SRGB);
            albedoView_ = baseColor_->view();
            albedoSampler_ = baseColor_->sampler();
            flags |= 1u;
        }

        // Normal (UNORM)
        if (!desc.normalPath.empty())
        {
            normal_ = std::make_unique<Vk::Gfx::Texture2D>();
            normal_->loadFromFile(allocator_, device_, uploadPool, uploadQueue,
                                  desc.normalPath, true, VK_FORMAT_R8G8B8A8_UNORM);
            normalView_ = normal_->view();
            normalSampler_ = normal_->sampler();
            flags |= 2u;
        }

        // MetallicRoughness (UNORM) — B=metallic, G=roughness
        if (!desc.mrPath.empty())
        {
            mr_ = std::make_unique<Vk::Gfx::Texture2D>();
            mr_->loadFromFile(allocator_, device_, uploadPool, uploadQueue,
                              desc.mrPath, true, VK_FORMAT_R8G8B8A8_UNORM);
            mrView_ = mr_->view();
            mrSampler_ = mr_->sampler();
            flags |= 4u;
        }

        // AO (UNORM)
        if (!desc.occlusionPath.empty())
        {
            occlusion_ = std::make_unique<Vk::Gfx::Texture2D>();
            occlusion_->loadFromFile(allocator_, device_, uploadPool, uploadQueue,
                                     desc.occlusionPath, true, VK_FORMAT_R8G8B8A8_UNORM);
            aoView_ = occlusion_->view();
            aoSampler_ = occlusion_->sampler();
            flags |= 8u;
        }

        // Emissive (sRGB)
        if (!desc.emissivePath.empty())
        {
            emissive_ = std::make_unique<Vk::Gfx::Texture2D>();
            emissive_->loadFromFile(allocator_, device_, uploadPool, uploadQueue,
                                    desc.emissivePath, true, VK_FORMAT_R8G8B8A8_SRGB);
            emissiveView_ = emissive_->view();
            emissiveSampler_ = emissive_->sampler();
            flags |= 16u;
        }

        // UBO
        createUbo();
        MaterialParams params = desc.params;
        params.flags = flags;
        updateUbo(params);

        // Allocate descriptor set (set = 1)
        VkDescriptorSetAllocateInfo dsAi{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dsAi.descriptorPool = pool;
        dsAi.descriptorSetCount = 1;
        dsAi.pSetLayouts = &layout;

        VK_CHECK(vkAllocateDescriptorSets(device_, &dsAi, &set_));

        // Write descriptors (bindings 0..5)
        VkDescriptorImageInfo i0{albedoSampler_, albedoView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo i1{normalSampler_, normalView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo i2{mrSampler_, mrView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo i3{aoSampler_, aoView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo i4{emissiveSampler_, emissiveView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        VkDescriptorBufferInfo b5{};
        b5.buffer = ubo_;
        b5.offset = 0;
        b5.range = sizeof(MaterialParams);

        VkWriteDescriptorSet writes[6]{};
        auto W = [&](uint32_t idx, uint32_t binding, VkDescriptorType t, const void *info)
        {
            writes[idx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[idx].dstSet = set_;
            writes[idx].dstBinding = binding;
            writes[idx].dstArrayElement = 0;
            writes[idx].descriptorType = t;
            writes[idx].descriptorCount = 1;
            if (t == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                writes[idx].pImageInfo = reinterpret_cast<const VkDescriptorImageInfo *>(info);
            else
                writes[idx].pBufferInfo = reinterpret_cast<const VkDescriptorBufferInfo *>(info);
        };

        W(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &i0);
        W(1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &i1);
        W(2, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &i2);
        W(3, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &i3);
        W(4, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &i4);
        W(5, 5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &b5);

        vkUpdateDescriptorSets(device_, 6, writes, 0, nullptr);
    }

    void Material::destroy() noexcept
    {
        if (!device_)
            return;

        if (ubo_)
        {
            vmaDestroyBuffer(allocator_, ubo_, uboAlloc_);
            ubo_ = VK_NULL_HANDLE;
            uboAlloc_ = VK_NULL_HANDLE;
        }

        baseColor_.reset();
        normal_.reset();
        mr_.reset();
        occlusion_.reset();
        emissive_.reset();

        set_ = VK_NULL_HANDLE; // belongs to pool, freed with pool destroy

        device_ = VK_NULL_HANDLE;
        allocator_ = VK_NULL_HANDLE;
    }

} // namespace Render
