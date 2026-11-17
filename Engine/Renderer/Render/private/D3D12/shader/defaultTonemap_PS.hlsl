#define REGISTERS_NO_BUILTIN_OBJECT
#include "common/registers.hlsli"

cbuffer TonemapCB : ACTION_DATA_CB
{
    float act_ExposureEV;
    float act_Mode;
    float act_GainR;
    float act_GainG;
    float act_GainB;
    float act_Contrast;
    float act_Saturation;
    float act_Gamma;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};


float3 ApplyExposure(float3 color)
{
    return color * exp2(act_ExposureEV);
}

float3 ApplyWhiteBalance(float3 color)
{
    return color * max(float3(act_GainR, act_GainG, act_GainB), 0.0f);
}

float3 TonemapACES(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;

    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 TonemapReinhard(float3 x)
{
    return x / (1.0f + x);
}

float3 ApplyCurve(float3 color)
{
    if (act_Mode < 0.5f)
        return TonemapACES(color);
    if (act_Mode < 1.5f)
        return TonemapReinhard(color);

    return saturate(color);
}

float3 ApplyGrade(float3 color)
{
    color = saturate((color - 0.5f) * max(act_Contrast, 0.0f) + 0.5f);

    const float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    color = saturate(lerp(luma.xxx, color, max(act_Saturation, 0.0f)));

    return pow(color, 1.0f / max(act_Gamma, 0.01f));
}

float4 PSMain(PSInput input) : SV_TARGET
{
    const float4 scene = g_SR0.Load(int3(input.Pos.xy, 0));

    float3 color = scene.rgb;

    color = ApplyExposure(color);
    color = ApplyWhiteBalance(color);
    color = ApplyCurve(color);
    color = ApplyGrade(color);

    return float4(color, scene.a);
}
