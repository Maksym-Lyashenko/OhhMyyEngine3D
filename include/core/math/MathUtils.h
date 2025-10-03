#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <limits>
#include <cmath>

namespace Core
{
    namespace MathUtils
    {

        /**
         * @brief Axis-aligned bounding box in world space.
         */
        struct AABB
        {
            glm::vec3 min{};
            glm::vec3 max{};

            /// Create an empty AABB (min = +inf, max = -inf).
            static AABB makeEmpty() noexcept
            {
                const float inf = std::numeric_limits<float>::infinity();
                return {glm::vec3(inf), glm::vec3(-inf)};
            }

            /// Expand by a point.
            void expandBy(const glm::vec3 &p) noexcept
            {
                min = glm::min(min, p);
                max = glm::max(max, p);
            }

            /// Expand by another AABB.
            void expandBy(const AABB &b) noexcept
            {
                min = glm::min(min, b.min);
                max = glm::max(max, b.max);
            }
        };

        /**
         * @brief Expand an AABB [outMin,outMax] by the 8 transformed corners of box [mn,mx] with matrix M.
         */
        inline void expandAABBByMat4(const glm::vec3 &mn,
                                     const glm::vec3 &mx,
                                     const glm::mat4 &M,
                                     glm::vec3 &outMin,
                                     glm::vec3 &outMax) noexcept
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

        /**
         * @brief Transform a local-space AABB by matrix M and return its world-space AABB.
         */
        [[nodiscard]] inline AABB transformAABB(const AABB &local, const glm::mat4 &M) noexcept
        {
            glm::vec3 mn(std::numeric_limits<float>::infinity());
            glm::vec3 mx(-std::numeric_limits<float>::infinity());
            expandAABBByMat4(local.min, local.max, M, mn, mx);
            return {mn, mx};
        }

        /**
         * @brief Minimal distance from camera to fully fit @p box within vertical fov @p fovY and @p aspect.
         * @param fovY  Vertical field-of-view in **radians**.
         * @param aspect Width/Height.
         */
        [[nodiscard]] inline float distanceToFit(const AABB &box, float fovY, float aspect) noexcept
        {
            const glm::vec3 size = box.max - box.min;
            const float halfW = 0.5f * glm::max(size.x, size.z); // XZ footprint
            const float halfH = 0.5f * size.y;

            const float distY = halfH / std::tan(0.5f * fovY);
            const float distX = (halfW / aspect) / std::tan(0.5f * fovY);
            return glm::max(distX, distY);
        }

    } // namespace MathUtils
} // namespace Core
