#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "asset/MeshData.h"

namespace Asset::Processing
{

    struct OptimizeSettings
    {
        // Reorder triangles to reduce overdraw using position data.
        bool optimizeOverdraw = true;
        // Overdraw threshold; 1.05–1.2 is a sensible range.
        float overdrawThreshold = 1.05f;

        // Reorder vertices/indices to improve post-transform vertex cache locality.
        bool optimizeFetch = true;

        // Reorder indices to improve pre-transform vertex cache locality.
        bool optimizeCache = true;

        // Optional triangle count reduction (LOD) using error metric.
        bool simplify = false;
        // Target index count as a ratio of the original (e.g., 0.5 = 50%).
        float simplifyTargetRatio = 0.5f;
        // Allowed geometric error for simplification.
        float simplifyError = 1e-2f;
    };

    // Optimizes MeshData in-place.
    // Expected layout:
    //   positions: xyz xyz ...
    //   normals:   nx ny nz ... (same count as positions; will be generated if missing)
    //   texcoords: u v u v ...
    void OptimizeMeshInPlace(MeshData &md, const OptimizeSettings &s = {});

} // namespace Asset::Processing
