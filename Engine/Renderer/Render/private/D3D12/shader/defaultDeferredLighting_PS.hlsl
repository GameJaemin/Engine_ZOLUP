#define REGISTERS_NO_BUILTIN_OBJECT
#include "common/depth.hlsli"
#include "common/lighting.hlsli"

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};


float4 PSMain(PSInput input) : SV_TARGET
{
    const int3 pixel = int3(input.Pos.xy, 0);

    const float depth = g_SR8.Load(pixel).r;

    if (depth >= 1.0f)
        discard;

    const float3 albedo = g_SR0.Load(pixel).rgb;
    const float3 N = normalize(g_SR1.Load(pixel).xyz * 2.0f - 1.0f);
    const float4 ormPreset = g_SR2.Load(pixel);
    const float3 orm = ormPreset.rgb;
    const uint shadowPreset = (uint)(ormPreset.a + 0.5f);
    const float3 emission = g_SR3.Load(pixel).rgb;

    float2 texSize;
    g_SR8.GetDimensions(texSize.x, texSize.y);

    const float3 P = WorldPosition(input.Pos.xy / texSize, depth);
    const float3 V = normalize(g_EyePos - P);

    const float ao = orm.r * ScreenAO(input.Pos.xy);
    const float3 color = ShadeSurfaceAllLights(P, N, V, albedo, orm.g, orm.b, ao, shadowPreset) + emission;

    return float4(color, 1.0f);
}
