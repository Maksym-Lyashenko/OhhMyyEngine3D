#version 450

layout(location=0) in vec3 inPos;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUV;
layout(location=3) in vec4 inTangent; // .w = bitangent sign

layout(std140, set=0, binding=0) uniform ViewUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos;
} uView;

layout(push_constant) uniform PushConsts { mat4 model; } pc;

layout(location=0) out vec2  vUV;
layout(location=1) out vec3  vPosWS;
layout(location=2) out vec3  vNormalWS;
layout(location=3) out vec3  vTangentWS;
layout(location=4) out float vBtSign;

void main() {
    mat3 nrmMat = transpose(inverse(mat3(pc.model)));
    vec4 posWS  = pc.model * vec4(inPos, 1.0);

    vUV        = inUV;
    vPosWS     = posWS.xyz;
    vNormalWS  = normalize(nrmMat * inNormal);
    vTangentWS = normalize(nrmMat * inTangent.xyz);
    vBtSign    = inTangent.w;

    gl_Position = uView.viewProj * posWS;
}
