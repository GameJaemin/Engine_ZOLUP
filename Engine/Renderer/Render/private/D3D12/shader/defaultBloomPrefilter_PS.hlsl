#include "common/bloom.hlsli"

cbuffer BloomPrefilterCB : ACTION_DATA_CB
{
    float act_Threshold;
    float act_Knee;
};


static const float BLOOM_THRESHOLD = 1.0f;
static const float BLOOM_KNEE = 0.5f;


float4 PSMain(PSInput input) : SV_TARGET
{
    float2 srcSize;
    g_SR0.GetDimensions(srcSize.x, srcSize.y);

    const float3 color = BloomBox4(g_SR0, input.UV, 1.0f / srcSize);

    const float threshold = BLOOM_THRESHOLD * max(act_Threshold, 0.0f);
    const float knee = threshold * BLOOM_KNEE * max(act_Knee, 0.0f);

    const float brightness = max(color.r, max(color.g, color.b));

    float soft = clamp(brightness - threshold + knee, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee + 1e-4f);

    const float keep = max(soft, brightness - threshold) / max(brightness, 1e-4f);

    return float4(color * keep, 1.0f);
}
