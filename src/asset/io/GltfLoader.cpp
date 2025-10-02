#include "asset/io/GltfLoader.h"

#include "core/Logger.h"

#include <stdexcept>

#include <glm/gtc/type_ptr.hpp>

namespace Asset
{

    void GltfLoader::copyAttribute(const cgltf_accessor *accessor, std::vector<float> &dst)
    {
        dst.resize(accessor->count * cgltf_num_components(accessor->type));
        cgltf_accessor_unpack_floats(accessor, dst.data(), dst.size());
    }

    void GltfLoader::copyIndices(const cgltf_accessor *accessor, std::vector<uint32_t> &dst)
    {
        dst.resize(accessor->count);
        for (cgltf_size i = 0; i < accessor->count; ++i)
        {
            cgltf_size idx = cgltf_accessor_read_index(accessor, i);
            dst[i] = static_cast<uint32_t>(idx);
        }
    }

    glm::mat4 GltfLoader::getNodeTransform(const cgltf_node *node)
    {
        glm::mat4 t(1.0f);
        if (!node)
            return t;

        if (node->has_matrix)
        {
            t = glm::make_mat4(node->matrix); // row-major
        }
        else
        {
            glm::mat4 trans = glm::translate(glm::mat4(1.0f),
                                             glm::vec3(node->translation[0], node->translation[1], node->translation[2]));
            glm::quat q(node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]);
            glm::mat4 rot = glm::mat4_cast(q);
            glm::mat4 scl = glm::scale(glm::mat4(1.0f),
                                       glm::vec3(node->scale[0], node->scale[1], node->scale[2]));
            t = trans * rot * scl;
        }
        return t;
    }

    std::vector<MeshData> GltfLoader::loadMeshes(const std::string &path)
    {
        cgltf_options options = {};
        cgltf_data *data = nullptr;
        cgltf_result res = cgltf_parse_file(&options, path.c_str(), &data);
        if (res != cgltf_result_success)
            throw std::runtime_error("Failed to parse glTF: " + path);

        res = cgltf_load_buffers(&options, data, path.c_str());
        if (res != cgltf_result_success)
        {
            cgltf_free(data);
            throw std::runtime_error("Failed to load glTF buffers: " + path);
        }

        std::vector<MeshData> meshes;

        // glTF nodes → meshes
        for (cgltf_size ni = 0; ni < data->nodes_count; ++ni)
        {
            const cgltf_node &node = data->nodes[ni];
            if (!node.mesh)
                continue;

            cgltf_float mat[16];
            cgltf_node_transform_world(&node, mat);
            glm::mat4 nodeTransform = glm::make_mat4(mat);

            const cgltf_mesh &mesh = *node.mesh;
            for (cgltf_size pi = 0; pi < mesh.primitives_count; ++pi)
            {
                const cgltf_primitive &prim = mesh.primitives[pi];
                MeshData md;
                md.localTransform = nodeTransform;

                for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai)
                {
                    const cgltf_attribute &attr = prim.attributes[ai];
                    if (attr.type == cgltf_attribute_type_position)
                        copyAttribute(attr.data, md.positions);
                    else if (attr.type == cgltf_attribute_type_normal)
                        copyAttribute(attr.data, md.normals);
                    else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0)
                        copyAttribute(attr.data, md.texcoords);
                }

                if (prim.indices)
                    copyIndices(prim.indices, md.indices);
                else
                {
                    size_t vtxCount = md.positions.size() / 3;
                    md.indices.resize(vtxCount);
                    for (size_t i = 0; i < vtxCount; ++i)
                        md.indices[i] = (uint32_t)i;
                }

                meshes.push_back(std::move(md));
            }
        }

        cgltf_free(data);
        return meshes;
    }

} // namespace Asset
