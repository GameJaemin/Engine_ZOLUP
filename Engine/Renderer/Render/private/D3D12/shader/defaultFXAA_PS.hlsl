#define REGISTERS_NO_BUILTIN_OBJECT
#include "common/registers.hlsli"

cbuffer FXAACB : ACTION_DATA_CB
{
    float act_EdgeThreshold;
    float act_SpanMax;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};


static const float FXAA_EDGE_THRESHOLD = 0.125f;
static const float FXAA_EDGE_THRESHOLD_MIN = 0.03125f;
static const float FXAA_SPAN_MAX = 8.0f;
static const float FXAA_REDUCE_MUL = 0.125f;
static const float FXAA_REDUCE_MIN = 0.0078125f;


float Luma(float3 color)
{
    return sqrt(dot(saturate(color), float3(0.299f, 0.587f, 0.114f)));
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float2 texSize;
    g_SR0.GetDimensions(texSize.x, texSize.y);

    const float2 texel = 1.0f / texSize;
    const float2 uv = input.Pos.xy * texel;

    const float4 center = g_SR0.SampleLevel(g_SamplerLinearClamp, uv, 0);

    const float lumaM  = Luma(center.rgb);
    const float lumaNW = Luma(g_SR0.SampleLevel(g_SamplerLinearClamp, uv + float2(-0.5f, -0.5f) * texel, 0).rgb);
    const float lumaNE = Luma(g_SR0.SampleLevel(g_SamplerLinearClamp, uv + float2( 0.5f, -0.5f) * texel, 0).rgb);
    const float lumaSW = Luma(g_SR0.SampleLevel(g_SamplerLinearClamp, uv + float2(-0.5f,  0.5f) * texel, 0).rgb);
    const float lumaSE = Luma(g_SR0.SampleLevel(g_SamplerLinearClamp, uv + float2( 0.5f,  0.5f) * texel, 0).rgb);

    const float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    const float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    const float threshold = max(FXAA_EDGE_THRESHOLD_MIN, lumaMax * FXAA_EDGE_THRESHOLD) * max(act_EdgeThreshold, 0.0f);

    if (lumaMax - lumaMin < threshold)
        return center;

    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    const float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25f * FXAA_REDUCE_MUL, FXAA_REDUCE_MIN);
    const float dirScale = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    const float spanMax = FXAA_SPAN_MAX * max(act_SpanMax, 0.0f);

    dir = clamp(dir * dirScale, -spanMax, spanMax) * texel;

    const float3 colorA = 0.5f * (
        g_SR0.SampleLevel(g_SamplerLinearClamp, uv + dir * (1.0f / 3.0f - 0.5f), 0).rgb +
        g_SR0.SampleLevel(g_SamplerLinearClamp, uv + dir * (2.0f / 3.0f - 0.5f), 0).rgb);

    const float3 colorB = colorA * 0.5f + 0.25f * (
        g_SR0.SampleLevel(g_SamplerLinearClamp, uv + dir * -0.5f, 0).rgb +
        g_SR0.SampleLevel(g_SamplerLinearClamp, uv + dir *  0.5f, 0).rgb);

    const float lumaB = Luma(colorB);

    if (lumaB < lumaMin || lumaB > lumaMax)
        return float4(colorA, center.a);

    return float4(colorB, center.a);
}
