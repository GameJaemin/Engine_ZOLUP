#include "common/registers.hlsli"


cbuffer ShadowViewCB : ACTION_DATA_CB
{
    matrix act_LightVP;
};


#if VERTEX_SKINNED
cbuffer BoneCB : OBJECT_CB(1)
{
    matrix obj_BoneTr[SKIN_BONE_MAX];
};
#endif


struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};


#if VERTEX_SKINNED
PSInput VSMain(float4 position : POSITION, float2 uv : TEXCOORD,
                uint4 boneIdx : BLENDINDICES, float4 boneWeight : BLENDWEIGHT)
#elif VERTEX_SIMPLE
PSInput VSMain(float4 position : POSITION)
#else
PSInput VSMain(float4 position : POSITION, float2 uv : TEXCOORD)
#endif
{
    PSInput result;

    float3 posL = position.xyz;

#if VERTEX_SKINNED
    const float wSum = boneWeight.x + boneWeight.y + boneWeight.z + boneWeight.w;
    if (wSum > 0.0f)
    {
        matrix skin = boneWeight.x * obj_BoneTr[boneIdx.x]
                    + boneWeight.y * obj_BoneTr[boneIdx.y]
                    + boneWeight.z * obj_BoneTr[boneIdx.z]
                    + boneWeight.w * obj_BoneTr[boneIdx.w];

        posL = mul(float4(posL, 1.0f), skin).xyz;
    }
#endif

    float4 posW = mul(float4(posL, 1.0f), obj_W);
    result.position = mul(posW, act_LightVP);

#if VERTEX_SIMPLE
    result.uv = float2(0.0f, 0.0f);
#else
    result.uv = uv;
#endif

    return result;
}
