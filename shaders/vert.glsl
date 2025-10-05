#version 450

// === inputs ===
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inTangent; // xyz tangent, w = bitangent sign (+1/-1)

// === push constants ===
layout(push_constant) uniform PC {
    mat4 model;
} pc;

// === set = 0 : view UBO (VS+FS) ===
layout(set = 0, binding = 0) uniform ViewUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos; // xyz
} viewU;

// === varyings to FS (раскладываем TBN и UV/posW) ===
layout(location = 0) out vec3 vT;
layout(location = 1) out vec3 vB;
layout(location = 2) out vec3 vN;
layout(location = 3) out vec2 vUV;
layout(location = 4) out vec3 vPosW;

void main()
{
    // позиции
    vec4 posW4 = pc.model * vec4(inPos, 1.0);
    vPosW = posW4.xyz;

    // нормали/тангенты в мир. (если будет non-uniform scale — используй нормальную матрицу)
    mat3 M = mat3(pc.model);
    vec3 N = normalize(M * inNormal);
    vec3 T = normalize(M * inTangent.xyz);
    // битангент через знак из inTangent.w
    vec3 B = normalize(cross(N, T) * inTangent.w);

    vN  = N;
    vT  = T;
    vB  = B;
    vUV = inUV;

    gl_Position = viewU.viewProj * posW4;
}
