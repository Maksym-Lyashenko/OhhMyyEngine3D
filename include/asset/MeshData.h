#pragma once

#include <cstdint>
#include <vector>
#include <glm/mat4x4.hpp>

namespace Asset
{

    /**
     * @brief CPU-side mesh container used by importers and uploaders.
     *
     * Layout is SoA (structure-of-arrays) to simplify direct GPU uploads:
     * - positions: xyzxyz... (size = 3 * vertexCount)
     * - normals:   nxnynz... (optional; size == positions.size())
     * - texcoords: uvuv...   (optional; size = 2 * vertexCount)
     * - indices:   32-bit triangle indices (3 * triangleCount)
     *
     * localTransform stores the node's world transform from the source asset.
     * You can choose to bake it on upload or keep it separate.
     */
    struct MeshData
    {
        using Index = std::uint32_t;

        std::vector<float> positions; // xyzxyz...
        std::vector<float> normals;   // nxnynz... (optional)
        std::vector<float> texcoords; // uvuv...   (optional)
        std::vector<Index> indices;   // triangle indices (3*i)

        glm::mat4 localTransform{1.0f};

        // ---- Convenience API (header-only, noexcept) ----
        [[nodiscard]] std::size_t vertexCount() const noexcept { return positions.size() / 3; }
        [[nodiscard]] std::size_t indexCount() const noexcept { return indices.size(); }
        [[nodiscard]] bool hasNormals() const noexcept { return normals.size() == positions.size(); }
        [[nodiscard]] bool hasTexcoord0() const noexcept { return texcoords.size() == vertexCount() * 2; }
        [[nodiscard]] bool empty() const noexcept { return positions.empty(); }

        // Clear all arrays; keep capacity as-is (use shrink_to_fit() if needed).
        void clear() noexcept
        {
            positions.clear();
            normals.clear();
            texcoords.clear();
            indices.clear();
            localTransform = glm::mat4(1.0f);
        }

        // Optional: reduce memory after import
        void shrink_to_fit()
        {
            positions.shrink_to_fit();
            normals.shrink_to_fit();
            texcoords.shrink_to_fit();
            indices.shrink_to_fit();
        }
    };

} // namespace Asset
