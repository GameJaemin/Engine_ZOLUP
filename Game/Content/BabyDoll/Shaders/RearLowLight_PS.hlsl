Texture2D<float4> sceneColor : register(t0, space0);

float4 PSMain(float4 position : SV_POSITION) : SV_TARGET
{
    const float4 scene = sceneColor.Load(int3(int2(position.xy), 0));
    const float3 color = saturate(scene.rgb);
    const float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    // Lift the recorded shadows without adding a gray floor to black or washing out bright surfaces.
    const float lifted = lerp(luminance, pow(luminance, 0.55f), 0.85f);
    return float4(saturate(color + (lifted - luminance)), scene.a);
}
