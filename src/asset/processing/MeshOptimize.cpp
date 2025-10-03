#include "asset/processing/MeshOptimize.h"
#include <meshoptimizer.h>
#include <cstddef>
#include <algorithm>

namespace Asset::Processing
{

    void OptimizeMeshInPlace(MeshData &md, const OptimizeSettings &s)
    {
        const size_t vertexCount = md.positions.size() / 3;
        const size_t indexCount = md.indices.size();
        if (vertexCount == 0 || indexCount == 0)
            return;

        // 1) Build a temporary interleaved vertex buffer (pos+normal+uv) so all passes
        //    (remap/overdraw/fetch) operate coherently on the same stream.
        struct Vtx
        {
            float px, py, pz;
            float nx, ny, nz;
            float u, v;
        };

        std::vector<Vtx> verts(vertexCount);
        const bool hasNormals = md.normals.size() == md.positions.size();
        const bool hasTexcoords = (md.texcoords.size() / 2) == vertexCount;

        for (size_t i = 0; i < vertexCount; ++i)
        {
            verts[i].px = md.positions[i * 3 + 0];
            verts[i].py = md.positions[i * 3 + 1];
            verts[i].pz = md.positions[i * 3 + 2];

            if (hasNormals)
            {
                verts[i].nx = md.normals[i * 3 + 0];
                verts[i].ny = md.normals[i * 3 + 1];
                verts[i].nz = md.normals[i * 3 + 2];
            }
            else
            {
                // Fallback if source mesh has no normals.
                verts[i].nx = 0.f;
                verts[i].ny = 1.f;
                verts[i].nz = 0.f;
            }

            if (hasTexcoords)
            {
                verts[i].u = md.texcoords[i * 2 + 0];
                verts[i].v = md.texcoords[i * 2 + 1];
            }
            else
            {
                verts[i].u = 0.f;
                verts[i].v = 0.f;
            }
        }

        // 2) Generate a vertex remap (removes duplicate vertices and compacts the vertex buffer).
        std::vector<unsigned int> remap(indexCount);
        const size_t newVertexCount = meshopt_generateVertexRemap(
            remap.data(),
            md.indices.data(), indexCount,
            verts.data(), vertexCount, sizeof(Vtx));

        // Apply remap to index and vertex buffers.
        std::vector<unsigned int> newIndices(indexCount);
        meshopt_remapIndexBuffer(newIndices.data(), md.indices.data(), indexCount, remap.data());

        std::vector<Vtx> newVerts(newVertexCount);
        meshopt_remapVertexBuffer(newVerts.data(), verts.data(), vertexCount, sizeof(Vtx), remap.data());

        verts.swap(newVerts);
        md.indices.assign(newIndices.begin(), newIndices.end());

        // 3) Pre-transform vertex cache optimization (triangle order).
        if (s.optimizeCache)
        {
            meshopt_optimizeVertexCache(md.indices.data(),
                                        md.indices.data(),
                                        md.indices.size(),
                                        verts.size());
        }

        // 4) Overdraw optimization (uses position stream).
        if (s.optimizeOverdraw)
        {
            meshopt_optimizeOverdraw(md.indices.data(),
                                     md.indices.data(),
                                     md.indices.size(),
                                     &verts[0].px, // pointer to first float3 position
                                     verts.size(),
                                     sizeof(Vtx),
                                     s.overdrawThreshold);
        }

        // 5) Post-transform vertex fetch optimization (reorders vertices to memory-friendly order).
        if (s.optimizeFetch)
        {
            meshopt_optimizeVertexFetch(verts.data(),
                                        md.indices.data(),
                                        md.indices.size(),
                                        verts.data(),
                                        verts.size(),
                                        sizeof(Vtx));
        }

        // 6) Optional triangle count reduction (LOD).
        if (s.simplify)
        {
            const size_t curIndexCount = md.indices.size();
            const size_t vertexCountNow = verts.size();

            // sanity: need triangles and vertices
            if (curIndexCount >= 3 && (curIndexCount % 3) == 0 && vertexCountNow > 0)
            {
                // target index count (multiple of 3 and really smaller than source)
                size_t target = static_cast<size_t>(double(curIndexCount) * s.simplifyTargetRatio);
                if (target < 3)
                    target = 3;
                target -= (target % 3);

                if (target >= curIndexCount)
                {
                    target = (curIndexCount >= 6) ? (curIndexCount - 3) : 3;
                }

                std::vector<unsigned int> lod(curIndexCount);

                size_t written = meshopt_simplify(
                    lod.data(),
                    md.indices.data(), curIndexCount,
                    &verts[0].px,
                    vertexCountNow,
                    sizeof(Vtx),
                    target,
                    s.simplifyError);

                if (written == 0)
                {
                    float err = std::max(s.simplifyError, 1e-2f);
                    written = meshopt_simplifySloppy(
                        lod.data(),
                        md.indices.data(), curIndexCount,
                        &verts[0].px,
                        vertexCountNow,
                        sizeof(Vtx),
                        target,
                        err);
                }

                if (written >= 3)
                    written -= (written % 3);
                else
                    written = 0;

                if (written >= 3)
                {
                    lod.resize(written);
                    md.indices.swap(lod);

                    meshopt_optimizeVertexCache(
                        md.indices.data(), md.indices.data(),
                        md.indices.size(), vertexCountNow);

                    meshopt_optimizeVertexFetch(
                        verts.data(),
                        md.indices.data(), md.indices.size(),
                        verts.data(), vertexCountNow, sizeof(Vtx));
                }
                // else: keep original indices
            }
        }

        // 7) De-interleave back into MeshData (SoA layout).
        const size_t vCount = verts.size();
        md.positions.resize(vCount * 3);
        md.normals.resize(vCount * 3);
        md.texcoords.resize(vCount * 2);

        for (size_t i = 0; i < vCount; ++i)
        {
            md.positions[i * 3 + 0] = verts[i].px;
            md.positions[i * 3 + 1] = verts[i].py;
            md.positions[i * 3 + 2] = verts[i].pz;

            md.normals[i * 3 + 0] = verts[i].nx;
            md.normals[i * 3 + 1] = verts[i].ny;
            md.normals[i * 3 + 2] = verts[i].nz;

            md.texcoords[i * 2 + 0] = verts[i].u;
            md.texcoords[i * 2 + 1] = verts[i].v;
        }
    }

} // namespace Asset::Processing
