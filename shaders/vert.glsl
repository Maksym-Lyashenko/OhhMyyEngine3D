#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 vNormal; // world-space
layout(location = 1) out vec3 vWorldPos;

layout(push_constant) uniform Push {
    mat4 model;
    mat4 view;
    mat4 proj;
} u;

void main() {
    mat4 M = u.model;
    mat4 V = u.view;
    mat4 P = u.proj;

    vec4 worldPos = M * vec4(inPos, 1.0);
    vWorldPos = worldPos.xyz;

    // normal в world space (без масштабов — через inverse-transpose)
    mat3 N = mat3(transpose(inverse(M)));
    vNormal = normalize(N * inNormal);

    gl_Position = P * V * worldPos;
}
