// include/render/ViewUniforms.h
#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace Render
{
    struct alignas(16) ViewUniforms
    {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 viewProj;
        glm::vec4 cameraPos; // xyz + pad
    };

    static_assert(sizeof(ViewUniforms) % 16 == 0, "UBO must be 16-byte aligned");

} // namespace Render
