#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vk_mem_alloc.h>

namespace Vk::Gfx
{
    /**
     * @brief A 2D texture wrapper that handles image creation, upload, mip generation,
     * and destruction using Vulkan Memory Allocator (VMA).
     *
     * Provides helpers for both direct RGBA8 data upload and file-based loading
     * via stb_image (if OME3D_USE_STB is defined).
     */
    class Texture2D
    {
    public:
        /**
         * @brief Create a 2D texture directly from raw RGBA8 pixel data.
         * @param allocator   The VMA allocator used for GPU memory.
         * @param device      The logical Vulkan device.
         * @param cmdPool     A command pool from the graphics queue family.
         * @param graphicsQueue Graphics queue used for copy and layout transitions.
         * @param pixels      Pointer to raw pixel data (4 bytes per pixel, row-major).
         * @param w, h        Image dimensions in pixels.
         * @param generateMips Whether to generate mipmaps on the GPU.
         * @param format      The Vulkan image format (default: VK_FORMAT_R8G8B8A8_UNORM).
         */

        ~Texture2D() { destroy(); }

        Texture2D() = default;
        Texture2D(const Texture2D &) = delete;
        Texture2D &operator=(const Texture2D &) = delete;

        Texture2D(Texture2D &&other) noexcept { *this = std::move(other); }
        Texture2D &operator=(Texture2D &&other) noexcept
        {
            if (this != &other)
            {
                destroy();
                device_ = other.device_;
                allocator_ = other.allocator_;
                image_ = other.image_;
                imageAlloc_ = other.imageAlloc_;
                imageView_ = other.imageView_;
                sampler_ = other.sampler_;
                format_ = other.format_;
                width_ = other.width_;
                height_ = other.height_;
                mipLevels_ = other.mipLevels_;

                other.device_ = VK_NULL_HANDLE;
                other.allocator_ = VK_NULL_HANDLE;
                other.image_ = VK_NULL_HANDLE;
                other.imageAlloc_ = VK_NULL_HANDLE;
                other.imageView_ = VK_NULL_HANDLE;
                other.sampler_ = VK_NULL_HANDLE;
                other.format_ = VK_FORMAT_UNDEFINED;
                other.width_ = 0;
                other.height_ = 0;
                other.mipLevels_ = 1;
            }
            return *this;
        }

        void createFromRGBA8(
            VmaAllocator allocator,
            VkDevice device,
            VkCommandPool cmdPool,
            VkQueue graphicsQueue,
            const void *pixels,
            uint32_t w,
            uint32_t h,
            bool generateMips,
            VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

        /**
         * @brief Load texture from an image file using stb_image (requires OME3D_USE_STB).
         */
        void loadFromFile(
            VmaAllocator allocator,
            VkDevice device,
            VkCommandPool cmdPool,
            VkQueue graphicsQueue,
            const std::string &path,
            bool genMips,
            VkFormat fmt = VK_FORMAT_R8G8B8A8_UNORM);

        /**
         * @brief Destroy all Vulkan resources owned by this texture.
         * Safe to call multiple times.
         */
        void destroy();

        // --- Getters ---
        VkImageView view() const { return imageView_; }
        VkSampler sampler() const { return sampler_; }
        VkFormat format() const { return format_; }
        uint32_t width() const { return width_; }
        uint32_t height() const { return height_; }
        uint32_t mipLevels() const { return mipLevels_; }

    private:
        // --- Internal helpers ---
        VkCommandBuffer beginOneTimeCommands(VkDevice device, VkCommandPool pool);
        void endOneTimeCommands(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer cmd);
        void transition(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout,
                        uint32_t baseMip, uint32_t mipCount);
        void createImageView();
        void createSampler();
        void generateMipmaps(VkCommandPool pool, VkQueue queue);

        // --- Resource state ---
        VkDevice device_ = VK_NULL_HANDLE;
        VmaAllocator allocator_ = VK_NULL_HANDLE;
        VkImage image_ = VK_NULL_HANDLE;
        VmaAllocation imageAlloc_ = VK_NULL_HANDLE;
        VkImageView imageView_ = VK_NULL_HANDLE;
        VkSampler sampler_ = VK_NULL_HANDLE;
        VkFormat format_ = VK_FORMAT_UNDEFINED;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        uint32_t mipLevels_ = 1;
    };

} // namespace Vk::Gfx
