#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

namespace Vk::Gfx
{

    struct Vertex
    {
        glm::vec3 pos;    // location = 0
        glm::vec3 normal; // location = 1

        static VkVertexInputBindingDescription binding()
        {
            VkVertexInputBindingDescription b{};
            b.binding = 0;
            b.stride = sizeof(Vertex);
            b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return b;
        }

        static std::array<VkVertexInputAttributeDescription, 2> attributes()
        {
            std::array<VkVertexInputAttributeDescription, 2> a{};
            // position
            a[0].location = 0;
            a[0].binding = 0;
            a[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            a[0].offset = offsetof(Vertex, pos);
            // normal
            a[1].location = 1;
            a[1].binding = 0;
            a[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            a[1].offset = offsetof(Vertex, normal);
            return a;
        }
    };

} // namespace Vk::Gfx
