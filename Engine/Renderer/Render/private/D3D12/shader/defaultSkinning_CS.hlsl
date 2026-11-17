#include "common/registers_compute.hlsli"

#define SKIN_BONE_MAX 512

cbuffer BoneCB : ACTION_DATA_CB
{
    matrix act_BoneTr[SKIN_BONE_MAX];
};

struct SkinnedVertex
{
    float3 pos;
    float2 uv;
    float3 normal;
    float3 tangent;
    float4 color;
    uint   boneIdx01;
    uint   boneIdx23;
    float4 boneWeight;
};

struct StaticVertex
{
    float3 pos;
    float2 uv;
    float3 normal;
    float3 tangent;
    float4 color;
};

StructuredBuffer<SkinnedVertex>  g_Src : CS_READ_BUFFER(0);
RWStructuredBuffer<StaticVertex> g_Dst : CS_WRITE_BUFFER(0);


[numthreads(64, 1, 1)]
void CSMain(uint3 threadId : SV_DispatchThreadID)
{
    const SkinnedVertex src = g_Src[threadId.x];

    StaticVertex dst;
    dst.pos = src.pos;
    dst.uv = src.uv;
    dst.normal = src.normal;
    dst.tangent = src.tangent;
    dst.color = src.color;

    const float4 w = src.boneWeight;

    if (w.x + w.y + w.z + w.w > 0.0f)
    {
        const uint4 boneIdx = uint4(src.boneIdx01 & 0xFFFF, src.boneIdx01 >> 16, src.boneIdx23 & 0xFFFF, src.boneIdx23 >> 16);

        const matrix skin = w.x * act_BoneTr[boneIdx.x]
                          + w.y * act_BoneTr[boneIdx.y]
                          + w.z * act_BoneTr[boneIdx.z]
                          + w.w * act_BoneTr[boneIdx.w];

        dst.pos = mul(float4(src.pos, 1.0f), skin).xyz;
        dst.normal = mul(src.normal, (float3x3)skin);
        dst.tangent = mul(src.tangent, (float3x3)skin);
    }

    g_Dst[threadId.x] = dst;
}
