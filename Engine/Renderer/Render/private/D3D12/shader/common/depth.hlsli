#ifndef DEPTH_HLSLI
#define DEPTH_HLSLI

#include "registers.hlsli"

float ViewDepth(float depth)
{
    return g_P[3][2] / (depth - g_P[2][2]);
}

float3 ViewPosition(float2 uv, float depth)
{
    const float z = ViewDepth(depth);
    const float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);

    return float3(ndc.x * z / g_P[0][0], ndc.y * z / g_P[1][1], z);
}

float3 WorldPosition(float2 uv, float depth)
{
    return mul((float3x3)g_V, ViewPosition(uv, depth) - g_V[3].xyz);
}

#endif
