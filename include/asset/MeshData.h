#pragma once

#include <vector>
#include <cstdint>
#include <glm/ext/matrix_float4x4.hpp>

namespace Asset
{

    struct MeshData
    {
        std::vector<float> positions;  // xyzxyz...
        std::vector<float> normals;    // nxnynz...
        std::vector<float> texcoords;  // uvu vuv ...
        std::vector<uint32_t> indices; // triangle indices

        glm::mat4 localTransform{1.0f};
    };

} // namespace Asset