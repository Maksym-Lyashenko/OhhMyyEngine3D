#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

namespace Vk
{

    /**
     * @brief RAII wrapper over VkCommandPool for a single queue family.
     *
     * Notes:
     *  - Default flags enable VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
     *    which is handy if you re-record per frame.
     *  - Consider adding VK_COMMAND_POOL_CREATE_TRANSIENT_BIT if most command
     *    buffers are short-lived.
     */
    class CommandPool final
    {
    public:
        CommandPool(VkDevice device,
                    uint32_t graphicsQueueFamilyIndex,
                    VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        ~CommandPool() noexcept;

        CommandPool(const CommandPool &) = delete;
        CommandPool &operator=(const CommandPool &) = delete;

        CommandPool(CommandPool &&other) noexcept;
        CommandPool &operator=(CommandPool &&other) noexcept;

        /// Underlying pool handle.
        VkCommandPool get() const noexcept { return pool_; }

        /// Reset all command buffers allocated from this pool.
        void reset(VkCommandPoolResetFlags flags = 0) const;

        /// Destroy and create again (e.g., if queue family/flags changed).
        void recreate(uint32_t graphicsQueueFamilyIndex,
                      VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    private:
        VkDevice device_{VK_NULL_HANDLE};
        VkCommandPool pool_{VK_NULL_HANDLE};

        void create(uint32_t graphicsQueueFamilyIndex, VkCommandPoolCreateFlags flags);
        void destroy() noexcept;
    };

} // namespace Vk
