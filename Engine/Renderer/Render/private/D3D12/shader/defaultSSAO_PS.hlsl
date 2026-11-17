#define REGISTERS_NO_BUILTIN_OBJECT
#include "common/depth.hlsli"

cbuffer SSAOCB : ACTION_DATA_CB
{
    float act_Radius;
    float act_Intensity;
    float act_Bias;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};


static const int   SSAO_SAMPLE_COUNT = 16;
static const float SSAO_RADIUS = 0.5f;
static const float SSAO_BIAS = 0.025f;

static const float3 SSAO_KERNEL[16] =
{
    float3( 0.5381f,  0.1856f, 0.4319f), float3( 0.1379f,  0.2486f, 0.4430f),
    float3( 0.3371f,  0.5679f, 0.0057f), float3(-0.6999f, -0.0451f, 0.0019f),
    float3( 0.0689f, -0.1598f, 0.8547f), float3( 0.0560f,  0.0069f, 0.1843f),
    float3(-0.0146f,  0.1402f, 0.0762f), float3( 0.0100f, -0.1924f, 0.0344f),
    float3(-0.3577f, -0.5301f, 0.4358f), float3(-0.3169f,  0.1063f, 0.0158f),
    float3( 0.0103f, -0.5869f, 0.0046f), float3(-0.0897f, -0.4940f, 0.3287f),
    float3( 0.7119f, -0.0154f, 0.0918f), float3(-0.0533f,  0.0596f, 0.5411f),
    float3( 0.0352f, -0.0631f, 0.5460f), float3(-0.4776f,  0.2847f, 0.0271f)
};


float InterleavedGradientNoise(float2 pixel)
{
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

float4 PSMain(PSInput input) : SV_TARGET4
{
    float2 texSize;
    g_SR8.GetDimensions(texSize.x, texSize.y);

    const float2 uv = input.Pos.xy / texSize;
    const float depth = g_SR8.Load(int3(input.Pos.xy, 0)).r;

    const float3 P = ViewPosition(uv, depth);

    const float3 N = normalize(cross(ddx(P), ddy(P)));

    if (depth >= 1.0f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);

    const float angle = InterleavedGradientNoise(input.Pos.xy) * 6.2831853f;
    const float3 randomDir = float3(cos(angle), sin(angle), 0.0f);

    const float3 T = normalize(randomDir - N * dot(randomDir, N));
    const float3 B = cross(N, T);

    const float radius = SSAO_RADIUS * max(act_Radius, 0.0f);
    const float bias = SSAO_BIAS * max(act_Bias, 0.0f);

    float occlusion = 0.0f;

    [unroll]
    for (int i = 0; i < SSAO_SAMPLE_COUNT; i++)
    {
        const float3 offset = T * SSAO_KERNEL[i].x + B * SSAO_KERNEL[i].y + N * SSAO_KERNEL[i].z;
        const float3 samplePos = P + offset * radius;

        const float4 clip = mul(float4(samplePos, 1.0f), g_P);
        const float2 sampleUV = clip.xy / clip.w * float2(0.5f, -0.5f) + 0.5f;

        const float sceneZ = ViewDepth(g_SR8.SampleLevel(g_SamplerPointClamp, sampleUV, 0).r);

        const float rangeCheck = smoothstep(0.0f, 1.0f, radius / max(abs(P.z - sceneZ), 1e-4f));

        occlusion += (sceneZ <= samplePos.z - bias ? 1.0f : 0.0f) * rangeCheck;
    }

    const float ao = saturate(1.0f - occlusion / SSAO_SAMPLE_COUNT * max(act_Intensity, 0.0f));

    return float4(ao, ao, ao, 1.0f);
}
