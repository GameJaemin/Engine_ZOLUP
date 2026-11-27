Texture2D<float4> sceneColor : register(t0, space0);

cbuffer ChaseView : register(b0, space2)
{
    float grainStrength;
    float vignetteStrength;
    float grainFrame;
};

float4 PSMain(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    const float4 scene = sceneColor.Load(int3(int2(position.xy), 0));
    const uint2 pixel = uint2(position.xy);
    uint seed = pixel.x * 1973u + pixel.y * 9277u + uint(grainFrame) * 26699u;
    seed = (seed ^ (seed >> 16)) * 0x7feb352du;
    seed = (seed ^ (seed >> 15)) * 0x846ca68bu;
    const float noise = float((seed ^ (seed >> 16)) & 65535u) / 65535.0f;
    const float2 fromCenter = uv * 2.0f - 1.0f;
    const float edge = smoothstep(0.28f, 1.24f, length(fromCenter));
    const float luminance = dot(scene.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    const float grain = (noise - 0.5f) * grainStrength * lerp(0.35f, 1.0f, edge) * smoothstep(0.0f, 0.12f, luminance);
    return float4(saturate((scene.rgb + grain) * (1.0f - edge * vignetteStrength)), scene.a);
}
