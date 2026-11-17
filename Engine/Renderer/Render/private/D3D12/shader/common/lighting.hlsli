#ifndef LIGHTING_HLSLI
#define LIGHTING_HLSLI

#include "registers.hlsli"

static const float PI = 3.14159265f;

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

float DistributionGGX(float NdotH, float alpha)
{
    const float a2 = alpha * alpha;
    const float d = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-6f);
}

float GeometrySmith(float NdotV, float NdotL, float rough)
{
    const float k = (rough + 1.0f) * (rough + 1.0f) / 8.0f;
    const float gv = NdotV / (NdotV * (1.0f - k) + k);
    const float gl = NdotL / (NdotL * (1.0f - k) + k);
    return gv * gl;
}

float3 BRDF(float3 N, float3 V, float3 L, float3 albedo, float rough, float metal)
{
    const float3 H = normalize(V + L);
    const float NdotL = saturate(dot(N, L));
    const float NdotV = max(dot(N, V), 1e-4f);
    const float NdotH = saturate(dot(N, H));
    const float VdotH = saturate(dot(V, H));

    const float3 F0 = lerp(0.04f.xxx, albedo, metal);
    const float3 F = FresnelSchlick(VdotH, F0);
    const float D = DistributionGGX(NdotH, rough * rough);
    const float G = GeometrySmith(NdotV, NdotL, rough);

    const float3 spec = (D * G * F) / max(4.0f * NdotV * NdotL, 1e-4f);
    const float3 kd = (1.0f - F) * (1.0f - metal);

    return (kd * albedo / PI + spec) * NdotL;
}

float SampleShadowTile(Texture2D atlas, float3 P, matrix lightVP, float4 tile)
{
    const float4 posL = mul(float4(P, 1.0f), lightVP);

    if (posL.w <= 0.0f)
        return 1.0f;

    const float3 ndc = posL.xyz / posL.w;

    float2 uv = ndc.xy * float2(0.5f, -0.5f) + 0.5f;

    if (any(uv < 0.0f) || any(uv > 1.0f) || ndc.z > 1.0f)
        return 1.0f;

    float2 atlasSize;
    atlas.GetDimensions(atlasSize.x, atlasSize.y);

    const float2 halfTexel = 0.5f / atlasSize;

    uv = clamp(uv * tile.xy + tile.zw, tile.zw + halfTexel, tile.zw + tile.xy - halfTexel);

    return atlas.SampleCmpLevelZero(g_SamplerShadow, uv, ndc.z);
}

float GlobalShadow(float3 P)
{
    const float viewDepth = mul(float4(P, 1.0f), g_V).z;

    int cascade = -1;

    [unroll]
    for (int i = 3; i >= 0; i--)
    {
        if (g_LSplit[i] > 0.0f && viewDepth <= g_LSplit[i])
            cascade = i;
    }

    if (cascade < 0)
        return 1.0f;

    return SampleShadowTile(g_ShadowGlobal, P, g_LVP[cascade], g_LTile[cascade]);
}

float PresetShadow(uint index, float3 P)
{
    const float4 tile = g_ShadowPresets[index].tile;

    if (tile.x <= 0.0f)
        return 1.0f;

    return SampleShadowTile(g_ShadowLocal, P, g_ShadowPresets[index].vp, tile);
}

float GlobalShadowPreset(float3 P, uint preset)
{
    const uint index = preset * SHADOW_PRESET_ENTRY_MAX + SHADOW_PRESET_GLOBAL;

    if (g_ShadowPresets[index].lightSlot == 0)
        return GlobalShadow(P);

    return PresetShadow(index, P);
}

uint2 PresetLightMask(uint preset)
{
    uint2 mask = uint2(0, 0);

    [unroll]
    for (uint k = 0; k < SHADOW_PRESET_LOCAL_MAX; k++)
    {
        const uint lightId = g_ShadowPresets[preset * SHADOW_PRESET_ENTRY_MAX + k].lightSlot;

        if (lightId != 0 && lightId < 32)
            mask.x |= 1u << lightId;
        else if (lightId >= 32 && lightId < 64)
            mask.y |= 1u << (lightId - 32);
    }

    return mask;
}

float LocalShadow(uint lightId, float3 P, uint preset, uint2 presetMask)
{
    const uint bits = lightId < 32 ? presetMask.x : presetMask.y;

    if (lightId >= 64 || ((bits >> (lightId & 31)) & 1) == 0)
        return 1.0f;

    uint index = preset * SHADOW_PRESET_ENTRY_MAX;

    [loop]
    for (uint k = 0; k < SHADOW_PRESET_LOCAL_MAX; k++)
    {
        if (g_ShadowPresets[preset * SHADOW_PRESET_ENTRY_MAX + k].lightSlot == lightId)
        {
            index = preset * SHADOW_PRESET_ENTRY_MAX + k;
            break;
        }
    }

    return PresetShadow(index, P);
}


float3 NormalMapToWorld(float3 normalVal, float3 vertexNormal, float3 vertexTangent)
{
    const float3 N = normalize(vertexNormal);

    const float3 tangent = vertexTangent - N * dot(vertexTangent, N);
    const float lenSq = dot(tangent, tangent);

    if (lenSq <= 1e-6f * dot(vertexTangent, vertexTangent))
        return N;

    const float3 T = tangent / sqrt(lenSq);
    const float3 B = cross(N, T);
    const float3 tangentNormal = normalVal * 2.0f - 1.0f;

    return normalize(T * tangentNormal.x + B * tangentNormal.y + N * tangentNormal.z);
}


float ScreenAO(float2 pixel)
{
    float2 texSize;
    g_SR4.GetDimensions(texSize.x, texSize.y);

    const float3 value = g_SR4.SampleLevel(g_SamplerPointClamp, pixel / texSize, 0).rgb;

    if (all(abs(value - float3(1.0f, 0.0f, 1.0f)) < 0.01f))
        return 1.0f;

    return value.r;
}

float3 LocalLightColor(uint lightId, float3 P, float3 N, float3 V, float3 albedo, float rough, float metal)
{
    const Light light = g_Lights[lightId];
    if (light.l_Type == 0)
        return 0.0f.xxx;

    float3 L;
    float atten = 1.0f;

    if (light.l_Type == 1)
    {
        L = normalize(-light.l_Dir);
    }
    else
    {
        const float3 d = light.l_Pos - P;
        const float dist = length(d);
        L = d / max(dist, 1e-4f);

        const float r = max(light.l_Range, 1e-4f);
        const float x = saturate(1.0f - (dist * dist) / (r * r));
        atten = x * x;

        if (light.l_Type == 3)
        {
            const float cosCone = cos(light.l_Angle * 0.5f);
            const float cd = dot(-L, normalize(light.l_Dir));
            atten *= smoothstep(cosCone, cosCone + 0.05f, cd);
        }

        if (atten <= 0.0f)
            return 0.0f.xxx;
    }

    return BRDF(N, V, L, albedo, rough, metal) * light.l_Col * atten * PI;
}

float3 ShadeLocalLight(uint lightId, float3 P, float3 N, float3 V, float3 albedo, float rough, float metal, uint preset, uint2 presetMask)
{
    const float3 lightColor = LocalLightColor(lightId, P, N, V, albedo, rough, metal);

    if (!any(lightColor))
        return lightColor;

    return lightColor * LocalShadow(lightId, P, preset, presetMask);
}

#ifndef REGISTERS_NO_BUILTIN_OBJECT
float3 ShadeSurface(float3 P, float3 N, float3 V, float3 albedo, float rough, float metal, float ao)
{
    rough = clamp(rough, 0.04f, 1.0f);
    metal = saturate(metal);

    const uint preset = min(obj_ShadowPreset, SHADOW_PRESET_MAX - 1);

    float3 color = BRDF(N, V, normalize(-g_LDir), albedo, rough, metal) * g_LCol.rgb * PI * GlobalShadowPreset(P, preset);

    const uint2 presetMask = PresetLightMask(preset);

    [unroll]
    for (uint i = 0; i < 8; i++)
        color += ShadeLocalLight(ObjLightId(i), P, N, V, albedo, rough, metal, preset, presetMask);

    color += g_LAmb.rgb * albedo * ao;

    return color;
}
#endif

float3 ShadeSurfaceAllLights(float3 P, float3 N, float3 V, float3 albedo, float rough, float metal, float ao, uint shadowPreset)
{
    rough = clamp(rough, 0.04f, 1.0f);
    metal = saturate(metal);

    const uint preset = min(shadowPreset, SHADOW_PRESET_MAX - 1);

    float3 color = BRDF(N, V, normalize(-g_LDir), albedo, rough, metal) * g_LCol.rgb * PI * GlobalShadowPreset(P, preset);

    const uint2 presetMask = PresetLightMask(preset);

    [loop]
    for (uint i = 0; i < 64; i++)
        color += ShadeLocalLight(i, P, N, V, albedo, rough, metal, preset, presetMask);

    color += g_LAmb.rgb * albedo * ao;

    return color;
}

#endif
