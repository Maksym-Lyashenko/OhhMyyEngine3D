#include "render/WorkshopScene.h"

#include "rhi/vk/gfx/utils/MeshUtils.h" // only for AABB compute you already use

namespace Render
{
    // ------------------------------------------------------------
    // Create a rectangular floor quad in XZ at constant Y.
    // Vertex layout matches Vk::Gfx::Vertex: pos, normal, uv, tangent.w
    // ------------------------------------------------------------
    void WorkshopScene::makePlaneXZ(float halfX, float halfZ, float y,
                                    std::vector<Vk::Gfx::Vertex> &outVerts,
                                    std::vector<uint32_t> &outIdx,
                                    bool flipWinding)
    {
        outVerts.clear();
        outIdx.clear();
        outVerts.resize(4);

        // Positions (counter-clockwise when looking from +Y)
        glm::vec3 p0(-halfX, y, -halfZ);
        glm::vec3 p1(halfX, y, -halfZ);
        glm::vec3 p2(halfX, y, halfZ);
        glm::vec3 p3(-halfX, y, halfZ);

        // Normal up
        glm::vec3 n(0.0f, 1.0f, 0.0f);
        // Tangent along +X, bitangent sign = +1
        glm::vec3 t(1.0f, 0.0f, 0.0f);
        float btSign = 1.0f;

        auto put = [&](int i, const glm::vec3 &pos, const glm::vec2 &uv)
        {
            Vk::Gfx::Vertex v{};
            v.pos = {pos.x, pos.y, pos.z};
            v.normal = {n.x, n.y, n.z};
            v.uv = {uv.x, uv.y};
            v.tangent = {t.x, t.y, t.z, btSign};
            outVerts[i] = v;
        };

        // Basic tiling UVs (fit to quad size so it’s visually obvious under lighting)
        put(0, p0, {0.0f, 0.0f});
        put(1, p1, {1.0f, 0.0f});
        put(2, p2, {1.0f, 1.0f});
        put(3, p3, {0.0f, 1.0f});

        // Two triangles
        if (!flipWinding)
            outIdx = {0, 1, 2, 0, 2, 3};
        else
            outIdx = {0, 2, 1, 0, 3, 2};
    }

    // ------------------------------------------------------------
    // Create a rectangular wall quad in YZ at constant X.
    // faceToCenter=true -> normal points toward -X (center at 0)
    // faceToCenter=false -> normal points to +X
    // ------------------------------------------------------------
    void WorkshopScene::makeWallYZ(float x, float halfY, float halfZ,
                                   std::vector<Vk::Gfx::Vertex> &outVerts,
                                   std::vector<uint32_t> &outIdx,
                                   bool faceToCenter)
    {
        outVerts.clear();
        outIdx.clear();
        outVerts.resize(4);

        // Positions (counter-clockwise when looking along wall normal)
        glm::vec3 p0(x, -halfY, -halfZ);
        glm::vec3 p1(x, halfY, -halfZ);
        glm::vec3 p2(x, halfY, halfZ);
        glm::vec3 p3(x, -halfY, halfZ);

        // Normal along +/-X
        glm::vec3 n = faceToCenter ? glm::vec3(-1, 0, 0) : glm::vec3(1, 0, 0);
        // Tangent along +Z so normal maps look consistent; bitangent sign +1
        glm::vec3 t(0.0f, 0.0f, 1.0f);
        float btSign = 1.0f;

        auto put = [&](int i, const glm::vec3 &pos, const glm::vec2 &uv)
        {
            Vk::Gfx::Vertex v{};
            v.pos = {pos.x, pos.y, pos.z};
            v.normal = {n.x, n.y, n.z};
            v.uv = {uv.x, uv.y};
            v.tangent = {t.x, t.y, t.z, btSign};
            outVerts[i] = v;
        };

        // Simple UVs
        put(0, p0, {0.0f, 0.0f});
        put(1, p1, {0.0f, 1.0f});
        put(2, p2, {1.0f, 1.0f});
        put(3, p3, {1.0f, 0.0f});

        // Winding must match the chosen normal direction:
        // If faceToCenter -> we look from +X toward -X, so CCW is (0,1,2),(0,2,3).
        // If not faceToCenter -> we look from -X toward +X, flip indices.
        if (faceToCenter)
            outIdx = {0, 1, 2, 0, 2, 3};
        else
            outIdx = {0, 2, 1, 0, 3, 2};
    }

    void WorkshopScene::build(VmaAllocator allocator,
                              VkDevice device,
                              VkCommandPool cmdPool,
                              VkQueue queue,
                              MaterialSystem &materialSystem)
    {
        // Clear any previous GPU content
        gpuMeshes_.clear();
        drawItems_.clear();
        materials_.clear();

        // -----------------------------
        // 1) Build geometry (CPU side)
        // -----------------------------
        // Room size ~ 20x20 meters, wall height ~ 6m.
        const float roomHalfX = 10.0f;
        const float roomHalfZ = 10.0f;
        const float wallHalfY = 3.0f;

        std::vector<Vk::Gfx::Vertex> vtx;
        std::vector<uint32_t> idx;

        // Floor at y=0
        makePlaneXZ(roomHalfX, roomHalfZ, 0.0f, vtx, idx, /*flipWinding=*/true);
        auto floorMesh = std::make_unique<Vk::Gfx::Mesh>();
        floorMesh->create(allocator, device, cmdPool, queue, vtx, idx, glm::mat4(1.0f), "Workshop_Floor");

        // Left wall at x = -roomHalfX, facing center (+X normal? we want it to face inward)
        makeWallYZ(-roomHalfX, wallHalfY, roomHalfZ, vtx, idx, /*faceToCenter=*/true /*normal +X*/);
        auto leftWallMesh = std::make_unique<Vk::Gfx::Mesh>();
        leftWallMesh->create(allocator, device, cmdPool, queue, vtx, idx, glm::mat4(1.0f), "Workshop_Wall_L");

        // Right wall at x = +roomHalfX, facing center (-X normal)
        makeWallYZ(+roomHalfX, wallHalfY, roomHalfZ, vtx, idx, /*faceToCenter=*/false /*normal -X*/);
        auto rightWallMesh = std::make_unique<Vk::Gfx::Mesh>();
        rightWallMesh->create(allocator, device, cmdPool, queue, vtx, idx, glm::mat4(1.0f), "Workshop_Wall_R");

        // (Optional) Add a small box at center to catch highlights
        // Skipped for brevity—floor + walls are enough to validate lights.

        // -----------------------------------
        // 2) Create simple materials
        // -----------------------------------
        // NOTE:
        //   Your MaterialSystem currently expects textures. For a quick start,
        //   you can reuse any existing textures (e.g., baseColor = a gray tile).
        //   If empty paths are supported with a default fallback in shader,
        //   leave them empty; otherwise reuse the makarov textures for now.

        MaterialDesc floorMat{};
        // If you have a neutral gray albedo texture, put it here:
        // floorMat.baseColorPath = "assets/common/grey.png";
        // For now reuse what you already have:
        floorMat.baseColorPath = "assets/textures/ceramic_floor/Poliigon_TilesCeramicWhite_6956_BaseColor.jpg";
        floorMat.normalPath = "assets/textures/ceramic_floor/Poliigon_TilesCeramicWhite_6956_Normal.png";
        floorMat.metallicPath = "assets/textures/ceramic_floor/Poliigon_TilesCeramicWhite_6956_Metallic.jpg";
        floorMat.roughnessPath = "assets/textures/ceramic_floor/Poliigon_TilesCeramicWhite_6956_Roughness.jpg";
        floorMat.occlusionPath = "assets/textures/ceramic_floor/Poliigon_TilesCeramicWhite_6956_AmbientOcclusion.jpg";

        floorMat.params.uvTiling = {4.0f, 4.0f};

        auto floorMtl = materialSystem.createMaterial(floorMat);

        MaterialDesc wallMat = floorMat; // same set is OK for testing
        auto wallMtl = materialSystem.createMaterial(wallMat);

        materials_.push_back(floorMtl);
        materials_.push_back(wallMtl);

        // -----------------------------------
        // 3) Register draw items
        // -----------------------------------
        drawItems_.push_back(Vk::Gfx::DrawItem{floorMesh.get(), floorMtl.get()});
        drawItems_.push_back(Vk::Gfx::DrawItem{leftWallMesh.get(), wallMtl.get()});
        drawItems_.push_back(Vk::Gfx::DrawItem{rightWallMesh.get(), wallMtl.get()});

        // Keep ownership of meshes in the Scene
        gpuMeshes_.push_back(std::move(floorMesh));
        gpuMeshes_.push_back(std::move(leftWallMesh));
        gpuMeshes_.push_back(std::move(rightWallMesh));

        // -----------------------------------
        // 4) Compute world AABB for cameras
        // -----------------------------------
        std::vector<const Vk::Gfx::Mesh *> tmp;
        tmp.reserve(gpuMeshes_.size());
        for (auto &m : gpuMeshes_)
            tmp.push_back(m.get());
        worldAaBb_ = Vk::Gfx::Utils::computeWorldAABB(tmp);
    }
}
