#pragma once

#include <glm/glm.hpp>
#include <cstdint>

// Matches GLSL DirectionalLight (std430)
struct DirectionalLightGPU
{
    glm::vec4 direction_ws; // xyz direction (normalized), w padding
    glm::vec4 radiance;     // rgb radiance (linear), a padding
};

// Matches GLSL PointLight (std430)
struct PointLightGPU
{
    glm::vec4 position_ws; // xyz pos, w padding
    glm::vec4 color_range; // rgb color (linear), a = range (meters)
};

// Matches GLSL SpotLight (std430)
struct SpotLightGPU
{
    glm::vec4 position_range;  // xyz pos, w = range
    glm::vec4 direction_inner; // xyz dir (normalized), w = cos(inner)
    glm::vec4 color_outer;     // rgb color, w = cos(outer)
};

// std140 UBO (counts/config/ambient). Keep 16-byte alignment.
struct LightingCountsUBO
{
    alignas(16) glm::uvec4 counts_flags; // x=dirCount, y=pointCount, z=spotCount, w=flags
    alignas(16) glm::vec4 ambientRGBA;   // rgb used
};

// ===== static_asserts (сломаются — сразу видно где не так) =====
static_assert(sizeof(DirectionalLightGPU) == 32, "DirLight size");
static_assert(sizeof(PointLightGPU) == 32, "PointLight size");
static_assert(sizeof(SpotLightGPU) == 48, "SpotLight size"); // 3*vec4
static_assert(offsetof(LightingCountsUBO, counts_flags) == 0, "counts_flags");
static_assert(offsetof(LightingCountsUBO, ambientRGBA) == 16, "ambientRGBA");
static_assert(sizeof(LightingCountsUBO) == 32, "LightingCountsUBO size");
