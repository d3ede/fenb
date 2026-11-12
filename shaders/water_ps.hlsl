sampler2D OriginalWater : register(s0);
sampler2D SceneColor : register(s1);
samplerCUBE SkyEnvironment : register(s2);
float4 EyeTime : register(c0);
float4 Surface : register(c1); // ripple strength, scale, opacity, sky
float4 Sun : register(c2); // glints, gloss, tint, speed
float4 LightDir : register(c3);
float4 LightColor : register(c4);
float4 FogColor : register(c5);
float4 Fog : register(c6); // enabled, mode: linear3/exp1/exp2, start, end
float4 FogExtra : register(c7); // density
float4 Screen : register(c8); // inverse dimensions, scene copy valid, refraction strength
float4 Detail : register(c9); // original texture, wave height, wavelength, sky cube valid
row_major float4x4 View : register(c10);
float4 WaterPS(float2 uv:TEXCOORD0,float3 world:TEXCOORD1,float3 normal:TEXCOORD2,float fogDepth:TEXCOORD3,float4 color:COLOR0,float2 pixel:VPOS):COLOR0 {
    float t=EyeTime.w*Sun.w;
    float2 p=world.xz*Surface.y;
    // Crossed, domain-warped wave bands break the old regular horizontal comb.
    float2 q=p+.65*float2(sin(p.y*.31+t*.23),sin(p.x*.27-t*.19));
    float a=dot(q,float2(.93,.37))+t*1.1;
    float b=dot(q,float2(-.61,1.37))-t*1.43;
    float c=dot(q,float2(1.73,-1.19))+t*1.87;
    float2 ripple=cos(a)*float2(.93,.37)+.53*cos(b)*float2(-.61,1.37)+.26*cos(c)*float2(1.73,-1.19);
    // Attenuate subpixel ripples at distance instead of letting specular highlights shimmer.
    float footprint=max(length(ddx(p)),length(ddy(p)));
    float rippleFade=rcp(1+footprint*footprint*6);
    // Evaluate the long-wave slope per pixel: coarse Neuz triangles must not flatten the normals.
    float k=6.2831853/Detail.z;
    float wa=dot(world.xz,float2(.8,.6))*k+t;
    float wb=dot(world.xz,float2(-.4,.9165))*k*1.57-t*1.24;
    float2 slope=Detail.y*k*(cos(wa)*float2(.8,.6)+.628*cos(wb)*float2(-.4,.9165));
    float3 n=normalize(float3(-slope.x,1,-slope.y)+float3(ripple.x,0,ripple.y)*Surface.x*.12*rippleFade);
    float3 v=normalize(EyeTime.xyz-world);
    if(v.y<0)n=-n;
    float facing=saturate(abs(dot(n,v)));
    float fresnel=.025+.975*pow(1-facing,5);
    float4 tex=tex2D(OriginalWater,uv+ripple*Surface.x*.008);
    float3 body=lerp(float3(.16,.31,.39),float3(.09,.37,.37),Sun.z);
    body=lerp(body,tex.rgb,Detail.x)*lerp(float3(.75,.78,.82),float3(1,1,1),saturate(color.rgb));
    // Captured sky/cloud geometry where available, analytic fallback otherwise. No world objects.
    float3 reflected=reflect(-v,n);
    float3 sky=lerp(FogColor.rgb,float3(.36,.58,.78),saturate(reflected.y*.8+.2));
    if(Detail.w>.5) sky=texCUBE(SkyEnvironment,reflected).rgb;
    float3 base=body;
    float opacity=saturate(Surface.z+fresnel*(1-Surface.z)*Surface.w);
    if(Screen.z>.5){
        float2 screenUV=(pixel+.5)*Screen.xy;
        float3 viewNormal=mul(float4(n,0),View).xyz;
        float2 offset=viewNormal.xy*float2(1,-1)*Screen.w*12*Screen.xy*rippleFade;
        // Keep distortion small and clamp at screen edges. No claim of depth-aware rejection.
        float2 sampleUV=clamp(screenUV+offset,Screen.xy,1-Screen.xy);
        float3 through=tex2D(SceneColor,sampleUV).rgb;
        through*=lerp(float3(1,1,1),float3(.78,.96,.98),Surface.z*Sun.z);
        base=lerp(through,body,Surface.z*.72);
    }
    base=lerp(base,sky,fresnel*Surface.w);
    float3 light=normalize(LightDir.xyz+float3(0,.00001,0));
    float3 h=normalize(v+light);
    float nh=saturate(dot(n,h)),nl=saturate(dot(n,light)),nv=max(.04,facing);
    float roughness=lerp(.32,.11,Sun.y);
    float alpha2=pow(roughness,4);
    float denom=nh*nh*(alpha2-1)+1;
    float distribution=alpha2/(3.14159265*denom*denom);
    float smithK=(roughness+1)*(roughness+1)*.125;
    float geometry=nv/(nv*(1-smithK)+smithK)*nl/(nl*(1-smithK)+smithK);
    float lightFresnel=.02+.98*pow(1-saturate(dot(v,h)),5);
    float spec=min(1.5,distribution*geometry*lightFresnel/max(.001,4*nv))*Sun.x*LightDir.w;
    base+=LightColor.rgb*spec;
    float visibility=1;
    if(Fog.x>.5){
        if(Fog.y>2.5)visibility=saturate((Fog.w-fogDepth)/max(.001,Fog.w-Fog.z));
        else if(Fog.y>1.5)visibility=exp(-pow(FogExtra.x*fogDepth,2));
        else visibility=exp(-FogExtra.x*fogDepth);
    }
    // The scene copy already contains fog on underwater geometry; fog only the new contribution.
    if(Screen.z>.5){
        float3 unshifted=tex2D(SceneColor,(pixel+.5)*Screen.xy).rgb;
        base=lerp(unshifted,base,visibility);
    } else base=lerp(FogColor.rgb,base,visibility);
    // FENB owns water opacity. Legacy vertex/material alpha can be zero even on visible Neuz water.
    // Preserve texture cutouts without multiplying opacity by that unused vertex channel.
    return float4(base,tex.a*(Screen.z>.5?1:opacity));
}
