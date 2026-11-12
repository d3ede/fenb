// SMAA 1x, High preset. See vendor/smaa/LICENSE.txt and UPSTREAM.md.
float4 Metrics : register(c0);
#define SMAA_RT_METRICS Metrics
#define SMAA_HLSL_3
#define SMAA_PRESET_HIGH
#include "../vendor/smaa/SMAA.hlsl"
sampler2D Image : register(s0);
sampler2D Second : register(s1);
sampler2D Search : register(s2);

float4 SMAAEdges(float2 uv : TEXCOORD0) : COLOR0 {
    float4 offset[3];
    SMAAEdgeDetectionVS(uv, offset);
    return float4(SMAAColorEdgeDetectionPS(uv, offset, Image), 0, 0);
}
float4 SMAAWeights(float2 uv : TEXCOORD0) : COLOR0 {
    float2 pixel;
    float4 offset[3];
    SMAABlendingWeightCalculationVS(uv, pixel, offset);
    return SMAABlendingWeightCalculationPS(uv, pixel, offset, Image, Second, Search, 0);
}
float4 SMAABlend(float2 uv : TEXCOORD0) : COLOR0 {
    float4 offset;
    SMAANeighborhoodBlendingVS(uv, offset);
    float4 color = SMAANeighborhoodBlendingPS(uv, offset, Image, Second);
    color.a = tex2D(Image, uv).a;
    return color;
}
