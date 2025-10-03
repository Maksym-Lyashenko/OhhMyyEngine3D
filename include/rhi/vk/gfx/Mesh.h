#pragma once

#include "rhi/vk/gfx/Buffer.h"
#include "rhi/vk/gfx/Vertex.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>
#include <stdexcept>

namespace Vk::Gfx
{

    /**
     * @brief Simple GPU mesh: vertex+index buffers + local transform + CPU-side AABB.
     *
     * Notes:
     *  - Buffers are created as HOST_VISIBLE|HOST_COHERENT and uploaded directly.
     *    For large meshes prefer a staging path with DEVICE_LOCAL buffers.
     *  - Not copyable, but movable.
     */
    class Mesh
    {
    public:
        Mesh() = default;
        ~Mesh() noexcept { destroy(); }

        Mesh(const Mesh &) = delete;
        Mesh &operator=(const Mesh &) = delete;

        Mesh(Mesh &&other) noexcept { moveFrom(other); }
        Mesh &operator=(Mesh &&other) noexcept
        {
            if (this != &other)
            {
                destroy();
                moveFrom(other);
            }
            return *this;
        }

        /// Create vertex/index buffers and upload data. Throws on failure or bad indices.
        void create(VkPhysicalDevice phys, VkDevice dev,
                    const std::vector<Vertex> &vertices,
                    const std::vector<uint32_t> &indices,
                    const glm::mat4 &local = glm::mat4(1.0f));

        /// Destroy buffers (safe to call multiple times).
        void destroy() noexcept
        {
            ibo_.destroy();
            vbo_.destroy();
            indexCount_ = 0u;
            // keep AABB and transform values; they are harmless
        }

        /// Bind VBO/IBO to the given command buffer.
        void bind(VkCommandBuffer cmd) const noexcept;

        /// Issue an indexed draw (1 instance). Does nothing if indexCount == 0.
        void draw(VkCommandBuffer cmd) const noexcept;

        // ----- Accessors -----
        [[nodiscard]] const glm::mat4 &getLocalTransform() const noexcept { return localTransform_; }
        [[nodiscard]] uint32_t getIndexCount() const noexcept { return indexCount_; }

        [[nodiscard]] const glm::vec3 &getMin() const noexcept { return aabbMin_; }
        [[nodiscard]] const glm::vec3 &getMax() const noexcept { return aabbMax_; }

    private:
        void moveFrom(Mesh &other) noexcept
        {
            vbo_ = std::move(other.vbo_);
            ibo_ = std::move(other.ibo_);
            indexCount_ = other.indexCount_;
            other.indexCount_ = 0u;
            aabbMin_ = other.aabbMin_;
            aabbMax_ = other.aabbMax_;
            localTransform_ = other.localTransform_;
        }

        Buffer vbo_;
        Buffer ibo_;
        uint32_t indexCount_{0};

        glm::vec3 aabbMin_{0.0f};
        glm::vec3 aabbMax_{0.0f};

        glm::mat4 localTransform_{1.0f};
    };

} // namespace Vk::Gfx
