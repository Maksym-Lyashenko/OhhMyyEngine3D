#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D uAlbedo;

void main()
{
    vec3 N = normalize(vNormal);
    float nd = clamp(dot(N, normalize(vec3(0.3, 1.0, 0.2))), 0.0, 1.0);
    vec3 lit = mix(vec3(0.15), vec3(1.0), nd);

    vec3 albedo = texture(uAlbedo, vUV).rgb; // sample
    outColor = vec4(albedo * lit, 1.0);
}
