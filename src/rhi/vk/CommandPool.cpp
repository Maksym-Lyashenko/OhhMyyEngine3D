#include "rhi/vk/CommandPool.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

namespace Vk
{

    CommandPool::CommandPool(VkDevice device,
                             uint32_t graphicsQueueFamilyIndex,
                             VkCommandPoolCreateFlags flags)
        : device_(device)
    {
        create(graphicsQueueFamilyIndex, flags);
    }

    CommandPool::~CommandPool() noexcept
    {
        destroy();
    }

    CommandPool::CommandPool(CommandPool &&other) noexcept
        : device_(other.device_), pool_(other.pool_)
    {
        other.device_ = VK_NULL_HANDLE;
        other.pool_ = VK_NULL_HANDLE;
    }

    CommandPool &CommandPool::operator=(CommandPool &&other) noexcept
    {
        if (this != &other)
        {
            destroy();
            device_ = other.device_;
            pool_ = other.pool_;
            other.device_ = VK_NULL_HANDLE;
            other.pool_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    void CommandPool::create(uint32_t graphicsQueueFamilyIndex, VkCommandPoolCreateFlags flags)
    {
        VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        info.flags = flags; // RESET / TRANSIENT flags as needed
        info.queueFamilyIndex = graphicsQueueFamilyIndex;

        VK_CHECK(vkCreateCommandPool(device_, &info, nullptr, &pool_));
        Core::Logger::log(Core::LogLevel::INFO, "Command pool created");
    }

    void CommandPool::destroy() noexcept
    {
        if (pool_ != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device_, pool_, nullptr);
            pool_ = VK_NULL_HANDLE;
            Core::Logger::log(Core::LogLevel::INFO, "Command pool destroyed");
        }
    }

    void CommandPool::reset(VkCommandPoolResetFlags flags) const
    {
        // Resets all command buffers allocated from this pool.
        // Use 0 or VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT to free internal memory.
        VK_CHECK(vkResetCommandPool(device_, pool_, flags));
    }

    void CommandPool::recreate(uint32_t graphicsQueueFamilyIndex, VkCommandPoolCreateFlags flags)
    {
        destroy();
        create(graphicsQueueFamilyIndex, flags);
    }

} // namespace Vk
