#include "render/Scene.h"

#include "core/Logger.h"
#include "rhi/vk/Common.h"
#include "rhi/vk/gfx/Vertex.h"
#include "rhi/vk/gfx/utils/MeshUtils.h"

#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

using Core::Logger;
using Core::LogLevel;
using Core::MathUtils::AABB;
using Vk::Gfx::Utils::computeWorldAABB;

namespace Render
{
    void Scene::loadModel(const std::string &gltfPath,
                          VmaAllocator allocator,
                          VkDevice device,
                          VkCommandPool cmdPool,
                          VkQueue queue,
                          MaterialSystem &materialSystem)
    {
        // 1) Parse glTF -> MeshData list (CPU side)
        std::vector<Asset::MeshData> meshDatas = Asset::GltfLoader::loadMeshes(gltfPath);

        // 2) Run mesh optimization passes
        {
            struct MeshStats
            {
                size_t vertices{};
                size_t indices{};
                size_t triangles() const { return indices / 3; }
            };

            auto collectStats = [](const std::vector<Asset::MeshData> &mds)
            {
                MeshStats s{};
                for (auto &m : mds)
                {
                    s.vertices += m.positions.size() / 3;
                    s.indices += m.indices.size();
                }
                return s;
            };

            MeshStats before = collectStats(meshDatas);

            Asset::Processing::OptimizeSettings opt{};
            opt.optimizeCache = true;
            opt.optimizeOverdraw = true;
            opt.overdrawThreshold = 1.05f;
            opt.optimizeFetch = true;
            opt.simplify = true;            // optional LOD-like simplification
            opt.simplifyTargetRatio = 0.6f; // ~60% triangles
            opt.simplifyError = 1e-2f;

            for (auto &md : meshDatas)
            {
                Asset::Processing::OptimizeMeshInPlace(md, opt);
            }

            MeshStats after = collectStats(meshDatas);
            CORE_LOG_DEBUG(
                "Mesh optimize: vertices " + std::to_string(before.vertices) + " -> " + std::to_string(after.vertices) +
                ", indices " + std::to_string(before.indices) + " -> " + std::to_string(after.indices) +
                ", tris " + std::to_string(before.triangles()) + " -> " + std::to_string(after.triangles()));
        }

        // 3) Upload each mesh to GPU (DEVICE_LOCAL via transient staging)
        gpuMeshes_.clear();
        drawItems_.clear();

        gpuMeshes_.reserve(meshDatas.size());
        drawItems_.reserve(meshDatas.size());

        for (auto &md : meshDatas)
        {
            // Build interleaved vertex buffer compatible with Vk::Gfx::Vertex
            std::vector<Vk::Gfx::Vertex> vertices;
            const size_t vertCount = md.positions.size() / 3;
            const bool hasNormals = (md.normals.size() == md.positions.size());
            const bool hasUVs = (md.texcoords.size() == vertCount * 2);
            const bool hasTangents = (md.tangents.size() == vertCount * 4);

            vertices.reserve(vertCount);

            for (size_t i = 0; i < vertCount; i++)
            {
                Vk::Gfx::Vertex v{};

                // position
                v.pos = {md.positions[i * 3 + 0], md.positions[i * 3 + 1], md.positions[i * 3 + 2]};

                // normal (fallback to up if missing)
                if (hasNormals)
                    v.normal = {md.normals[i * 3 + 0], md.normals[i * 3 + 1], md.normals[i * 3 + 2]};
                else
                    v.normal = {0.0f, 1.0f, 0.0f};

                // uv (fallback to 0,0)
                if (hasUVs)
                    v.uv = {md.texcoords[i * 2 + 0], md.texcoords[i * 2 + 1]};
                else
                    v.uv = {0.0f, 0.0f};

                // tangent (xyz = tangent, w = bitangent sign)
                if (hasTangents)
                    v.tangent = {md.tangents[i * 4 + 0], md.tangents[i * 4 + 1], md.tangents[i * 4 + 2], md.tangents[i * 4 + 3]};
                else
                {
                    // Cheap, orthonormal fallback tangent
                    glm::vec3 n = glm::normalize(glm::vec3(v.normal));
                    glm::vec3 arbitrary = (std::abs(n.y) < 0.999f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
                    glm::vec3 t = glm::normalize(arbitrary - n * glm::dot(n, arbitrary));
                    v.tangent = glm::vec4(t, 1.0f);
                }

                vertices.push_back(v);
            }

            // Create GPU mesh (allocates VkBuffers via VMA and uploads via staging)
            auto meshGpu = std::make_unique<Vk::Gfx::Mesh>();
            meshGpu->create(
                allocator,
                device,
                cmdPool,
                queue,
                vertices,
                md.indices,
                md.localTransform);

            gpuMeshes_.push_back(std::move(meshGpu));
        }

        // 4) Create a material and assign it to all draw items
        // NOTE: for now, paths are hardcoded; later read them from glTF materials.
        Render::MaterialDesc matDesc{};
        matDesc.baseColorPath = "assets/makarov/textures/makarov_baseColor.png";
        matDesc.normalPath = "assets/makarov/textures/makarov_normal.png";
        matDesc.mrPath = "assets/makarov/textures/makarov_metallicRoughness.png";

        auto matShared = materialSystem.createMaterial(matDesc);
        materials_.clear();
        materials_.push_back(matShared);

        // 5) Build draw list (mesh + material)
        drawItems_.clear();
        drawItems_.reserve(gpuMeshes_.size());
        for (auto &mPtr : gpuMeshes_)
        {
            drawItems_.push_back(Vk::Gfx::DrawItem{
                /*mesh*/ mPtr.get(),
                /*material*/ matShared.get()});
        }

        // 6) Compute world AABB for camera framing
        {
            std::vector<const Vk::Gfx::Mesh *> tmpList;
            tmpList.reserve(gpuMeshes_.size());
            for (auto &mPtr : gpuMeshes_)
                tmpList.push_back(mPtr.get());
            worldAaBb_ = computeWorldAABB(tmpList);
        }
    }

} // namespace Render
