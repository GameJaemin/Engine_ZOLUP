Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

cbuffer ImGuiDrawOption : register(b1)
{
    uint g_DrawOption;
};

#define DRAW_OPTION_LINEAR_TO_GAMMA 0x1

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};


float4 PSMain(PSInput input) : SV_TARGET
{
    float4 texColor = g_Texture.Sample(g_Sampler, input.UV);

    if ((g_DrawOption & DRAW_OPTION_LINEAR_TO_GAMMA) != 0)
    {
        const float3 c = saturate(texColor.rgb);
        texColor.rgb = lerp(1.055f * pow(c, 1.0f / 2.4f) - 0.055f, c * 12.92f, step(c, 0.0031308f));
    }

    return input.Color * texColor;
}
