#pragma once

#include <vulkan/vulkan.h>

#include <sstream>
#include <glm/ext/matrix_float4x4.hpp>

inline VkFormat FindSupportedDepthFormat(VkPhysicalDevice physicalDevice)
{
    const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT};

    for (VkFormat f : candidates)
    {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, f, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return f;
    }
    throw std::runtime_error("No supported depth format");
}

struct PushPC
{
    glm::mat4 model;
    glm::mat4 normalMatrix;
};