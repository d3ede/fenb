// Four base-64 digits fit exactly in the FP32 receiver alpha lane. The sign
// is assigned separately by the capture pass to identify winter ground.
float EncodeGrassGroundColor(float4 sampleColor) {
    return 1 + dot(floor(saturate(sampleColor) * 63 + .5), float4(1, 64, 4096, 262144));
}

float4 FilterGrassGroundColor(float4 encoded, float4 weights, out float signedLuma) {
    // Decode the four neighbors in parallel, one vector per color component.
    float4 packed = max(0, abs(encoded) - 1);
    float4 upperR = floor(packed / 64), upperG = floor(packed / 4096);
    float4 opacity = floor(packed / 262144);
    float4 r = packed - upperR * 64, g = upperR - upperG * 64, b = upperG - opacity * 64;
    float4 luma = .2126*r + .7152*g + .0722*b;
    signedLuma = dot(luma * sign(encoded), weights) / 63;
    return float4(dot(r,weights), dot(g,weights), dot(b,weights), dot(opacity,weights)) / 63;
}
