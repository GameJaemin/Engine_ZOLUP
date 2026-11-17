#include "common/bloom.hlsli"

cbuffer BloomUpCB : ACTION_DATA_CB
{
    float act_Radius;
};


float4 PSMain(PSInput input) : SV_TARGET
{
    float2 srcSize;
    g_Tex[0].GetDimensions(srcSize.x, srcSize.y);

    const float2 texel = max(act_Radius, 0.0f) / srcSize;

    return float4(BloomTent9(g_Tex[0], input.UV, texel), 1.0f);
}
