#if defined(OUTPUT_GBUFFER) || defined(OUTPUT_COLORID) || defined(OUTPUT_DEPTHONLY) || defined(OUTPUT_ID)
#error "defaultDebug_PS: Color 레이아웃만 지원"
#endif


#include "common/registers.hlsli"


cbuffer DebugCB : OBJECT_CB(2)
{
    float4 obj_DebugColor;
};


float4 PSMain(float4 position : SV_POSITION) : SV_TARGET
{
    return obj_DebugColor;
}
