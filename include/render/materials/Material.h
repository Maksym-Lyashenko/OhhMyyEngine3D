#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <string>

#include "rhi/vk/gfx/Texture2D.h"

#include <vk_mem_alloc.h>
#include <glm/glm.hpp>

namespace Render
{

    // std140, 16-byte aligned
    struct alignas(16) MaterialParams
    {
        glm::vec4 baseColorFactor; //  0..15

        float metallicFactor;   // 16
        float roughnessFactor;  // 20
        float emissiveStrength; // 24
        float _pad0;            // 28  <-- добивка до 32 (std140)

        glm::vec2 uvTiling; // 32..39
        glm::vec2 uvOffset; // 40..47

        uint32_t flags;    // 48..51
        uint32_t _pad1[3]; // 52..63  (итого 64 байта)
    };

    static_assert(offsetof(Render::MaterialParams, baseColorFactor) == 0, "baseColorFactor");
    static_assert(offsetof(Render::MaterialParams, metallicFactor) == 16, "metallicFactor");
    static_assert(offsetof(Render::MaterialParams, roughnessFactor) == 20, "roughnessFactor");
    static_assert(offsetof(Render::MaterialParams, emissiveStrength) == 24, "emissiveStrength");
    static_assert(offsetof(Render::MaterialParams, uvTiling) == 32, "uvTiling");
    static_assert(offsetof(Render::MaterialParams, uvOffset) == 40, "uvOffset");
    static_assert(offsetof(Render::MaterialParams, flags) == 48, "flags");
    static_assert(sizeof(Render::MaterialParams) == 64, "MaterialParams must be 64 bytes (std140)");

    struct MaterialDesc
    {
        // File paths (simple MVP; later we can accept prebuilt textures)
        std::string baseColorPath;
        std::string normalPath;
        std::string mrPath;        // metallic-roughness (glTF-style: B=metallic, G=roughness)
        std::string metallicPath;  // optional single-channel metallic
        std::string roughnessPath; // optional single-channel roughness
        std::string occlusionPath;
        std::string heightPath; // optional height (for POM/parallax) — пока просто храним
        std::string emissivePath;
        MaterialParams params{};
    };

    /**
     * @brief Owns textures + tiny UBO and a ready-to-bind descriptor set (set = 1).
     *
     * TEMPORARY: This class allocates descriptor sets directly from a pool passed in
     * by MaterialSystem. In the future we may want a central allocator or bindless.
     */
    class Material
    {
    public:
        Material() = default;
        ~Material() { destroy(); }

        Material(const Material &) = delete;
        Material &operator=(const Material &) = delete;

        void create(VmaAllocator allocator, VkDevice dev,
                    VkDescriptorPool pool, VkDescriptorSetLayout layout,
                    VkCommandPool uploadPool,
                    VkQueue uploadQueue,
                    const MaterialDesc &desc,
                    // fallback textures (not owned)
                    Vk::Gfx::Texture2D *white,
                    Vk::Gfx::Texture2D *flatNormal,
                    Vk::Gfx::Texture2D *black);

        void destroy() noexcept;

        VkDescriptorSet descriptorSet() const noexcept { return set_; }

    private:
        void createUbo();
        void updateUbo(const MaterialParams &p);

    private:
        VmaAllocator allocator_{VK_NULL_HANDLE};
        VkDevice device_{VK_NULL_HANDLE};

        // GPU
        VkBuffer ubo_{VK_NULL_HANDLE};
        VmaAllocation uboAlloc_{VK_NULL_HANDLE};
        VkDescriptorSet set_{VK_NULL_HANDLE};

        // Owned textures (may be nullptr if we used fallbacks)
        std::unique_ptr<Vk::Gfx::Texture2D> baseColor_;
        std::unique_ptr<Vk::Gfx::Texture2D> normal_;
        std::unique_ptr<Vk::Gfx::Texture2D> mr_;
        std::unique_ptr<Vk::Gfx::Texture2D> occlusion_;
        std::unique_ptr<Vk::Gfx::Texture2D> emissive_;

        // Bound texture views/samplers (either owned textures or fallbacks)
        VkImageView albedoView_{VK_NULL_HANDLE};
        VkSampler albedoSampler_{VK_NULL_HANDLE};
        VkImageView normalView_{VK_NULL_HANDLE};
        VkSampler normalSampler_{VK_NULL_HANDLE};
        VkImageView mrView_{VK_NULL_HANDLE};
        VkSampler mrSampler_{VK_NULL_HANDLE};
        VkImageView aoView_{VK_NULL_HANDLE};
        VkSampler aoSampler_{VK_NULL_HANDLE};
        VkImageView emissiveView_{VK_NULL_HANDLE};
        VkSampler emissiveSampler_{VK_NULL_HANDLE};
    };

} // namespace Render
