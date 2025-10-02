#pragma once
#include <vulkan/vulkan.h>

namespace Vk
{

    // Owns a VkCommandPool used to allocate primary command buffers.
    class CommandPool
    {
    public:
        CommandPool(VkDevice device, uint32_t graphicsQueueFamilyIndex);
        ~CommandPool();

        CommandPool(const CommandPool &) = delete;
        CommandPool &operator=(const CommandPool &) = delete;

        VkCommandPool get() const { return pool; }

    private:
        VkDevice device{};
        VkCommandPool pool{VK_NULL_HANDLE};

        void create(uint32_t graphicsQueueFamilyIndex);
        void destroy();
    };

} // namespace Vk
