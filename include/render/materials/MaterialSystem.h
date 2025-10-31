#pragma once

#include <vulkan/vulkan.h>

#include <memory>

#include "Material.h"
#include "rhi/vk/gfx/Texture2D.h"

#include <vk_mem_alloc.h>

namespace Render
{

    /**
     * @brief TEMPORARY material hub: owns descriptor pool, layout (must match pipeline set=1),
     * and fallback textures. Creates Material instances on demand.
     *
     * Later we can move to a dedicated module, add caching and async loading.
     */
    class MaterialSystem
    {
    public:
        MaterialSystem() = default;
        ~MaterialSystem() { shutdown(); }

        void init(VmaAllocator allocator, VkDevice dev,
                  VkDescriptorSetLayout materialLayout,
                  uint32_t maxMaterials);

        void shutdown() noexcept;

        std::shared_ptr<Material> createMaterial(const MaterialDesc &desc);

        // Fallbacks (non-owning accessors)
        Vk::Gfx::Texture2D *white() const { return white_.get(); }
        Vk::Gfx::Texture2D *black() const { return black_.get(); }
        Vk::Gfx::Texture2D *flatNormal() const { return flatNormal_.get(); }

        VkDescriptorPool pool() const { return pool_; }
        VkDescriptorSetLayout layout() const { return layout_; }

        // TEMPORARY: expose cmd resources if needed by Texture2D
        void setUploadCmd(VkCommandPool pool, VkQueue queue);

    private:
        void createFallbacks();
        void destroyFallbacks();

    private:
        VmaAllocator allocator_{VK_NULL_HANDLE};
        VkDevice device_{VK_NULL_HANDLE};

        VkDescriptorPool pool_{VK_NULL_HANDLE};
        VkDescriptorSetLayout layout_{VK_NULL_HANDLE}; // same object as in pipeline (we don't create it here)

        // Upload helpers (TEMP): used if Texture2D path loaders need them
        VkCommandPool uploadPool_{VK_NULL_HANDLE};
        VkQueue uploadQueue_{VK_NULL_HANDLE};

        // 1×1 fallback textures
        std::unique_ptr<Vk::Gfx::Texture2D> white_;
        std::unique_ptr<Vk::Gfx::Texture2D> black_;
        std::unique_ptr<Vk::Gfx::Texture2D> flatNormal_;
    };

} // namespace Render
