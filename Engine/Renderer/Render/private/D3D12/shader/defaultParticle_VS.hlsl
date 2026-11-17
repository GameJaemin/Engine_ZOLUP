#define REGISTERS_NO_BUILTIN_OBJECT
#include "common/registers.hlsli"
#include "common/particle.hlsli"


StructuredBuffer<Particle> g_Particles : DRAW_BUFFER;


struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 Local : TEXCOORD0;
    float3 Color : TEXCOORD1;
};


PSInput VSMain(float4 position : POSITION, uint instanceId : SV_InstanceID)
{
    const Particle p = g_Particles[instanceId];

    float4 posV = mul(float4(p.pos, 1.0f), g_V);
    posV.xy += position.xy * p.size;

    PSInput result;
    result.Pos = mul(posV, g_P);
    result.Local = position.xy;
    result.Color = p.color.rgb;

    return result;
}
