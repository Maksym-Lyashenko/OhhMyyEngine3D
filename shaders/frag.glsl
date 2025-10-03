#version 450

layout(location = 0) in vec3 vNormalWS;
layout(location = 1) in vec3 vPosWS;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 N = normalize(vNormalWS);
    vec3 L = normalize(vec3(0.4, 0.7, 0.2)); // чисто для теста
    float ndotl = max(dot(N, L), 0.0);
    vec3 base = vec3(0.75, 0.8, 0.9);
    outColor = vec4(base * (0.2 + 0.8 * ndotl), 1.0);
}
