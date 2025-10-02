#pragma once

#include <cgltf.h>

#include <string>
#include <vector>

#include "asset/MeshData.h"

namespace Asset
{

    class GltfLoader
    {
    public:
        static std::vector<MeshData> loadMeshes(const std::string &path);

    private:
        static void copyAttribute(const cgltf_accessor *accessor, std::vector<float> &dst);
        static void copyIndices(const cgltf_accessor *accessor, std::vector<uint32_t> &dst);
        static glm::mat4 getNodeTransform(const cgltf_node *node);
    };

} // namespace Asset