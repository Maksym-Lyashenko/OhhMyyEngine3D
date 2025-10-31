#include "render/materials/MaterialSystem.h"
#include "rhi/vk/Common.h"

namespace Render
{

    void MaterialSystem::init(VmaAllocator allocator, VkDevice dev,
                              VkDescriptorSetLayout materialLayout,
                              uint32_t maxMaterials)
    {
        shutdown();

        allocator_ = allocator;
        device_ = dev;
        layout_ = materialLayout; // pipeline owns creation; we just store it

        // Descriptor pool: enough for N materials (5 CIS + 1 UBO each)
        VkDescriptorPoolSize sizes[2]{};
        sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sizes[0].descriptorCount = maxMaterials * 5;
        sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        sizes[1].descriptorCount = maxMaterials * 1;

        VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        ci.maxSets = maxMaterials;
        ci.poolSizeCount = 2;
        ci.pPoolSizes = sizes;

        VK_CHECK(vkCreateDescriptorPool(device_, &ci, nullptr, &pool_));

        createFallbacks();
    }

    void MaterialSystem::shutdown() noexcept
    {
        destroyFallbacks();

        if (pool_)
        {
            vkDestroyDescriptorPool(device_, pool_, nullptr);
            pool_ = VK_NULL_HANDLE;
        }

        layout_ = VK_NULL_HANDLE;
        device_ = VK_NULL_HANDLE;
        allocator_ = VK_NULL_HANDLE;
    }

    std::shared_ptr<Material> MaterialSystem::createMaterial(const MaterialDesc &desc)
    {
        if (uploadPool_ == VK_NULL_HANDLE || uploadQueue_ == VK_NULL_HANDLE)
        {
            throw std::runtime_error("MaterialSystem: upload pool/queue not set");
        }

        auto mat = std::make_shared<Material>();
        mat->create(allocator_, device_,
                    /*descPool*/ pool_,
                    /*layout*/ layout_,
                    /*upload*/ uploadPool_,
                    uploadQueue_,
                    desc,
                    white_.get(), flatNormal_.get(), black_.get());
        return mat;
    }

    void MaterialSystem::setUploadCmd(VkCommandPool pool, VkQueue queue)
    {

        uploadPool_ = pool;
        uploadQueue_ = queue;
    }

    void MaterialSystem::createFallbacks()
    {
        // Safety: must be set via setUploadCmd() before init()
        if (uploadPool_ == VK_NULL_HANDLE || uploadQueue_ == VK_NULL_HANDLE)
        {
            throw std::runtime_error("MaterialSystem: upload pool/queue not set (call setUploadCmd before init)");
        }

        // 1×1 RGBA pixels
        const uint8_t whitePix[4] = {255, 255, 255, 255};
        const uint8_t blackPix[4] = {0, 0, 0, 255};
        const uint8_t flatNormalPix[4] = {128, 128, 255, 255}; // (0.5,0.5,1)

        white_ = std::make_unique<Vk::Gfx::Texture2D>();
        black_ = std::make_unique<Vk::Gfx::Texture2D>();
        flatNormal_ = std::make_unique<Vk::Gfx::Texture2D>();

        white_->createFromRGBA8(allocator_, device_, uploadPool_, uploadQueue_, whitePix, 1, 1, true, VK_FORMAT_R8G8B8A8_SRGB);
        black_->createFromRGBA8(allocator_, device_, uploadPool_, uploadQueue_, blackPix, 1, 1, true, VK_FORMAT_R8G8B8A8_UNORM);
        flatNormal_->createFromRGBA8(allocator_, device_, uploadPool_, uploadQueue_, flatNormalPix, 1, 1, true, VK_FORMAT_R8G8B8A8_UNORM);
    }

    void MaterialSystem::destroyFallbacks()
    {
        flatNormal_.reset();
        black_.reset();
        white_.reset();
    }

} // namespace Render
