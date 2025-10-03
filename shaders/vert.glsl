#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(push_constant) uniform PC {
    mat4 model;
} pc;

layout(set = 0, binding = 0) uniform ViewUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos; // xyz used
} ubo;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUV;

void main()
{
    gl_Position = ubo.viewProj * pc.model * vec4(inPos, 1.0);
    // normal: ignore non-uniform scale for now
    vNormal = mat3(pc.model) * inNormal;
    vUV = inUV;
}
