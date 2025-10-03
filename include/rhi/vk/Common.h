#pragma once
#include <vulkan/vulkan.h>
#include "core/Logger.h"

#include <stdexcept>
#include <sstream>
#include <initializer_list>

// -------- Result → string ----------
inline const char *VkResultToString(VkResult r)
{
    switch (r)
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
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";

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

    // Common extension/1.1+ errors worth handling explicitly:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION:
        return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
    // NV/legacy:
    case VK_ERROR_INVALID_SHADER_NV:
        return "VK_ERROR_INVALID_SHADER_NV";
    default:
        return "VK_RESULT_UNKNOWN";
    }
}

// ---------- helpers ----------
inline bool vk_is_allowed(VkResult r, std::initializer_list<VkResult> ok)
{
    for (auto v : ok)
        if (r == v)
            return true;
    return false;
}

#define VK__FORMAT_MSG(prefix, call, result) \
    ([&]() {                                                                       \
        std::ostringstream _vk_msg;                                               \
        _vk_msg << prefix << #call << " -> " << VkResultToString(result)          \
                << " (" << static_cast<int>(result) << ") "                       \
                << " at " << __FILE__ << ":" << __LINE__ << " in " << __func__;   \
        return _vk_msg.str(); }())

// ---------- strict check: throw on anything != SUCCESS ----------
#define VK_CHECK(call)                                               \
    do                                                               \
    {                                                                \
        VkResult _vk_r = (call);                                     \
        if (_vk_r != VK_SUCCESS)                                     \
        {                                                            \
            auto _m = VK__FORMAT_MSG("Vulkan error: ", call, _vk_r); \
            Core::Logger::log(Core::LogLevel::ERROR, _m);            \
            throw std::runtime_error(_m);                            \
        }                                                            \
    } while (0)

// ---------- strict check but terminate (for ctor/dtor paths) ----------
#define VK_CHECK_FATAL(call)                                         \
    do                                                               \
    {                                                                \
        VkResult _vk_r = (call);                                     \
        if (_vk_r != VK_SUCCESS)                                     \
        {                                                            \
            auto _m = VK__FORMAT_MSG("Vulkan FATAL: ", call, _vk_r); \
            Core::Logger::log(Core::LogLevel::ERROR, _m);            \
            std::terminate();                                        \
        }                                                            \
    } while (0)

// ---------- warn-only ----------
#define VK_CHECK_WARN(call)                                                     \
    do                                                                          \
    {                                                                           \
        VkResult _vk_r = (call);                                                \
        if (_vk_r != VK_SUCCESS)                                                \
        {                                                                       \
            Core::Logger::log(Core::LogLevel::WARNING,                          \
                              VK__FORMAT_MSG("Vulkan warning: ", call, _vk_r)); \
        }                                                                       \
    } while (0)

// ---------- check with whitelist of acceptable results ----------
// Usage: VK_CHECK_ALLOWED(vkQueuePresentKHR(...), { VK_SUBOPTIMAL_KHR, VK_ERROR_OUT_OF_DATE_KHR });
#define VK_CHECK_ALLOWED(call, allowed_list)                            \
    do                                                                  \
    {                                                                   \
        VkResult _vk_r = (call);                                        \
        if (_vk_r != VK_SUCCESS && !vk_is_allowed(_vk_r, allowed_list)) \
        {                                                               \
            auto _m = VK__FORMAT_MSG("Vulkan error: ", call, _vk_r);    \
            Core::Logger::log(Core::LogLevel::ERROR, _m);               \
            throw std::runtime_error(_m);                               \
        }                                                               \
    } while (0)

// ---------- debug-only (no-throw in Release) ----------
#if !defined(NDEBUG)
#define VK_DCHECK(call) VK_CHECK(call)
#else
#define VK_DCHECK(call) (void)(call)
#endif
