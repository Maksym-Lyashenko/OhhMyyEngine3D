#pragma once

#define VMA_STATS_STRING_ENABLED 1 // vmaBuildStatsString
// logger VMA:
#define VMA_DEBUG_LOG(format, ...) Core::Logger::logf(Core::LogLevel::DEBUG, format, ##__VA_ARGS__)

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "rhi/vk/Common.h"

namespace Vk
{
    class VulkanAllocator
    {
    public:
        VulkanAllocator() = default;
        ~VulkanAllocator() { destroy(); }

        void init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
        {
            physicalDevice_ = physicalDevice;

            VmaVulkanFunctions funcs{};
            funcs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
            funcs.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

            VmaAllocatorCreateInfo info{};
            info.instance = instance;
            info.physicalDevice = physicalDevice;
            info.device = device;
            info.pVulkanFunctions = &funcs;
            info.vulkanApiVersion = VK_API_VERSION_1_4;

            VK_CHECK(vmaCreateAllocator(&info, &allocator));
        }

        void destroy()
        {
            if (allocator != VK_NULL_HANDLE)
            {
                logBudgets();
                dumpStatsToFile("vma_stats.json", true);

                vmaDestroyAllocator(allocator);
                allocator = VK_NULL_HANDLE;
            }
        }

        void getBudgets(std::vector<VmaBudget> &outBudgets,
                        VkPhysicalDeviceMemoryProperties &outMemProps) const
        {
            outBudgets.assign(VK_MAX_MEMORY_HEAPS, {});
            vmaGetHeapBudgets(allocator, outBudgets.data());
            vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &outMemProps);
        }

        void logBudgets() const
        {
            if (!allocator)
                return;

            VkPhysicalDeviceMemoryProperties mp{};
            vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &mp);

            VmaBudget budgets[VK_MAX_MEMORY_HEAPS]{};
            vmaGetHeapBudgets(allocator, budgets);

            for (uint32_t i = 0; i < mp.memoryHeapCount; ++i)
            {
                const auto &b = budgets[i];

                Core::Logger::log(
                    Core::LogLevel::INFO,
                    "VMA Heap " + std::to_string(i) +
                        " used=" + std::to_string(b.usage / (1024 * 1024)) + " MB / " +
                        std::to_string(b.budget / (1024 * 1024)) + " MB" +
                        " allocCount=" + std::to_string(b.statistics.allocationCount) +
                        " blockCount=" + std::to_string(b.statistics.blockCount));
            }
        }

        void dumpStatsToFile(const std::string &path, bool detailed) const
        {
#if VMA_STATS_STRING_ENABLED
            if (!allocator)
                return;
            char *stats = nullptr;
            vmaBuildStatsString(allocator, &stats, /*detailedMap*/ detailed ? 1 : 0);
            if (stats)
            {
                std::ofstream f(path, std::ios::binary);
                if (f)
                    f << stats;
                vmaFreeStatsString(allocator, stats);
                Core::Logger::log(Core::LogLevel::INFO, std::string("VMA stats written to ") + path);
            }
#else
            (void)path;
            Core::Logger::log(Core::LogLevel::WARNING,
                              "VMA stats string disabled (define VMA_STATS_STRING_ENABLED 1 before including vk_mem_alloc.h)");
#endif
        }

        [[nodiscard]] VmaAllocator get() const { return allocator; }

    private:
        VmaAllocator allocator = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    };
}
