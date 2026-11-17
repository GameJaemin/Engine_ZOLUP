#include "common/bloom.hlsli"


float4 PSMain(PSInput input) : SV_TARGET
{
    float2 srcSize;
    g_Tex[0].GetDimensions(srcSize.x, srcSize.y);

    return float4(BloomBox4(g_Tex[0], input.UV, 1.0f / srcSize), 1.0f);
}
