#define REGISTERS_NO_BUILTIN_OBJECT
#include "common/depth.hlsli"

cbuffer AOBlurCB : ACTION_DATA_CB
{
    float act_DepthThreshold;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};


static const int   AOBLUR_HALF = 2;
static const float AOBLUR_DEPTH_THRESHOLD = 0.25f;


float4 PSMain(PSInput input) : SV_TARGET4
{
    float2 aoSize;
    g_SR4.GetDimensions(aoSize.x, aoSize.y);

    const float2 texel = 1.0f / aoSize;
    const float2 uv = input.Pos.xy * texel;

    const float centerZ = ViewDepth(g_SR8.Load(int3(input.Pos.xy, 0)).r);
    const float threshold = AOBLUR_DEPTH_THRESHOLD * max(act_DepthThreshold, 0.0f);

    float sum = 0.0f;
    float weightSum = 0.0f;

    [unroll]
    for (int y = -AOBLUR_HALF; y < AOBLUR_HALF; y++)
    {
        [unroll]
        for (int x = -AOBLUR_HALF; x < AOBLUR_HALF; x++)
        {
            const float2 tapUV = uv + float2(x, y) * texel;

            const float tapAO = g_SR4.SampleLevel(g_SamplerPointClamp, tapUV, 0).r;
            const float tapZ = ViewDepth(g_SR8.SampleLevel(g_SamplerPointClamp, tapUV, 0).r);

            const float weight = abs(tapZ - centerZ) < threshold ? 1.0f : 0.0f;

            sum += tapAO * weight;
            weightSum += weight;
        }
    }

    const float ao = sum / max(weightSum, 1.0f);

    return float4(ao, ao, ao, 1.0f);
}
