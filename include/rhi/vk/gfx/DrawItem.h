#pragma once
#include "rhi/vk/gfx/Mesh.h"

namespace Render
{
    class Material;
}

namespace Vk::Gfx
{
    class Mesh;

    struct DrawItem
    {
        const Vk::Gfx::Mesh *mesh{nullptr};
        const Render::Material *material{nullptr};
        // NOTE: We take the model matrix from mesh->getLocalTransform(), so there is no field here.
    };

}