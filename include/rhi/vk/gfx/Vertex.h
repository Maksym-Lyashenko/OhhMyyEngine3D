#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef> // offsetof
#include <cstdint>

#include <glm/vec3.hpp>

namespace Vk::Gfx
{

    /**
     * @brief CPU-side vertex layout for the basic PBR-less pipeline.
     *
     * Semantics:
     *   location = 0 -> position (vec3)
     *   location = 1 -> normal   (vec3)
     *
     * Notes:
     *   - Matches GLSL:
     *       layout(location = 0) in vec3 inPos;
     *       layout(location = 1) in vec3 inNormal;
     *   - If you add UVs/tangents later, extend the struct and attributes() accordingly.
     */
    struct Vertex
    {
        glm::vec3 pos;    // location = 0
        glm::vec3 normal; // location = 1

        /// Binding description for a single interleaved vertex buffer (binding = 0).
        [[nodiscard]] static VkVertexInputBindingDescription binding() noexcept
        {
            VkVertexInputBindingDescription b{};
            b.binding = 0;
            b.stride = static_cast<uint32_t>(sizeof(Vertex));
            b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return b;
        }

        /// Attribute descriptions for pos/normal at locations 0/1.
        [[nodiscard]] static std::array<VkVertexInputAttributeDescription, 2> attributes() noexcept
        {
            std::array<VkVertexInputAttributeDescription, 2> a{};

            // position (vec3)
            a[0].location = 0;
            a[0].binding = 0;
            a[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            a[0].offset = static_cast<uint32_t>(offsetof(Vertex, pos));

            // normal (vec3)
            a[1].location = 1;
            a[1].binding = 0;
            a[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            a[1].offset = static_cast<uint32_t>(offsetof(Vertex, normal));

            return a;
        }
    };

    // Basic sanity checks to catch accidental padding/alignment changes.
    static_assert(offsetof(Vertex, pos) == 0, "pos must be at offset 0");
    static_assert(offsetof(Vertex, normal) == sizeof(glm::vec3), "normal must follow pos");
    static_assert(sizeof(Vertex) == sizeof(glm::vec3) * 2, "Vertex size must be 24 bytes");

} // namespace Vk::Gfx
