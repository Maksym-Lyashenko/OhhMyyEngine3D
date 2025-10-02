#include "rhi/vk/CommandPool.h"

#include "core/Logger.h"
#include <rhi/vk/Common.h>

using namespace Core;

namespace Vk
{

    CommandPool::CommandPool(VkDevice device, uint32_t graphicsQueueFamilyIndex)
        : device(device)
    {
        create(graphicsQueueFamilyIndex);
    }

    CommandPool::~CommandPool()
    {
        destroy();
    }

    void CommandPool::create(uint32_t graphicsQueueFamilyIndex)
    {
        // Command pools are tied to a single queue family.
        VkCommandPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        // Reset flag lets us reset individual command buffers each frame.
        info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        info.queueFamilyIndex = graphicsQueueFamilyIndex;

        VK_CHECK(vkCreateCommandPool(device, &info, nullptr, &pool));
        Logger::log(LogLevel::INFO, "Command pool created");
    }

    void CommandPool::destroy()
    {
        if (pool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device, pool, nullptr);
            pool = VK_NULL_HANDLE;
            Logger::log(LogLevel::INFO, "Command pool destroyed");
        }
    }

} // namespace Vk
