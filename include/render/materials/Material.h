#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include "rhi/vk/gfx/Texture2D.h"
#include <glm/glm.hpp>

namespace Render
{

    // std140, 16-byte aligned
    struct MaterialParams
    {
        glm::vec4 baseColorFactor{1, 1, 1, 1};
        float metallicFactor{1.0f};
        float roughnessFactor{1.0f};
        float emissiveStrength{0.0f};
        glm::vec2 uvTiling{1.0f, 1.0f};
        glm::vec2 uvOffset{0.0f, 0.0f};
        uint32_t flags{0}; // bitmask: 1=albedo, 2=normal, 4=mr, 8=ao, 16=emissive
        uint32_t _pad[3]{};
    };

    struct MaterialDesc
    {
        // File paths (simple MVP; later we can accept prebuilt textures)
        std::string baseColorPath;
        std::string normalPath;
        std::string mrPath; // metallic-roughness in BG channels
        std::string occlusionPath;
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

        void create(VkPhysicalDevice phys, VkDevice dev,
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
        void createUbo(VkPhysicalDevice phys);
        void updateUbo(const MaterialParams &p);

        static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags props);

    private:
        VkPhysicalDevice phys_{VK_NULL_HANDLE};
        VkDevice device_{VK_NULL_HANDLE};

        // GPU
        VkBuffer ubo_{VK_NULL_HANDLE};
        VkDeviceMemory uboMem_{VK_NULL_HANDLE};
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
