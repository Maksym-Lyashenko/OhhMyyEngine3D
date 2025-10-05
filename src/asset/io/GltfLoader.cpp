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

namespace
{

    // Fill md.tangents (size = 4*V) either from geometry or leave empty if impossible.
    static void GenerateTangents(Asset::MeshData &md)
    {
        const size_t vcount = md.positions.size() / 3;
        if (vcount == 0 || md.indices.size() < 3)
            return;
        if (md.texcoords.size() != vcount * 2)
            return; // need UVs
        if (md.normals.size() != vcount * 3)
            return; // need normals

        std::vector<glm::vec3> T(vcount, glm::vec3(0.0f));
        std::vector<glm::vec3> B(vcount, glm::vec3(0.0f));

        auto V3 = [&](size_t i)
        {
            return glm::vec3(md.positions[3 * i + 0], md.positions[3 * i + 1], md.positions[3 * i + 2]);
        };
        auto N3 = [&](size_t i)
        {
            return glm::vec3(md.normals[3 * i + 0], md.normals[3 * i + 1], md.normals[3 * i + 2]);
        };
        auto UV2 = [&](size_t i)
        {
            return glm::vec2(md.texcoords[2 * i + 0], md.texcoords[2 * i + 1]);
        };

        // accumulate per-triangle
        for (size_t i = 0; i + 2 < md.indices.size(); i += 3)
        {
            const uint32_t i0 = md.indices[i + 0];
            const uint32_t i1 = md.indices[i + 1];
            const uint32_t i2 = md.indices[i + 2];

            const glm::vec3 p0 = V3(i0), p1 = V3(i1), p2 = V3(i2);
            const glm::vec2 w0 = UV2(i0), w1 = UV2(i1), w2 = UV2(i2);

            const glm::vec3 dp1 = p1 - p0;
            const glm::vec3 dp2 = p2 - p0;
            const glm::vec2 duv1 = w1 - w0;
            const glm::vec2 duv2 = w2 - w0;

            const float denom = duv1.x * duv2.y - duv1.y * duv2.x;
            if (std::abs(denom) < 1e-8f)
                continue;
            const float r = 1.0f / denom;

            const glm::vec3 t = (dp1 * duv2.y - dp2 * duv1.y) * r;
            const glm::vec3 b = (dp2 * duv1.x - dp1 * duv2.x) * r;

            T[i0] += t;
            T[i1] += t;
            T[i2] += t;
            B[i0] += b;
            B[i1] += b;
            B[i2] += b;
        }

        // orthonormalize + handedness
        md.tangents.resize(vcount * 4);
        for (size_t i = 0; i < vcount; ++i)
        {
            glm::vec3 n = glm::normalize(N3(i));
            glm::vec3 t = T[i];
            if (glm::dot(t, t) < 1e-12f)
            {
                // fallback if degenerate — pick any axis not collinear with N
                glm::vec3 ref = (std::abs(n.z) < 0.999f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
                t = glm::normalize(glm::cross(ref, n));
            }
            t = glm::normalize(t - n * glm::dot(n, t));
            glm::vec3 b = glm::cross(n, t);
            float w = (glm::dot(b, B[i]) < 0.0f) ? -1.0f : 1.0f;

            md.tangents[4 * i + 0] = t.x;
            md.tangents[4 * i + 1] = t.y;
            md.tangents[4 * i + 2] = t.z;
            md.tangents[4 * i + 3] = w;
        }
    }

} // namespace

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

                    case cgltf_attribute_type_tangent:
                        if (isVecN(acc, cgltf_type_vec4, 4))
                        {
                            copyAttributeFloats(acc, md.tangents); // 4 * V (x,y,z,w)
                        }
                        else
                        {
                            Logger::log(LogLevel::WARNING, "TANGENT accessor is not vec4; skipping.");
                        }
                        break;

                    default:
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

                if (md.tangents.empty() && !md.normals.empty() && !md.texcoords.empty())
                {
                    GenerateTangents(md);
                }

                meshes.push_back(std::move(md));
            }
        }

        return meshes;
    }

} // namespace Asset
