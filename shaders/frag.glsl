#version 450

layout(location = 0) in vec3 vNormal;    // world-space
layout(location = 1) in vec3 vWorldPos;  // world-space
layout(location = 0) out vec4 outColor;

// Можно кодировать параметры света константами, чтобы не плодить дескрипторы.
// Если хочешь — потом вынесем в UBO.
const vec3  baseColor   = vec3(0.90, 0.06, 0.06); // "красная краска"
const vec3  lightDirWS  = normalize(vec3(-0.4, -1.0, -0.5)); // сверху/справа
const vec3  lightColor  = vec3(1.0);
const float ambientK    = 0.22;  // ambient
const float diffuseK    = 0.88;  // diffuse вес
const float gamma       = 2.2;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-lightDirWS); // к свету

    float NoL = max(dot(N, L), 0.0);

    vec3 colorLinear = baseColor * (ambientK + diffuseK * NoL) * lightColor;

    // простая гамма-коррекция в sRGB
    vec3 colorSRGB = pow(colorLinear, vec3(1.0 / gamma));

    outColor = vec4(colorSRGB, 1.0);
}
