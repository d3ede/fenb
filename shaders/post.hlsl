sampler2D Scene : register(s0);
sampler2D Glow : register(s1);
sampler2D Exposure : register(s2);
sampler2D MeanLuminance : register(s3);
float4 Bloom : register(c0); // strength, threshold (linear), knee, radius
float4 Grade : register(c1); // exposure, contrast, saturation, gamma
float4 Balance : register(c2); // temperature, tint, shadows, highlights
float4 Options : register(c3); // bloom enabled, color enabled
float4 Step : register(c4);
float4 Automatic : register(c5);

float4 Bright(float2 uv : TEXCOORD0) : COLOR0 {
    float3 a = tex2D(Scene, uv + Step.xy * float2(-.5,-.5)).rgb;
    float3 b = tex2D(Scene, uv + Step.xy * float2( .5,-.5)).rgb;
    float3 c = tex2D(Scene, uv + Step.xy * float2(-.5, .5)).rgb;
    float3 d = tex2D(Scene, uv + Step.xy * float2( .5, .5)).rgb;
    float3 rgb = pow(max((a+b+c+d)*.25, 0), 2.2);
    float luma = max(rgb.r, max(rgb.g, rgb.b));
    float soft = clamp(luma - Bloom.y + Bloom.z, 0, 2 * Bloom.z);
    soft = soft * soft / (4 * Bloom.z + 0.00001);
    float weight = max(luma - Bloom.y, soft) / max(luma, .00001);
    return float4(rgb * weight, 1);
}
float4 Blur(float2 uv : TEXCOORD0) : COLOR0 {
    float2 offset = Step.xy * Bloom.w;
    float3 c = tex2D(Scene, uv).rgb * .2270270270;
    c += tex2D(Scene, uv + offset * 1.3846153846).rgb * .3162162162;
    c += tex2D(Scene, uv - offset * 1.3846153846).rgb * .3162162162;
    c += tex2D(Scene, uv + offset * 3.2307692308).rgb * .0702702703;
    c += tex2D(Scene, uv - offset * 3.2307692308).rgb * .0702702703;
    return float4(c, 1);
}
float4 Composite(float2 uv : TEXCOORD0) : COLOR0 {
    float4 source = tex2D(Scene, uv);
    float3 rgb = pow(max(source.rgb, 0), 2.2);
    if (Automatic.y > .5) {
        // Adapt one scalar pivot from world-only log luminance. The same hue-preserving curve
        // acts on every mesh, including skinned characters and unlit props, without screen halos.
        float pivot=clamp(exp2(-tex2D(MeanLuminance,float2(.5,.5)).r),.02,.18);
        float luma=dot(rgb,float3(.2126,.7152,.0722));
        float mapped=pivot*pow(max(.000001,luma)/pivot,1-.55*Automatic.z);
        rgb*=clamp(mapped/max(.000001,luma),.45,3.5);
    }
    if (Options.x > .5) rgb += tex2D(Glow, uv).rgb * Bloom.x;
    if (Automatic.x > .5) rgb *= exp2(tex2D(Exposure,float2(.5,.5)).r);
    if (Options.y > .5) {
        rgb *= exp2(Grade.x);
        rgb *= float3(1 + Balance.x*.18, 1 + Balance.y*.12, 1 - Balance.x*.18);
        float luma = dot(rgb, float3(.2126,.7152,.0722));
        rgb += Balance.z * pow(saturate(1-luma), 2) * .25;
        rgb += Balance.w * pow(saturate(luma), 2) * .25;
    }
    rgb = pow(saturate(rgb), 1/2.2);
    if (Options.y > .5) {
        rgb = (rgb - .5) * Grade.y + .5;
        float gray = dot(rgb, float3(.2126,.7152,.0722));
        rgb = lerp(gray.xxx, rgb, Grade.z);
        rgb = pow(saturate(rgb), 1/Grade.w);
    }
    return float4(saturate(rgb), source.a);
}

