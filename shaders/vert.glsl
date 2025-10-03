#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

// set=0, binding=0 — UBO (как мы настроили в пайплайне)
layout(set = 0, binding = 0) uniform ViewUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos;
} uView;

// push-constants — только model
layout(push_constant) uniform ObjectPC { mat4 model; } pc;

// ВЫХОДЫ в FS — ДОЛЖНЫ существовать, если FS их читает!
layout(location = 0) out vec3 vNormalWS;
layout(location = 1) out vec3 vPosWS;

void main()
{
    mat4 model = pc.model;

    vec4 posWS = model * vec4(inPos, 1.0);
    vPosWS     = posWS.xyz;

    // если нормали в object space — умножаем на inverse-transpose(model)
    mat3 nrmMat = mat3(transpose(inverse(model)));
    vNormalWS   = normalize(nrmMat * inNormal);

    gl_Position = uView.proj * uView.view * posWS;
}
