#version 450

// — стабильные значения —
const float NORMAL_MIP_BIAS = 1.25;   // было так, когда «дрожь» ушла
const float NORMAL_SCALE    = 0.50;   // приглушаем нормаль
const float ALBEDO_MIP_BIAS = 0.25;   // мягкая фильтрация альбедо

layout(location=0) in vec2  vUV;
layout(location=1) in vec3  vPosWS;
layout(location=2) in vec3  vNormalWS;
layout(location=3) in vec3  vTangentWS;
layout(location=4) in float vBtSign;

layout(location=0) out vec4 outColor;

// set=0
layout(std140, set=0, binding=0) uniform ViewUBO {
    mat4 view; mat4 proj; mat4 viewProj; vec4 cameraPos;
} uView;

// set=1
layout(set=1, binding=0) uniform sampler2D uBaseColor; // sRGB
layout(set=1, binding=1) uniform sampler2D uNormal;    // UNORM
layout(set=1, binding=2) uniform sampler2D uMR;        // UNORM (MR или ARM)
layout(set=1, binding=3) uniform sampler2D uAO;        // UNORM (если не ARM)
layout(set=1, binding=4) uniform sampler2D uEmissive;  // sRGB
layout(std140, set=1, binding=5) uniform MaterialUBO {
    vec4  baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float emissiveStrength;
    float _pad0;
    vec2  uvTiling;
    vec2  uvOffset;
    uint  flags; // bit0=albedo, bit1=normal, bit2=hasMR, bit3=hasAO, bit4=hasEmissive, bit5=isARM
    uint _p1; uint _p2; uint _p3;
} uMat;

// set=2
struct DirectionalLightGPU { vec4 direction_ws; vec4 radiance; };
struct PointLightGPU       { vec4 position_ws;  vec4 color_range; }; // rgb + range
layout(std140, set=2, binding=0) uniform LightingCountsUBO {
    uvec4 counts_flags; // x=dir, y=point, z=spot, w=flags
    vec4  ambientRGBA;  // rgb
} uLight;
layout(std430, set=2, binding=1) readonly buffer DirBuf   { DirectionalLightGPU dir[];   };
layout(std430, set=2, binding=2) readonly buffer PointBuf { PointLightGPU       point[]; };

// ---------- helpers ----------
vec3 normalFromMap(vec2 uv, vec3 Nws, vec3 Tws, float btSign){
    vec3 N = normalize(Nws);
    vec3 T = normalize(Tws);
    vec3 B = normalize(btSign * cross(N, T));

    vec3 n = texture(uNormal, uv).xyz * 2.0 - 1.0;
    n.xy *= NORMAL_SCALE;
    n.z   = sqrt(max(0.0, 1.0 - dot(n.xy, n.xy)));

    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * n);
}

void sampleMR_AO(vec2 uv, out float metallic, out float roughness, out float ao){
    metallic  = uMat.metallicFactor;
    roughness = uMat.roughnessFactor;
    ao        = 1.0;
    bool hasMR = (uMat.flags & 4u) != 0u;
    bool hasAO = (uMat.flags & 8u) != 0u;
    bool isARM = (uMat.flags & 32u) != 0u;
    if (hasMR){
        vec3 mrg = texture(uMR, uv).rgb;
        if (isARM){ ao = mrg.r; roughness = mrg.g; metallic = mrg.b; }
        else { roughness = mrg.g; metallic = mrg.b; if (hasAO) ao = texture(uAO, uv).r; }
    } else if (hasAO){
        ao = texture(uAO, uv).r;
    }
}

// GGX / Smith / Schlick
float D_GGX(float NdotH, float a){
    float a2 = a*a;
    float d  = (NdotH*NdotH)*(a2-1.0)+1.0;
    return a2 / max(3.14159265 * d * d, 1e-7);
}
float G_Smith(float NdotV, float NdotL, float a){
    float k = (a+1.0); k = (k*k) / 8.0;
    float gv = NdotV / (NdotV*(1.0-k)+k);
    float gl = NdotL / (NdotL*(1.0-k)+k);
    return gv * gl;
}
vec3  F_Schlick(float cosTheta, vec3 F0){
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main(){
    vec2 uv = vUV * uMat.uvTiling + uMat.uvOffset;

    // Альбедо — без адаптивных «наворотов»
    vec3 albedo = texture(uBaseColor, uv).rgb * uMat.baseColorFactor.rgb;

    // MR/AO
    float metallic, roughness, ao;
    sampleMR_AO(uv, metallic, roughness, ao);
    roughness = clamp(roughness, 0.04, 1.0);
    float a = roughness * roughness;

    // Нормаль
    vec3 N = ((uMat.flags & 2u) != 0u)
           ? normalFromMap(uv, vNormalWS, vTangentWS, vBtSign)
           : normalize(vNormalWS);
    vec3 V = normalize(uView.cameraPos.xyz - vPosWS);
    float NdotV = max(dot(N, V), 0.0);

    // Specular AA (только Toksvig по мипу нормали)
    float lodN   = NORMAL_MIP_BIAS;
    vec3  nAvg   = textureLod(uNormal, uv, lodN).xyz * 2.0 - 1.0;
    float lenAvg = clamp(length(nAvg), 1e-3, 1.0);
    float sigma2 = max((1.0 / lenAvg) - 1.0, 0.0);
    a = sqrt(a*a + sigma2);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    // Directional
    uint nd = uLight.counts_flags.x;
    for (uint i=0u; i<nd; ++i) {
        vec3 L = normalize(-dir[i].direction_ws.xyz);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            float NdotH = max(dot(N, H), 0.0);
            float VdotH = max(dot(V, H), 0.0);
            float  D = D_GGX(NdotH, a);
            float  G = G_Smith(NdotV, NdotL, a);
            vec3   F = F_Schlick(VdotH, F0);
            vec3  spec = (D * G) * F / max(4.0 * NdotV * NdotL, 1e-6);
            vec3  kd   = (1.0 - F) * (1.0 - metallic);
            vec3  diff = kd * albedo / 3.14159265;
            Lo += (diff + spec) * dir[i].radiance.rgb * NdotL;
        }
    }

    // Point
    uint np = uLight.counts_flags.y;
    for (uint i=0u; i<np; ++i) {
        vec3 toL = point[i].position_ws.xyz - vPosWS;
        float d  = length(toL);
        vec3  L  = (d > 1e-5) ? toL / d : vec3(0,1,0);
        float r  = max(point[i].color_range.a, 1e-3);
        float att = 1.0 / (1.0 + (d*d)/(r*r));

        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            float NdotH = max(dot(N, H), 0.0);
            float VdotH = max(dot(V, H), 0.0);
            float  D = D_GGX(NdotH, a);
            float  G = G_Smith(NdotV, NdotL, a);
            vec3   F = F_Schlick(VdotH, F0);
            vec3  spec = (D * G) * F / max(4.0 * NdotV * NdotL, 1e-6);
            vec3  kd   = (1.0 - F) * (1.0 - metallic);
            vec3  diff = kd * albedo / 3.14159265;
            Lo += (diff + spec) * point[i].color_range.rgb * NdotL * att;
        }
    }

    vec3 ambient = uLight.ambientRGBA.rgb * albedo * (1.0 - metallic);
    ambient *= 1.2; 
    vec3 color = (ambient * ao) + Lo;

    if ((uMat.flags & 16u) != 0u)
        color += texture(uEmissive, uv).rgb * uMat.emissiveStrength;

    outColor = vec4(color, uMat.baseColorFactor.a);
}
