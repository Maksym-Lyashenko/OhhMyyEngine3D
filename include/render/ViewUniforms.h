#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Render
{

    struct ViewUniforms
    {
        glm::mat4 view{};
        glm::mat4 proj{};
        glm::mat4 viewProj{};
        glm::vec3 cameraPos{};
    };

} // namespace Render