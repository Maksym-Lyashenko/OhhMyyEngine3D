#include "rhi/vk/VulkanLogicalDevice.h"
#include "rhi/vk/VulkanPhysicalDevice.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

#ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif

#include <vector>
#include <string>
#include <set>
#include <cstring>
#include <algorithm>

namespace Vk
{

    using Core::LogLevel;

    // small helper: check if a device extension is supported
    static bool hasExtension(const char *name,
                             const std::vector<VkExtensionProperties> &avail)
    {
        for (const auto &e : avail)
        {
            if (std::strcmp(e.extensionName, name) == 0)
                return true;
        }
        return false;
    }

    VulkanLogicalDevice::VulkanLogicalDevice(const VulkanPhysicalDevice &physicalDevice)
    {
        // --- 1) Query queue families from the chosen physical device ---
        const auto indices = physicalDevice.getQueueFamilies();
        graphicsQueueFamilyIndex_ = indices.graphicsFamily.value();
        presentQueueFamilyIndex_ = indices.presentFamily.value();
        std::set<uint32_t> uniqueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()};

        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        float prio = 1.0f;
        for (uint32_t family : uniqueFamilies)
        {
            VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            q.queueFamilyIndex = family;
            q.queueCount = 1;
            q.pQueuePriorities = &prio;
            queueInfos.push_back(q);
        }

        // --- 2) Device extensions (swapchain is mandatory; portability subset when present) ---
        // Query available device extensions
        uint32_t extCount = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice.getDevice(), nullptr, &extCount, nullptr));
        std::vector<VkExtensionProperties> available(extCount);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice.getDevice(), nullptr, &extCount, available.data()));

        std::vector<const char *> requiredExts = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        // MoltenVK/portability requirement — enable if supported (harmless elsewhere if absent)
        if (hasExtension(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME, available))
        {
            requiredExts.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
            Core::Logger::log(LogLevel::INFO, "Enabling VK_KHR_portability_subset for this device");
        }

        // Validate required extensions presence
        for (const char *ext : requiredExts)
        {
            if (!hasExtension(ext, available))
            {
                std::string msg = std::string("Required device extension missing: ") + ext;
                Core::Logger::log(LogLevel::ERROR, msg);
                throw std::runtime_error(msg);
            }
        }

        // --- 3) Features chain (Vulkan 1.3) ---
        VkPhysicalDeviceFeatures coreFeatures{}; // keep default-off core features

        VkPhysicalDeviceVulkan13Features v13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        v13.synchronization2 = VK_TRUE;
        v13.dynamicRendering = VK_TRUE;

        // If you’ll need 1.2/1.1 features in future, chain them here before v13.

        // --- 4) Create device ---
        VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        ci.pNext = &v13;
        ci.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
        ci.pQueueCreateInfos = queueInfos.data();
        ci.pEnabledFeatures = &coreFeatures; // legacy core features struct
        ci.enabledExtensionCount = static_cast<uint32_t>(requiredExts.size());
        ci.ppEnabledExtensionNames = requiredExts.data();
        ci.enabledLayerCount = 0; // device layers deprecated

        VK_CHECK(vkCreateDevice(physicalDevice.getDevice(), &ci, nullptr, &device));

        // --- 5) Fetch queues ---
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

        Core::Logger::log(LogLevel::INFO, "Logical device created (VK_KHR_swapchain enabled)");
        Core::Logger::log(LogLevel::INFO, "Graphics & present queues retrieved");
    }

    VulkanLogicalDevice::~VulkanLogicalDevice() noexcept
    {
        if (device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(device);
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
            Core::Logger::log(LogLevel::INFO, "Logical device destroyed");
        }
    }

} // namespace Vk
