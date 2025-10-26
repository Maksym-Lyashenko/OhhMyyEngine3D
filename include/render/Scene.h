#pragma once

#include <memory>
#include <vector>
#include <string>

#include <glm/mat4x4.hpp>

#include "rhi/vk/gfx/Mesh.h"
#include "rhi/vk/gfx/DrawItem.h"
#include "render/materials/MaterialSystem.h"
#include "asset/io/GltfLoader.h"
#include "asset/processing/MeshOptimize.h"
#include "core/math/MathUtils.h"

namespace Render
{

    /**
     * @brief Scene is responsible for loading CPU-side assets (meshes/materials),
     *        uploading them to GPU (vertex/index buffers, descriptor sets),
     *        and preparing a list of DrawItem for rendering.
     *
     * VulkanRenderer should not care how the scene was built,
     * it should just get "what to draw".
     */
    class Scene
    {
    public:
        Scene() = default;
        ~Scene() = default;

        Scene(const Scene &) = delete;
        Scene &operator=(const Scene &) = delete;

        /**
         * @brief Load a glTF model from disk, optimize meshes, upload meshes to GPU,
         *        and create materials for them.
         *
         * @param gltfPath path to .gltf or .glb
         * @param physDevice VkPhysicalDevice for buffer allocations
         * @param device     VkDevice for buffer allocations / descriptor creation
         * @param materialSystem MaterialSystem (already initialized)
         *
         * This will fill internal gpuMeshes_, drawItems_, and worldAabb_.
         */

        void loadModel(
            const std::string &gltfPath,
            VkPhysicalDevice physDevice,
            VkDevice device,
            MaterialSystem &materialSystem);

        /**
         * @brief Get draw list for rendering.
         * This is stable pointer refs to Mesh + Material pairs.
         */
        const std::vector<Vk::Gfx::DrawItem> &drawItems() const noexcept { return drawItems_; }

        /**
         * @brief Returns world-space bounding box of all meshes in this scene.
         * Used for camera framing.
         */
        const Core::MathUtils::AABB &worldBounds() const noexcept { return worldAaBb_; }

    private:
        // All GPU meshes owned by the Scene
        std::vector<std::unique_ptr<Vk::Gfx::Mesh>> gpuMeshes_;

        // Materials owned by the Scene (shared_ptr<IMaterial>)
        std::vector<std::shared_ptr<Render::Material>> materials_;

        // Final "what to draw" list (non-owning pointers into gpuMeshes_ and materials_)
        std::vector<Vk::Gfx::DrawItem> drawItems_;

        // Cached world bounds of the whole scene
        Core::MathUtils::AABB worldAaBb_{};
    };

} // namespace Render