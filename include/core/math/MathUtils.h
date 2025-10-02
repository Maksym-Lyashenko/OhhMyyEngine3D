#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cfloat> // FLT_MAX

namespace Core
{
    namespace MathUtils
    {

        struct AABB
        {
            glm::vec3 min;
            glm::vec3 max;
        };

        inline void expandAABBByMat4(const glm::vec3 &mn,
                                     const glm::vec3 &mx,
                                     const glm::mat4 &M,
                                     glm::vec3 &outMin,
                                     glm::vec3 &outMax)
        {
            const glm::vec3 c[8] = {
                {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z}, {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, {mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z}};
            for (int i = 0; i < 8; ++i)
            {
                const glm::vec3 w = glm::vec3(M * glm::vec4(c[i], 1.0f));
                outMin = glm::min(outMin, w);
                outMax = glm::max(outMax, w);
            }
        }

        // Возвращает минимальную дистанцию до цели, чтобы AABB точно поместился в кадр
        // при заданных fovY и aspect (учитывает и ширину, и высоту).
        inline float distanceToFit(const AABB &box, float fovY, float aspect)
        {
            const glm::vec3 size = box.max - box.min;
            const float halfW = 0.5f * glm::max(size.x, size.z);
            const float halfH = 0.5f * size.y;

            const float distY = halfH / std::tan(0.5f * fovY);
            const float distX = (halfW / aspect) / std::tan(0.5f * fovY);
            return glm::max(distX, distY);
        }

    } // namespace MathUtils

} // namespace Core
