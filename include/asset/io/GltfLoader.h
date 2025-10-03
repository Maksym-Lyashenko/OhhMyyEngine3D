#pragma once

#include <string>
#include <vector>

#include "asset/MeshData.h"

namespace Asset
{

    /**
     * @brief Loads meshes from a glTF file into CPU-side MeshData containers.
     *
     * - Supports positions, normals, and TEXCOORD_0 (if present).
     * - Fills MeshData::indices (uses sequential 0..N-1 if primitive has no indices).
     * - Writes node's world transform into MeshData::localTransform.
     * - Only triangle primitives are imported; others are skipped with a warning.
     */
    class GltfLoader
    {
    public:
        /// Load meshes from a glTF file at @p path. Throws std::runtime_error on failure.
        [[nodiscard]] static std::vector<MeshData> loadMeshes(const std::string &path);
    };

} // namespace Asset
