#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>

namespace Vk::Gfx
{

    /**
     * Lightweight GPU 2D texture (VkImage + VkImageView + VkSampler).
     *
     * Responsibilities:
     *  - own/destroy image/memory/view/sampler (RAII)
     *  - upload RGBA8 pixels via staging buffer
     *  - (optional) generate mipmaps on GPU (blit chain)
     *
     * Notes:
     *  - No anisotropy (we didn't enable it on the device yet).
     *  - Sampler is clamped to repeat and uses linear filtering + mipmaps.
     *  - Format is VK_FORMAT_R8G8B8A8_UNORM by default.
     */
    class Texture2D
    {
    public:
        Texture2D() = default;
        ~Texture2D() { destroy(); }

        Texture2D(const Texture2D &) = delete;
        Texture2D &operator=(const Texture2D &) = delete;

        /**
         * Create from raw RGBA8 pixel data (row-major, tightly packed).
         * If generateMips==true, builds full mip chain (down to 1x1).
         */
        void createFromRGBA8(VkPhysicalDevice phys,
                             VkDevice device,
                             VkCommandPool cmdPool,
                             VkQueue graphicsQueue,
                             const void *pixels,
                             uint32_t width,
                             uint32_t height,
                             bool generateMips = true,
                             VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

        /**
         * Convenience: load from file path using stb_image (optional).
         * Define OME3D_USE_STB before including this header and link stb.
         */
        void loadFromFile(VkPhysicalDevice phys,
                          VkDevice device,
                          VkCommandPool cmdPool,
                          VkQueue graphicsQueue,
                          const std::string &path,
                          bool generateMips = true,
                          VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

        void destroy();

        // Accessors
        VkImageView view() const noexcept { return imageView_; }
        VkSampler sampler() const noexcept { return sampler_; }
        VkImage image() const noexcept { return image_; }
        uint32_t width() const noexcept { return width_; }
        uint32_t height() const noexcept { return height_; }
        uint32_t mipLevels() const noexcept { return mipLevels_; }

    private:
        // owned GPU objects
        VkDevice device_ = VK_NULL_HANDLE;
        VkImage image_ = VK_NULL_HANDLE;
        VkDeviceMemory memory_ = VK_NULL_HANDLE;
        VkImageView imageView_ = VK_NULL_HANDLE;
        VkSampler sampler_ = VK_NULL_HANDLE;

        // meta
        uint32_t width_ = 0, height_ = 0, mipLevels_ = 1;
        VkFormat format_ = VK_FORMAT_R8G8B8A8_UNORM;

        // --- creation helpers ---
        static uint32_t findMemoryType(VkPhysicalDevice phys,
                                       uint32_t typeBits,
                                       VkMemoryPropertyFlags props);

        void createImage(VkPhysicalDevice phys,
                         uint32_t w, uint32_t h, uint32_t mips,
                         VkFormat fmt, VkImageUsageFlags usage);

        void createImageView();
        void createSampler();

        // --- upload path ---
        VkCommandBuffer beginOneTimeCommands(VkDevice device, VkCommandPool pool);
        void endOneTimeCommands(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer cmd);

        void transition(VkCommandBuffer cmd,
                        VkImageLayout oldLayout,
                        VkImageLayout newLayout,
                        uint32_t baseMip = 0,
                        uint32_t mipCount = 1);

        void copyBufferToImage(VkDevice device,
                               VkPhysicalDevice phys,
                               VkCommandPool pool,
                               VkQueue queue,
                               const void *pixels,
                               size_t byteSize);

        void generateMipmaps(VkPhysicalDevice phys,
                             VkCommandPool pool,
                             VkQueue queue);
    };

} // namespace Vk::Gfx
