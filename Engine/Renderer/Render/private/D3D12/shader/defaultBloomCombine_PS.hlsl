#include "common/bloom.hlsli"

cbuffer BloomCombineCB : ACTION_DATA_CB
{
    float act_Intensity;
    float act_Radius;
};


static const float BLOOM_INTENSITY = 0.25f;


float4 PSMain(PSInput input) : SV_TARGET
{
    const float4 scene = g_SR0.Load(int3(input.Pos.xy, 0));

    float2 srcSize;
    g_Tex[0].GetDimensions(srcSize.x, srcSize.y);

    const float2 texel = max(act_Radius, 0.0f) / srcSize;
    const float3 bloom = BloomTent9(g_Tex[0], input.UV, texel);

    return float4(scene.rgb + bloom * BLOOM_INTENSITY * max(act_Intensity, 0.0f), scene.a);
}
