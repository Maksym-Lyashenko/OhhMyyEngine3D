#include "rhi/vk/VulkanInstance.h"
#include "platform/WindowManager.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"

// Fallbacks for portability enumeration bits/extensions on older SDKs
#ifndef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
#define VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME "VK_KHR_portability_enumeration"
#endif

#ifndef VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
// Value from the spec; safe fallback for older headers.
#define VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR 0x00000001
#endif

#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace Vk
{

    using Core::LogLevel;

    static PFN_vkCreateDebugUtilsMessengerEXT pfnCreateDebugUtilsMessengerEXT = nullptr;
    static PFN_vkDestroyDebugUtilsMessengerEXT pfnDestroyDebugUtilsMessengerEXT = nullptr;

    VulkanInstance::VulkanInstance(const Platform::WindowManager &window,
                                   bool enableValidation)
        : validationEnabled(enableValidation)
    {
        if (validationEnabled && !checkValidationLayerSupport())
        {
            throw std::runtime_error("Validation layers requested but not available");
        }

        // Query supported API version
        uint32_t supportedApiVersion = 0;
        VK_CHECK(vkEnumerateInstanceVersion(&supportedApiVersion));

        std::ostringstream oss;
        oss << "Vulkan supported API version: "
            << VK_API_VERSION_MAJOR(supportedApiVersion) << "."
            << VK_API_VERSION_MINOR(supportedApiVersion) << "."
            << VK_API_VERSION_PATCH(supportedApiVersion);
        Core::Logger::log(LogLevel::INFO, oss.str());

        // Be conservative: cap at 1.3 unless you know you need newer.
        const uint32_t capApi = VK_API_VERSION_1_3;
        const uint32_t requestedApiVersion = std::min(supportedApiVersion, capApi);

        // Application info
        VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        appInfo.pApplicationName = "OhhMyyEngine3D";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "OhhMyyEngine3D";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = requestedApiVersion;

        // Required instance extensions
        auto extensions = getRequiredExtensions(window);

        // Instance flags (add portability bit when needed)
        VkInstanceCreateFlags flags = 0;
        for (const char *ext : extensions)
        {
            if (std::strcmp(ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0)
            {
                flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
                portabilityEnabled = true;
            }
        }

        // Optional: hook debug messenger at create time via pNext to catch early messages
        VkDebugUtilsMessengerCreateInfoEXT dbgCreateInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        if (validationEnabled)
        {
            dbgCreateInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
#ifdef _DEBUG
            dbgCreateInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                             VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
#endif
            dbgCreateInfo.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            dbgCreateInfo.pfnUserCallback = &VulkanInstance::debugCallback;
            dbgCreateInfo.pUserData = nullptr;
        }

        VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        createInfo.flags = flags;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        if (validationEnabled)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            createInfo.pNext = &dbgCreateInfo; // enable early debug utils
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
        Core::Logger::log(LogLevel::INFO, "Vulkan instance created");

        // Load debug utils entry points and create messenger (if enabled)
        if (validationEnabled)
        {
            pfnCreateDebugUtilsMessengerEXT =
                reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
            pfnDestroyDebugUtilsMessengerEXT =
                reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

            setupDebugMessenger();
        }
    }

    VulkanInstance::~VulkanInstance() noexcept
    {
        if (debugMessenger)
        {
            // safe if function pointer is null (won't be when validation enabled)
            pfnDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
            debugMessenger = VK_NULL_HANDLE;
        }
        if (instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
            Core::Logger::log(LogLevel::INFO, "Vulkan instance destroyed");
        }
    }

    std::vector<const char *> VulkanInstance::getRequiredExtensions(const Platform::WindowManager &window) const
    {
        auto exts = window.getRequiredExtensions();

        if (validationEnabled)
        {
            exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        // Portability (MoltenVK/macOS) — add if available; harmless on other platforms
        // Many loaders expose it; adding here is safe. Instance flag is set in ctor if present.
        exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

        return exts;
    }

    bool VulkanInstance::checkValidationLayerSupport() const
    {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> layers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

        for (const char *name : validationLayers)
        {
            bool found = false;
            for (const auto &lp : layers)
            {
                if (std::strcmp(name, lp.layerName) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                return false;
        }
        return true;
    }

    void VulkanInstance::setupDebugMessenger() noexcept
    {
        if (!pfnCreateDebugUtilsMessengerEXT)
            return;

        VkDebugUtilsMessengerCreateInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
#ifdef _DEBUG
        info.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
#endif
        info.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        info.pfnUserCallback = &VulkanInstance::debugCallback;

        if (pfnCreateDebugUtilsMessengerEXT(instance, &info, nullptr, &debugMessenger) == VK_SUCCESS)
        {
            Core::Logger::log(LogLevel::INFO, "Debug messenger created");
        }
        else
        {
            Core::Logger::log(LogLevel::WARNING, "Failed to create debug messenger");
        }
    }

    VkBool32 VKAPI_CALL VulkanInstance::debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT /*type*/,
        const VkDebugUtilsMessengerCallbackDataEXT *data,
        void * /*user*/) noexcept
    {
        // route to your Logger with readable severity/type
        using Core::Logger;
        switch (severity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            Logger::log(LogLevel::ERROR, std::string("[Vulkan] ") + data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            Logger::log(LogLevel::WARNING, std::string("[Vulkan] ") + data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            // Logger::log(LogLevel::INFO, std::string("[Vulkan] ") + data->pMessage);
            break;
        default:
            Logger::log(LogLevel::DEBUG, std::string("[Vulkan] ") + data->pMessage);
            break;
        }
        return VK_FALSE; // don't abort
    }

} // namespace Vk
