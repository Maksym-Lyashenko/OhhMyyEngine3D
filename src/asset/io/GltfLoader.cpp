#include "asset/io/GltfLoader.h"

#include "core/Logger.h"

#include <cgltf.h>
#include <stdexcept>
#include <memory>
#include <string_view>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using Core::Logger;
using Core::LogLevel;

namespace Asset
{

    // --- internal helpers (do not leak to header) ---
    namespace
    {

        struct CgltfDeleter
        {
            void operator()(cgltf_data *d) const noexcept
            {
                if (d)
                    cgltf_free(d);
            }
        };
        using CgltfPtr = std::unique_ptr<cgltf_data, CgltfDeleter>;

        inline bool isVecN(const cgltf_accessor *acc, cgltf_type type, int comps)
        {
            return acc && acc->type == type && cgltf_num_components(acc->type) == comps;
        }

        void copyAttributeFloats(const cgltf_accessor *accessor, std::vector<float> &dst)
        {
            const cgltf_size count = accessor->count * cgltf_num_components(accessor->type);
            dst.resize(static_cast<size_t>(count));
            cgltf_accessor_unpack_floats(accessor, dst.data(), count);
        }

        void copyIndices(const cgltf_accessor *accessor, std::vector<uint32_t> &dst)
        {
            dst.resize(static_cast<size_t>(accessor->count));
            for (cgltf_size i = 0; i < accessor->count; ++i)
            {
                const cgltf_size idx = cgltf_accessor_read_index(accessor, i);
                dst[static_cast<size_t>(i)] = static_cast<uint32_t>(idx);
            }
        }

        glm::mat4 nodeWorldTransform(const cgltf_node *node)
        {
            if (!node)
                return glm::mat4(1.0f);

            // Prefer cgltf_node_transform_world (handles hierarchy)
            cgltf_float m[16];
            cgltf_node_transform_world(node, m);
            // glTF is column-major; glm::make_mat4 expects column-major.
            return glm::make_mat4(m);
        }

    } // namespace

    // --- public API ---
    std::vector<MeshData> GltfLoader::loadMeshes(const std::string &path)
    {
        cgltf_options options{};
        cgltf_data *raw = nullptr;

        const cgltf_result parsed = cgltf_parse_file(&options, path.c_str(), &raw);
        if (parsed != cgltf_result_success)
        {
            throw std::runtime_error("Failed to parse glTF: " + path);
        }

        CgltfPtr data{raw};
        const cgltf_result loaded = cgltf_load_buffers(&options, data.get(), path.c_str());
        if (loaded != cgltf_result_success)
        {
            throw std::runtime_error("Failed to load glTF buffers: " + path);
        }

        std::vector<MeshData> meshes;
        meshes.reserve(data->meshes_count > 0 ? static_cast<size_t>(data->meshes_count) : 1u);

        // Iterate nodes → mesh primitives
        for (cgltf_size ni = 0; ni < data->nodes_count; ++ni)
        {
            const cgltf_node *node = &data->nodes[ni];
            if (!node || !node->mesh)
                continue;

            const glm::mat4 nodeXf = nodeWorldTransform(node);
            const cgltf_mesh &mesh = *node->mesh;

            for (cgltf_size pi = 0; pi < mesh.primitives_count; ++pi)
            {
                const cgltf_primitive &prim = mesh.primitives[pi];

                if (prim.type != cgltf_primitive_type_triangles)
                {
                    Logger::log(LogLevel::WARNING, "Skipping non-triangle primitive at node #" + std::to_string(ni));
                    continue;
                }

                MeshData md{};
                md.localTransform = nodeXf;

                // Attributes
                for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai)
                {
                    const cgltf_attribute &attr = prim.attributes[ai];
                    const cgltf_accessor *acc = attr.data;
                    if (!acc)
                        continue;

                    switch (attr.type)
                    {
                    case cgltf_attribute_type_position:
                        if (isVecN(acc, cgltf_type_vec3, 3))
                        {
                            copyAttributeFloats(acc, md.positions);
                        }
                        else
                        {
                            Logger::log(LogLevel::WARNING, "POSITION accessor is not vec3; skipping.");
                        }
                        break;

                    case cgltf_attribute_type_normal:
                        if (isVecN(acc, cgltf_type_vec3, 3))
                        {
                            copyAttributeFloats(acc, md.normals);
                        }
                        else
                        {
                            // Not critical — many assets omit normals or use different layout
                            Logger::log(LogLevel::WARNING, "NORMAL accessor is not vec3; skipping.");
                        }
                        break;

                    case cgltf_attribute_type_texcoord:
                        if (attr.index == 0)
                        {
                            if (isVecN(acc, cgltf_type_vec2, 2))
                            {
                                copyAttributeFloats(acc, md.texcoords);
                            }
                            else
                            {
                                Logger::log(LogLevel::WARNING, "TEXCOORD_0 accessor is not vec2; skipping.");
                            }
                        }
                        break;

                    default:
                        // ignore tangents/colors/extra sets for now
                        break;
                    }
                }

                // Indices (or generate a trivial 0..N-1)
                if (prim.indices)
                {
                    copyIndices(prim.indices, md.indices);
                }
                else
                {
                    const size_t vcount = md.positions.size() / 3;
                    md.indices.resize(vcount);
                    for (size_t i = 0; i < vcount; ++i)
                        md.indices[i] = static_cast<uint32_t>(i);
                }

                // Minimal sanity: ensure positions exist and index count % 3 == 0 for triangles
                if (md.positions.empty())
                {
                    Logger::log(LogLevel::WARNING, "Mesh primitive has no POSITION; skipping.");
                    continue;
                }
                if (md.indices.size() % 3 != 0)
                {
                    Logger::log(LogLevel::WARNING, "Index count is not a multiple of 3; primitive may be invalid.");
                }

                meshes.push_back(std::move(md));
            }
        }

        return meshes;
    }

} // namespace Asset
