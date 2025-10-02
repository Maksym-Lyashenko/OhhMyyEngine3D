#include "rhi/vk/VulkanInstance.h"
#include "platform/WindowManager.h"

#include "core/Logger.h"
#include <rhi/vk/Common.h>

#include <sstream>
#include <stdexcept>

using namespace Core;
using namespace Platform;

namespace Vk
{
    VulkanInstance::VulkanInstance(const WindowManager &window, bool enableValidation)
        : validationEnabled(enableValidation)
    {
        if (validationEnabled && !checkValidationLayerSupport())
        {
            throw std::runtime_error("Validation layers requested but not available!");
        }

        // Query supported API version
        uint32_t supportedApiVersion = 0;
        VK_CHECK(vkEnumerateInstanceVersion(&supportedApiVersion));

        std::ostringstream oss;
        oss << "Vulkan supported API version: "
            << VK_API_VERSION_MAJOR(supportedApiVersion) << "."
            << VK_API_VERSION_MINOR(supportedApiVersion) << "."
            << VK_API_VERSION_PATCH(supportedApiVersion);
        Logger::log(LogLevel::INFO, oss.str());

        // Request Vulkan 1.4 if available, otherwise fallback
        uint32_t requestedApiVersion = VK_API_VERSION_1_4;
        if (supportedApiVersion < requestedApiVersion)
        {
            Logger::log(LogLevel::WARNING, "Vulkan 1.4 not supported, falling back to available version");
            requestedApiVersion = supportedApiVersion;
        }

        // Application Info
        VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        appInfo.pApplicationName = "OhhMyyEngine3D";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "OhhMyyEngine3D";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = requestedApiVersion;

        // Required Extensions from GLFW
        auto extensions = getRequiredExtensions(window);

        VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (validationEnabled)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        // Create Vulkan Instance
        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));

        Logger::log(LogLevel::INFO, "Vulkan instance created successfully");
    }

    VulkanInstance::~VulkanInstance()
    {
        if (instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(instance, nullptr);
            Logger::log(LogLevel::INFO, "Vulkan instance destroyed");
        }
    }

    std::vector<const char *> VulkanInstance::getRequiredExtensions(const WindowManager &window) const
    {
        auto extensions = window.getRequiredExtensions();

        if (validationEnabled)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool VulkanInstance::checkValidationLayerSupport() const
    {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char *layerName : validationLayers)
        {
            bool layerFound = false;
            for (const auto &layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound)
            {
                return false;
            }
        }
        return true;
    }
} // namespace Vk
