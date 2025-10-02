#pragma once
#include <vulkan/vulkan.h>

namespace Vk
{

    class DepthResources
    {
    public:
        DepthResources() = default;
        ~DepthResources() { destroy(); }

        DepthResources(const DepthResources &) = delete;
        DepthResources &operator=(const DepthResources &) = delete;

        void create(VkPhysicalDevice physicalDevice,
                    VkDevice device,
                    VkExtent2D extent,
                    VkCommandPool commandPool,
                    VkQueue graphicsQueue);

        void destroy();

        VkFormat getFormat() const { return format; }
        VkImage getImage() const { return image; }
        VkImageView getView() const { return view; }

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_D32_SFLOAT;

        static bool hasStencil(VkFormat f)
        {
            return f == VK_FORMAT_D32_SFLOAT_S8_UINT || f == VK_FORMAT_D24_UNORM_S8_UINT;
        }

        void transitionToAttachment(VkCommandPool commandPool, VkQueue queue);
    };

} // namespace Vk
