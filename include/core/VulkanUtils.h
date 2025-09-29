#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <sstream>
#include "core/Logger.h"

// Converts VkResult enum to readable string
inline const char *VkResultToString(VkResult result)
{
    switch (result)
    {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_EVENT_SET:
        return "VK_EVENT_SET";
    case VK_EVENT_RESET:
        return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";

    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
        return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN:
        return "VK_ERROR_UNKNOWN";

    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV:
        return "VK_ERROR_INVALID_SHADER_NV";

    // Extend if you use other extensions with their own codes
    default:
        return "VK_RESULT_UNKNOWN";
    }
}

// Handy macro to check Vulkan calls and log nicely
#define VK_CHECK(call)                                               \
    do                                                               \
    {                                                                \
        VkResult _vk_result = (call);                                \
        if (_vk_result != VK_SUCCESS)                                \
        {                                                            \
            std::ostringstream _vk_err;                              \
            _vk_err << "Vulkan error: " << #call                     \
                    << " returned " << VkResultToString(_vk_result)  \
                    << " (" << _vk_result << ")";                    \
            Core::Logger::log(Core::LogLevel::ERROR, _vk_err.str()); \
            throw std::runtime_error(_vk_err.str());                 \
        }                                                            \
    } while (0)

#define VK_CHECK_FATAL(call) \
    do                       \
    {                        \
        VkResult r = (call); \
        if (r != VK_SUCCESS) \
        {                    \
            ... throw;       \
        }                    \
    } while (0)

#define VK_CHECK_WARN(call)        \
    do                             \
    {                              \
        VkResult r = (call);       \
        if (r != VK_SUCCESS)       \
        {                          \
            ... Logger::warning... \
        }                          \
    } while (0)
