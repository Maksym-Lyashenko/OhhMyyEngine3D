#pragma once

#include "rhi/vk/gfx/Mesh.h"
#include "core/math/MathUtils.h"

#include <limits>
#include <vector>

namespace Vk::Gfx::Utils
{

    inline Core::MathUtils::AABB computeWorldAABB(const std::vector<const class Vk::Gfx::Mesh *> &meshes)
    {
        glm::vec3 mn(FLT_MAX);
        glm::vec3 mx(-FLT_MAX);
        for (auto *m : meshes)
        {
            Core::MathUtils::expandAABBByMat4(m->getMin(), m->getMax(), m->getLocalTransform(), mn, mx);
        }
        return {mn, mx};
    }

}