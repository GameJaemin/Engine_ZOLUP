cbuffer ImGuiProjection : register(b0)
{
    float2 g_Scale;
    float2 g_Translate;
};

struct VSInput
{
    float2 Pos : POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};


PSInput VSMain(VSInput input)
{
    PSInput output;
    output.Pos = float4(input.Pos * g_Scale + g_Translate, 0.0f, 1.0f);
    output.UV = input.UV;
    output.Color = input.Color;
    return output;
}
