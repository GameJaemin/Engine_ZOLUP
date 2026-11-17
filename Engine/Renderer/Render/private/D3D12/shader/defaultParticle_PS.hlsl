#if defined(OUTPUT_GBUFFER) || defined(OUTPUT_COLORID) || defined(OUTPUT_DEPTHONLY)
#error "defaultParticle_PS: Color 레이아웃만 지원"
#endif

#define REGISTERS_NO_BUILTIN_OBJECT
#include "common/registers.hlsli"


struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 Local : TEXCOORD0;
    float3 Color : TEXCOORD1;
};


float4 PSMain(PSInput input) : SV_TARGET
{
    const float2 uv = float2(input.Local.x * 0.5f + 0.5f, 0.5f - input.Local.y * 0.5f);
    const float4 tex = g_Tex[0].Sample(g_SamplerLinearClamp, uv);

    float2 texSize;
    g_Tex[0].GetDimensions(texSize.x, texSize.y);

    if (texSize.x > 1.0f || texSize.y > 1.0f)
    {
        return float4(input.Color * tex.rgb * tex.a, 1.0f);
    }

    const float dist = length(input.Local);

    float glow = saturate(1.0f - dist);
    glow *= glow;

    return float4(input.Color * glow, 1.0f);
}
