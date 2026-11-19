#if !defined(OUTPUT_ID)
#error "defaultPick_PS: Id 레이아웃만 지원"
#endif


#include "common/registers.hlsli"


cbuffer PickCB : OBJECT_CB(2)
{
    uint obj_PickId;
};


uint PSMain(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
#if defined(ALPHA_CLIP)
    const float alpha = g_Tex[0].Sample(g_SamplerAnisoWrap, uv).a * obj_Col.a;
    clip(alpha - obj_AlphaCutoff);
#endif

    return obj_PickId;
}
