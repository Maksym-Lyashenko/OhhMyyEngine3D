#include "rhi/vk/gfx/Mesh.h"
#include "rhi/vk/Common.h" // VK_CHECK (if you use it elsewhere)

#include <algorithm>
#include <limits>

namespace Vk::Gfx
{
    void Mesh::create(VmaAllocator allocator,
                      VkDevice device,
                      VkCommandPool cmdPool,
                      VkQueue queue,
                      const std::vector<Vertex> &vertices,
                      const std::vector<uint32_t> &indices,
                      const glm::mat4 &local)
    {
        // Validate indices against vertex count
        const uint32_t vtxCount = static_cast<uint32_t>(vertices.size());
        for (size_t i = 0; i < indices.size(); ++i)
        {
            if (indices[i] >= vtxCount)
            {
                throw std::runtime_error(
                    "Mesh::create: index out of range " +
                    std::to_string(indices[i]) + " >= " + std::to_string(vtxCount));
            }
        }

        // Reset previous GPU resources
        destroy();

        // Store transform
        localTransform_ = local;

        // Compute CPU-side AABB
        if (!vertices.empty())
        {
            aabbMin_ = glm::vec3(vertices[0].pos[0], vertices[0].pos[1], vertices[0].pos[2]);
            aabbMax_ = aabbMin_;
            for (const auto &v : vertices)
            {
                const glm::vec3 p(v.pos[0], v.pos[1], v.pos[2]);
                aabbMin_ = glm::min(aabbMin_, p);
                aabbMax_ = glm::max(aabbMax_, p);
            }
        }
        else
        {
            aabbMin_ = glm::vec3(0.0f);
            aabbMax_ = glm::vec3(0.0f);
        }

        indexCount_ = static_cast<uint32_t>(indices.size());

        // Create & upload VBO (DEVICE_LOCAL via transient staging)
        const VkDeviceSize vboBytes = static_cast<VkDeviceSize>(sizeof(Vertex) * vertices.size());
        if (vboBytes > 0)
        {
            vbo_.createDeviceLocalWithData(
                allocator, device, cmdPool, queue,
                vertices.data(), vboBytes,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        }
        else
        {
            // Create an empty buffer so bind() is harmless (optional)
            vbo_.create(allocator, device, 0,
                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VMA_MEMORY_USAGE_GPU_ONLY);
        }

        // Create & upload IBO (DEVICE_LOCAL via transient staging)
        const VkDeviceSize iboBytes = static_cast<VkDeviceSize>(sizeof(uint32_t) * indices.size());
        if (iboBytes > 0)
        {
            ibo_.createDeviceLocalWithData(
                allocator, device, cmdPool, queue,
                indices.data(), iboBytes,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        }
        else
        {
            ibo_.create(allocator, device, 0,
                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VMA_MEMORY_USAGE_GPU_ONLY);
        }
    }

    void Mesh::bind(VkCommandBuffer cmd) const noexcept
    {
        if (vbo_.get() == VK_NULL_HANDLE || ibo_.get() == VK_NULL_HANDLE)
            return;

        VkBuffer vb = vbo_.get();
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &off);
        vkCmdBindIndexBuffer(cmd, ibo_.get(), 0, VK_INDEX_TYPE_UINT32);
    }

    void Mesh::draw(VkCommandBuffer cmd) const noexcept
    {
        if (indexCount_ == 0)
            return;
        vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
    }

} // namespace Vk::Gfx
