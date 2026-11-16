sampler2D Scene : register(s0);
sampler2D Glow : register(s1);
sampler2D Exposure : register(s2);
sampler2D MeanLuminance : register(s3);
float4 Bloom : register(c0); // strength, threshold (linear), knee, radius
float4 Grade : register(c1); // exposure, contrast, saturation, gamma
float4 Balance : register(c2); // temperature, tint, shadows, highlights
float4 Options : register(c3); // bloom enabled, color enabled, detail strength, texture contrast
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
    // Two-scale detail enhancement in the existing pass. Fine edges and broader
    // texture shading are separate bands; no synthetic grain or retained history.
    [branch] if (Options.z > 0) {
        const float3 lumaWeights = float3(.2126,.7152,.0722);
        float center = dot(source.rgb, lumaWeights);
        // Explicit LOD is required for texture reads inside an SM3 dynamic branch.
        float north = dot(tex2Dlod(Scene, float4(uv - float2(0, Step.y), 0, 0)).rgb, lumaWeights);
        float south = dot(tex2Dlod(Scene, float4(uv + float2(0, Step.y), 0, 0)).rgb, lumaWeights);
        float west = dot(tex2Dlod(Scene, float4(uv - float2(Step.x, 0), 0, 0)).rgb, lumaWeights);
        float east = dot(tex2Dlod(Scene, float4(uv + float2(Step.x, 0), 0, 0)).rgb, lumaWeights);
        float low = min(center, min(min(north, south), min(west, east)));
        float high = max(center, max(max(north, south), max(west, east)));
        float span = high - low;
        float nearMean = (north + south + west + east) * .25;
        float edgeGuard = 1 - smoothstep(.12, .30, span);
        float fine = (center - nearMean) * 2.2 * smoothstep(.003, .015, span);
        float textureBand = 0;
        [branch] if (Options.w > 0) {
            // Four diagonal samples cover the scale of painted stone/wood detail.
            // Skipping this band retains the four-read fine-detail mode.
            float2 radius = Step.xy * 7;
            float a = dot(tex2Dlod(Scene, float4(uv + radius, 0, 0)).rgb, lumaWeights);
            float b = dot(tex2Dlod(Scene, float4(uv - radius, 0, 0)).rgb, lumaWeights);
            float c = dot(tex2Dlod(Scene, float4(uv + radius * float2(-1,1), 0, 0)).rgb, lumaWeights);
            float d = dot(tex2Dlod(Scene, float4(uv + radius * float2(1,-1), 0, 0)).rgb, lumaWeights);
            low = min(low, min(min(a,b), min(c,d)));
            high = max(high, max(max(a,b), max(c,d)));
            float wideSpan = high - low;
            float structure = smoothstep(.008, .04, wideSpan) * (1 - smoothstep(.30,.65,wideSpan));
            // Reject samples across large brightness discontinuities. A plain
            // wide blur would emboss silhouettes and lay halos over flat surfaces.
            float4 samples = float4(a,b,c,d);
            float4 weights = saturate(1 - abs(samples-center)/.24);
            float wideMean = (center+dot(samples,weights))/(1+dot(weights,1));
            textureBand = (nearMean - wideMean) * 5.5 * Options.w * structure;
        }
        float tones = smoothstep(.015,.08,center) * (1 - smoothstep(.86,.99,center));
        float correction = (fine + textureBand) * Options.z * edgeGuard * tones;
        correction = clamp(correction, -.14 * Options.z, .14 * Options.z);
        // Permit controlled peak enhancement, unlike the old extrema clamp which
        // erased the strongest texture detail. Keep chroma and highlight headroom.
        float margin = (high-low) * .35;
        correction = clamp(correction, low-center-margin, high-center+margin);
        correction = clamp(correction, -min(source.r,min(source.g,source.b)),
                            1-max(source.r,max(source.g,source.b)));
        source.rgb += correction;
    }
    float3 rgb = pow(max(source.rgb, 0), 2.2);
    if (Automatic.y > .5) {
        // Adapt one scalar pivot from world-only log luminance. The same hue-preserving curve
        // acts on every mesh, including skinned characters and unlit props, without screen halos.
        float pivot=clamp(exp2(-tex2D(MeanLuminance,float2(.5,.5)).r),.02,.18);
        float luma=dot(rgb,float3(.2126,.7152,.0722));
        float mapped=pivot*pow(max(.000001,luma)/pivot,1-.55*Automatic.z);
        rgb*=clamp(mapped/max(.000001,luma),.45,3.5);
    }
    // Keep the sharp core of emissive art; bloom belongs around it, not on top of it again.
    float bloomRoom=1-smoothstep(.35,.95,max(source.r,max(source.g,source.b)));
    if (Options.x > .5) rgb += tex2D(Glow, uv).rgb * (Bloom.x*bloomRoom);
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

