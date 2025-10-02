#include "rhi/vk/gfx/Mesh.h"

#include "rhi/vk/vk_utils.h"

namespace Vk::Gfx
{

    void Mesh::create(VkPhysicalDevice phys, VkDevice dev,
                      const std::vector<Vertex> &vertices,
                      const std::vector<uint32_t> &indices,
                      const glm::mat4 &local)
    {
        const uint32_t vtxCount = static_cast<uint32_t>(vertices.size());
        for (size_t i = 0; i < indices.size(); ++i)
        {
            if (indices[i] >= vtxCount)
            {
                throw std::runtime_error("Mesh::create: index out of range " +
                                         std::to_string(indices[i]) + " >= " + std::to_string(vtxCount));
            }
        }

        localTransform = local;

        indexCount = static_cast<uint32_t>(indices.size());

        if (!vertices.empty())
        {
            aabbMin = glm::vec3(vertices[0].pos[0],
                                vertices[0].pos[1],
                                vertices[0].pos[2]);
            aabbMax = aabbMin;
            for (const auto &v : vertices)
            {
                glm::vec3 p(v.pos[0], v.pos[1], v.pos[2]);
                aabbMin = glm::min(aabbMin, p);
                aabbMax = glm::max(aabbMax, p);
            }
        }

        vbo.create(phys, dev,
                   sizeof(Vertex) * vertices.size(),
                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vbo.upload(vertices.data(), sizeof(Vertex) * vertices.size());

        ibo.create(phys, dev,
                   sizeof(uint32_t) * indices.size(),
                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        ibo.upload(indices.data(), sizeof(uint32_t) * indices.size());
    }

    void Mesh::bind(VkCommandBuffer cmd) const
    {
        VkBuffer vb = vbo.get();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
        vkCmdBindIndexBuffer(cmd, ibo.get(), 0, VK_INDEX_TYPE_UINT32);
    }

    void Mesh::draw(VkCommandBuffer cmd) const
    {
        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
    }

} // namespace Vk::Gfx
