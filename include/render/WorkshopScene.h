#pragma once

#include "render/Scene.h"

// Builds a small "workshop" room entirely in code, without external assets.
// Geometry:
//   - 1 large floor quad in XZ plane (y=0).
//   - 2 walls (left & right) as quads in YZ planes.
// Materials: reuses MaterialSystem (can be simple albedo-only).
namespace Render
{
    class WorkshopScene final : public Scene
    {
    public:
        // Build the scene into GPU using your existing upload path:
        // - allocates vertex/index buffers with VMA via Vk::Gfx::Mesh
        // - registers DrawItems (mesh + material) inside Scene
        // - computes world AABB for camera framing
        void build(VmaAllocator allocator,
                   VkDevice device,
                   VkCommandPool cmdPool,
                   VkQueue queue,
                   MaterialSystem &materialSystem);

    private:
        // Helpers to create simple quads procedurally.
        // Returned vectors are interleaved to your Vk::Gfx::Vertex layout.
        static void makePlaneXZ(float halfX, float halfZ, float y,
                                std::vector<Vk::Gfx::Vertex> &outVerts,
                                std::vector<uint32_t> &outIdx,
                                bool flipWinding = false);

        static void makeWallYZ(float x, float halfY, float halfZ,
                               std::vector<Vk::Gfx::Vertex> &outVerts,
                               std::vector<uint32_t> &outIdx,
                               bool faceToCenter);
    };
}
