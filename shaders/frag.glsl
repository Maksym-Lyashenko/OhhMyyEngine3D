#version 450

// --- поставь 1, если swapchain в sRGB (например VK_FORMAT_B8G8R8A8_SRGB)
#define SWAPCHAIN_SRGB 1

// === varyings === (должны 1-в-1 совпадать с VS)
layout(location = 0) in vec3 vT;
layout(location = 1) in vec3 vB;
layout(location = 2) in vec3 vN;
layout(location = 3) in vec2 vUV;
layout(location = 4) in vec3 vPosW;

// === выход ===
layout(location = 0) out vec4 outColor;

// === set = 0 : view UBO (VS+FS) ===
layout(set = 0, binding = 0) uniform ViewUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos; // xyz
} viewU;

// === set = 1 : материалы ===
layout(set = 1, binding = 0) uniform sampler2D uBaseColor;      // sRGB-текстура
layout(set = 1, binding = 1) uniform sampler2D uNormalMap;       // UNORM
layout(set = 1, binding = 2) uniform sampler2D uMetallicRoughness; // UNORM (B=metallic, G=roughness)

// binding 3/4 (AO/Emissive) в лэйауте есть, но тут не используем — это ок.

// UBO материалов (binding = 5)
layout(set = 1, binding = 5) uniform MaterialUBO {
    vec4  baseColorFactor; // множитель к baseColor
    float metalFactor;     // глобальный множитель к metallic (обычно 1.0)
    float roughFactor;     // глобальный множитель к roughness (обычно 1.0)
    float normalScale;     // 1.0 (можно отрицать для flip G)
    float uExposure;       // 1.0 .. 1.4
} matU;

// === вспомогательные ===
const float PI = 3.14159265359;

// без лишних зависимостей
vec3 srgb_to_linear(vec3 c) { return pow(c, vec3(2.2)); }
vec3 linear_to_srgb(vec3 c) { return pow(max(c, vec3(0.0)), vec3(1.0/2.2)); }

// нормаль из нормал-карты в TBN
vec3 normalFromMap(vec3 T, vec3 B, vec3 N, vec2 uv)
{
    vec3 tnorm = texture(uNormalMap, uv).xyz * 2.0 - 1.0;
    // часто нужно флипнуть G. Используем знак normalScale.
    tnorm.y *= sign(matU.normalScale == 0.0 ? 1.0 : matU.normalScale);

    mat3 TBN = mat3(normalize(T), normalize(B), normalize(N));
    return normalize(TBN * tnorm);
}

// GGX
float D_GGX(float NdotH, float a)
{
    float a2 = a*a;
    float d = (NdotH*NdotH)*(a2-1.0)+1.0;
    return a2 / (PI * d*d + 1e-7);
}
float V_SmithGGXCorrelated(float NdotV, float NdotL, float a)
{
    float a2 = a*a;
    float gv = NdotL * sqrt(max(0.0, (NdotV*(1.0-a2) + a2)));
    float gl = NdotV * sqrt(max(0.0, (NdotL*(1.0-a2) + a2)));
    return 0.5 / max(gv+gl, 1e-7);
}
vec3  F_Schlick(vec3 F0, float VdotH)
{
    float f = pow(1.0 - VdotH, 5.0);
    return F0 + (1.0 - F0) * f;
}

// простая «полугемисфера» + слабый spec IBL
vec3 hemiDiffuse(vec3 N, vec3 base)
{
    vec3 skyCol    = vec3(0.10, 0.11, 0.13);
    vec3 groundCol = vec3(0.03, 0.028, 0.025);
    float h = 0.5 * N.y + 0.5;
    return mix(groundCol, skyCol, h) * base;
}

// ACES (Narkowicz)
vec3 ACES(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main()
{
    // 1) сэмплы
    // baseColor: текстура в SRGB-формате -> sampler вернет *линейный* цвет, так что без конверта.
    vec4 baseTex = texture(uBaseColor, vUV);
    vec3 base = baseTex.rgb * matU.baseColorFactor.rgb;

    vec3 mr  = texture(uMetallicRoughness, vUV).rgb;
    float metallic  = clamp(mr.b * matU.metalFactor, 0.0, 1.0);
    float roughness = clamp(mr.g * matU.roughFactor, 0.04, 1.0);

    // 2) геометрия
    vec3 N = normalFromMap(vT, vB, vN, vUV);
    vec3 V = normalize(viewU.cameraPos.xyz - vPosW);
    vec3 L = normalize(vec3(0.3, 0.8, 0.4)); // key light направление
    vec3 H = normalize(L + V);

    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    // 3) PBR энергии
    vec3  F0dielectric = vec3(0.04);
    vec3  F0 = mix(F0dielectric, base, metallic);
    float a  = roughness*roughness;

    float  D = D_GGX(NdotH, a);
    float  Vg = V_SmithGGXCorrelated(NdotV, NdotL, a);
    vec3   F = F_Schlick(F0, VdotH);

    vec3 specBRDF = (D * Vg) * F;      // * 1/PI уже «внутри» D/V аппроксимаций
    vec3 kd = (1.0 - F) * (1.0 - metallic);
    vec3 diffBRDF = kd * base / PI;

    vec3 direct = (diffBRDF + specBRDF) * NdotL * vec3(3.0); // интенсивность лайтa

    // 4) простая IBL-заполнялка
    vec3 diffIBL = hemiDiffuse(N, base) * kd * 0.6;
    // чуть-чуть «холодного» отражения
    vec3 F_ibl = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);
    vec3 specIBL = F_ibl * vec3(0.10, 0.11, 0.13) * (1.0 - roughness) * 0.7;

    vec3 colorLin = direct + diffIBL + specIBL;

    // 5) экспозиция + тонмап
    float exposure = (matU.uExposure == 0.0 ? 1.1 : matU.uExposure);
    colorLin = vec3(1.0) - exp(-colorLin * exposure);
    vec3 mapped = ACES(colorLin);

#if SWAPCHAIN_SRGB
    outColor = vec4(clamp(mapped, 0.0, 1.0), baseTex.a * matU.baseColorFactor.a);
#else
    outColor = vec4(linear_to_srgb(mapped), baseTex.a * matU.baseColorFactor.a);
#endif
}
