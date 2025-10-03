#pragma once

#include <vector>
#include <limits>
#include <glm/vec3.hpp>

#include "rhi/vk/gfx/Mesh.h"     // needs getMin()/getMax()/getLocalTransform()
#include "core/math/MathUtils.h" // Core::MathUtils::{AABB, expandAABBByMat4}

namespace Vk::Gfx::Utils
{

    /**
     * @brief Compute a world-space AABB for a list of GPU meshes.
     * Expects meshes to expose local AABB (min/max) and a local→world transform.
     */
    [[nodiscard]] inline Core::MathUtils::AABB
    computeWorldAABB(const std::vector<const Vk::Gfx::Mesh *> &meshes) noexcept
    {
        using Core::MathUtils::AABB;
        using Core::MathUtils::expandAABBByMat4;

        glm::vec3 mn(std::numeric_limits<float>::infinity());
        glm::vec3 mx(-std::numeric_limits<float>::infinity());

        for (const Vk::Gfx::Mesh *m : meshes)
        {
            if (!m)
                continue;
            expandAABBByMat4(m->getMin(), m->getMax(), m->getLocalTransform(), mn, mx);
        }

        return AABB{mn, mx};
    }

} // namespace Vk::Gfx::Utils
