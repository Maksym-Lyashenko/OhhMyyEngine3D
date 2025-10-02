#pragma once

#include <glm/glm.hpp>

namespace Render
{
    struct MVP
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 viewProj;
    };

} // namespace Render