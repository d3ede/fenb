sampler2D OriginalWater : register(s0);
float4 EyeTime : register(c0);
float4 Surface : register(c1); // ripple strength, scale, opacity, sky
float4 Sun : register(c2); // glints, gloss, tint, speed
float4 LightDir : register(c3);
float4 LightColor : register(c4);
float4 FogColor : register(c5);
float4 Fog : register(c6); // enabled, mode: linear3/exp1/exp2, start, end
float4 FogExtra : register(c7); // density
float4 WaterPS(float2 uv:TEXCOORD0,float3 world:TEXCOORD1,float3 normal:TEXCOORD2,float fogDepth:TEXCOORD3,float4 color:COLOR0):COLOR0 {
    float t=EyeTime.w*Sun.w;
    float2 p=world.xz*Surface.y;
    float2 ripple=float2(cos(dot(p,float2(1.7,.6))+t*1.8)+.45*cos(dot(p,float2(-2.1,1.9))-t*2.3),
                         sin(dot(p,float2(.8,1.5))-t*1.3)+.45*sin(dot(p,float2(1.6,-2.3))+t*2));
    float3 n=normalize(normal+float3(ripple.x,0,ripple.y)*Surface.x*.20);
    float3 v=normalize(EyeTime.xyz-world);
    if(v.y<0)n=-n;
    float facing=saturate(abs(dot(n,v)));
    float fresnel=.025+.975*pow(1-facing,5);
    float4 tex=tex2D(OriginalWater,uv+ripple*Surface.x*.012);
    float3 base=lerp(tex.rgb,float3(.12,.44,.47),Sun.z)*lerp(float3(.65,.68,.72),float3(1,1,1),saturate(color.rgb));
    // Analytic sky environment, not a reflection of scene objects.
    float3 reflected=reflect(-v,n);
    float3 sky=lerp(FogColor.rgb,float3(.36,.58,.78),saturate(reflected.y*.8+.2));
    base=lerp(base,sky,fresnel*Surface.w);
    float3 h=normalize(v+LightDir.xyz);
    float power=lerp(24,240,Sun.y);
    float spec=pow(saturate(dot(n,h)),power)*Sun.x*LightDir.w;
    base+=LightColor.rgb*spec;
    float visibility=1;
    if(Fog.x>.5){
        if(Fog.y>2.5)visibility=saturate((Fog.w-fogDepth)/max(.001,Fog.w-Fog.z));
        else if(Fog.y>1.5)visibility=exp(-pow(FogExtra.x*fogDepth,2));
        else visibility=exp(-FogExtra.x*fogDepth);
    }
    base=lerp(FogColor.rgb,base,visibility);
    // FENB owns water opacity. Legacy vertex/material alpha can be zero even on visible Neuz water.
    // Preserve texture cutouts without multiplying opacity by that unused vertex channel.
    return float4(base,tex.a*saturate(Surface.z+fresnel*Surface.w*.18));
}
