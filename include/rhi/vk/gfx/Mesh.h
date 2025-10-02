#pragma once

#include "rhi/vk/gfx/Buffer.h"
#include "rhi/vk/gfx/Vertex.h"

#include <glm/glm.hpp>

#include <vector>
#include <cstdint>

namespace Vk::Gfx
{

    class Mesh
    {
    public:
        Mesh() = default;
        ~Mesh() { destroy(); }

        Mesh(const Mesh &) = delete;
        Mesh &operator=(const Mesh &) = delete;

        void create(VkPhysicalDevice phys, VkDevice dev,
                    const std::vector<Vertex> &vertices,
                    const std::vector<uint32_t> &indices,
                    const glm::mat4 &local = glm::mat4(1.0f));

        void destroy()
        {
            ibo.destroy();
            vbo.destroy();
            indexCount = 0;
        }

        void bind(VkCommandBuffer cmd) const;
        void draw(VkCommandBuffer cmd) const;

        const glm::mat4 &getLocalTransform() const { return localTransform; }
        uint32_t getIndexCount() const { return indexCount; }

        const glm::vec3 &getMin() const { return aabbMin; }
        const glm::vec3 &getMax() const { return aabbMax; }

    private:
        Buffer vbo;
        Buffer ibo;
        uint32_t indexCount{0};

        glm::vec3 aabbMin{0.0f};
        glm::vec3 aabbMax{0.0f};

        glm::mat4 localTransform{1.0f};
    };

} // namespace Vk::Gfx
