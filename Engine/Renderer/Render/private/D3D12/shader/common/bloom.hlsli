#ifndef BLOOM_HLSLI
#define BLOOM_HLSLI

#define REGISTERS_NO_BUILTIN_OBJECT
#include "registers.hlsli"

float3 BloomBox4(Texture2D tex, float2 uv, float2 texel)
{
    float3 color = tex.SampleLevel(g_SamplerLinearClamp, uv + float2(-1.0f, -1.0f) * texel, 0).rgb;
    color += tex.SampleLevel(g_SamplerLinearClamp, uv + float2( 1.0f, -1.0f) * texel, 0).rgb;
    color += tex.SampleLevel(g_SamplerLinearClamp, uv + float2(-1.0f,  1.0f) * texel, 0).rgb;
    color += tex.SampleLevel(g_SamplerLinearClamp, uv + float2( 1.0f,  1.0f) * texel, 0).rgb;

    return color * 0.25f;
}

float3 BloomTent9(Texture2D tex, float2 uv, float2 texel)
{
    float3 color = tex.SampleLevel(g_SamplerLinearClamp, uv, 0).rgb * 4.0f;

    color += tex.SampleLevel(g_SamplerLinearClamp, uv + float2(-1.0f,  0.0f) * texel, 0).rgb * 2.0f;
    color += tex.SampleLevel(g_SamplerLinearClamp, uv + float2( 1.0f,  0.0f) * texel, 0).rgb * 2.0f;
    color += tex.SampleLevel(g_SamplerLinearClamp, uv + float2( 0.0f, -1.0f) * texel, 0).rgb * 2.0f;
    color += tex.SampleLevel(g_SamplerLinearClamp, uv + float2( 0.0f,  1.0f) * texel, 0).rgb * 2.0f;

    color += tex.SampleLevel(g_SamplerLinearClamp, uv + float2(-1.0f, -1.0f) * texel, 0).rgb;
    color += tex.SampleLevel(g_SamplerLinearClamp, uv + float2( 1.0f, -1.0f) * texel, 0).rgb;
    color += tex.SampleLevel(g_SamplerLinearClamp, uv + float2(-1.0f,  1.0f) * texel, 0).rgb;
    color += tex.SampleLevel(g_SamplerLinearClamp, uv + float2( 1.0f,  1.0f) * texel, 0).rgb;

    return color / 16.0f;
}

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};

#endif
