sampler2D OriginalWater : register(s0);
sampler2D SceneColor : register(s1);
samplerCUBE SkyEnvironment : register(s2);
sampler2D SceneDepth : register(s3);
sampler2D WaterNormals : register(s4);
sampler2D CausticField : register(s5);
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
float4 Optics : register(c14); // valid depth, absorption, shore width, SSR
float4 Camera : register(c15); // projection _11, _22, _33, _43
float4 Viewport : register(c16); // origin UV, extent UV
float4 Trace : register(c17); // steps, world-distance budget, normal map, source depth quantum
float4 BodyAmbient : register(c18);
float4 BodyDiffuse : register(c19);
float4 Subsurface : register(c20);

float DepthZ(float2 uv) {
    float z=tex2Dlod(SceneDepth,float4(uv,0,0)).r;
    return Camera.w/min(-.000001,z-Camera.z);
}
float3 ViewPoint(float2 uv,float z) {
    float2 ndc=((uv-Viewport.xy)/Viewport.zw)*float2(2,-2)+float2(-1,1);
    return float3(ndc/Camera.xy,1)*z;
}
float3 WorldPoint(float3 p) { return EyeTime.xyz+mul((float3x3)View,p); }
float2 ProjectUV(float3 p) {
    return ((p.xy*Camera.xy/p.z)*float2(.5,-.5)+.5)*Viewport.zw+Viewport.xy;
}
float4 TraceReflection(float3 start,float3 direction,float3 planeNormal) {
    float previousT=.06;
    float previousDelta=-.01;
    [loop] for(int i=0;i<24;++i) {
        if(i>=Trace.x) break;
        float u=(i+1)/Trace.x;
        float t=.06+u*u*Trace.y;
        float3 ray=start+direction*t;
        if(ray.z<=.1) break;
        float2 uv=ProjectUV(ray);
        float2 localUV=(uv-Viewport.xy)/Viewport.zw;
        if(any(localUV<=.002)||any(localUV>=.998)) break;
        float sceneZ=DepthZ(uv);
        float delta=ray.z-sceneZ;
        if(delta>=0 && previousDelta<0) {
            float lo=previousT,hi=t;
            [loop] for(int j=0;j<12;++j) {
                float mid=(lo+hi)*.5;
                float3 q=start+direction*mid;
                float z=DepthZ(ProjectUV(q));
                if(q.z>z)hi=mid;else lo=mid;
            }
            float3 q=start+direction*((lo+hi)*.5);
            float2 hitUV=ProjectUV(q);
            float hitZ=DepthZ(hitUV);
            float3 hit=ViewPoint(hitUV,hitZ);
            // Reject silhouette jumps and loose depth matches instead of stretching a bank into water.
            float tolerance=clamp(2*hitZ*hitZ*Trace.w/max(.01,abs(Camera.w)),.03,.35);
            float edgeZ=max(abs(DepthZ(hitUV+float2(Screen.x,0))-hitZ),abs(DepthZ(hitUV+float2(0,Screen.y))-hitZ));
            float2 edge=min((hitUV-Viewport.xy)/Viewport.zw,1-(hitUV-Viewport.xy)/Viewport.zw);
            float confidence=saturate(min(edge.x,edge.y)*14)*saturate(1-hi/Trace.y);
            confidence*=abs(q.z-hitZ)<tolerance && dot(hit-start,planeNormal)>.01;
            // Precision of the depth format is not a bound on a sloped bank's pixel gradient.
            // Keep the silhouette rejection relative to view distance, independently of D16/D24.
            confidence*=saturate(1-edgeZ/max(.1,hitZ*.015));
            return float4(tex2Dlod(SceneColor,float4(hitUV,0,0)).rgb,confidence);
        }
        previousT=t;previousDelta=delta;
    }
    return 0;
}
float4 WaterPS(float2 uv:TEXCOORD0,float3 relative:TEXCOORD1,float3 normal:TEXCOORD2,float fogDepth:TEXCOORD3,float4 color:COLOR0,float2 pixel:VPOS):COLOR0 {
    float t=EyeTime.w*Sun.w;
    float3 world=relative+EyeTime.xyz;
    float2 p=world.xz*Surface.y;
    float2 footprintXY=max(abs(ddx(p)),abs(ddy(p)));
    float footprint=max(footprintXY.x,footprintXY.y)*1.414214;
    float3 viewPosition=mul(float4(relative,0),View).xyz;
    float3 v=normalize(-relative);
    float3 geometric=cross(ddx(relative),ddy(relative));
    geometric=normalize(geometric+float3(0,.00000001,0));
    if(geometric.y<0)geometric=-geometric;
    float surface=smoothstep(.45,.85,geometric.y);
    float3 planeNormal=mul(float4(geometric,0),View).xyz;
    float3 traceNormal=geometric;
    float3 traceReflected=reflect(-v,traceNormal);
    float4 worldReflection=0;
    if(Optics.x>.5 && Optics.w>0 && traceReflected.y>.005 && v.y>0 && surface>.01){
        float3 direction=mul(float4(traceReflected,0),View).xyz;
        worldReflection=TraceReflection(viewPosition+planeNormal*.05,direction,planeNormal);
    }
    // Crossed, domain-warped wave bands break the old regular horizontal comb.
    float firstLOD=clamp(log2(max(1,footprint*128*.109)),0,7);
    float2 first=tex2Dlod(WaterNormals,float4(p*.065+t*float2(.011,.007),0,firstLOD)).rg*2-1;
    float2 rotated=float2(p.x*.8-p.y*.6,p.x*.6+p.y*.8);
    float2 second=tex2Dlod(WaterNormals,float4(rotated*.109+t*float2(-.008,.009),0,firstLOD)).rg*2-1;
    float2 ripple=first+float2(second.x*.8+second.y*.6,-second.x*.6+second.y*.8)*.5;
    // Attenuate subpixel ripples at distance instead of letting specular highlights shimmer.
    float rippleFade=rcp(1+footprint*footprint*6);
    // Animated shading is separate from the static geometric plane used to locate reflected objects.
    float3 n=normalize(lerp(geometric,normal,surface)+float3(ripple.x,0,ripple.y)*Surface.x*.12*rippleFade*surface);
    float2 screenUV=(pixel+.5)*Screen.xy;
    float path=0,shore=1;
    if(Optics.x>.5){
        float floorZ=DepthZ(screenUV);
        path=clamp((floorZ-viewPosition.z)*length(viewPosition)/max(.1,viewPosition.z),0,80);
        shore=smoothstep(0,max(Optics.z,fwidth(path)*2),path);
        n=normalize(lerp(float3(0,1,0),n,shore));
    }
    if(v.y<0)n=-n;
    float facing=saturate(abs(dot(n,v)));
    float fresnel=.025+.975*pow(1-facing,5);
    float4 tex=tex2D(OriginalWater,uv+ripple*Surface.x*.008);
    float3 body=lerp(float3(.16,.31,.39),float3(.09,.37,.37),Sun.z);
    body=lerp(body,tex.rgb,Detail.x)*lerp(float3(.75,.78,.82),float3(1,1,1),saturate(color.rgb));
    float3 light=normalize(LightDir.xyz+float3(0,.00001,0));
    float3 illumination=saturate(BodyAmbient.rgb+BodyDiffuse.rgb*saturate(dot(n,light)));
    body*=illumination;
    // Sky/cloud environment with screen-space world hits where depth is available.
    float3 reflected=reflect(-v,n);
    float3 sky=lerp(FogColor.rgb,float3(.36,.58,.78),saturate(reflected.y*.8+.2));
    sky*=saturate(BodyAmbient.rgb+BodyDiffuse.rgb);
    if(Detail.w>.5) sky=texCUBE(SkyEnvironment,reflected).rgb;
    // No arbitrary screen-pixel warp after a ray hit: it moves unrelated scene pixels into the mirror.
    sky=lerp(sky,worldReflection.rgb,worldReflection.a*Optics.w);
    float reflectionWeight=fresnel*max(Surface.w,worldReflection.a*Optics.w)*shore*surface;
    float3 base=body;
    float opacity=saturate(Surface.z+fresnel*(1-Surface.z)*Surface.w);
    if(Screen.z>.5){
        float3 viewNormal=mul(float4(n,0),View).xyz;
        float2 offset=viewNormal.xy*float2(1,-1)*Screen.w*12*Screen.xy*rippleFade*shore*surface;
        float2 sampleUV=clamp(screenUV+offset,Screen.xy,1-Screen.xy);
        if(Optics.x>.5){
            float sampleZ=DepthZ(sampleUV);
            float3 samplePosition=ViewPoint(sampleUV,sampleZ);
            if(sampleZ<viewPosition.z+.01 || dot(samplePosition-viewPosition,planeNormal)>.03)sampleUV=screenUV;
        }
        float3 through=tex2D(SceneColor,sampleUV).rgb;
        if(Subsurface.x>.5){
            float3 focused=(tex2D(CausticField,sampleUV).rgb-128/255.0)*8;
            through*=max(0,1+focused*(saturate(path*4)/(1+path*.1))*shore*surface);
        }
        if(Optics.x>.5 && Optics.y>0){
            float density=-log(max(.001,1-Surface.z))*.14*Optics.y;
            float3 transmission=exp(-float3(1.5,.48,.27)*path*density);
            base=through*transmission+body*(1-transmission);
        } else {
            through*=lerp(float3(1,1,1),float3(.78,.96,.98),Surface.z*Sun.z);
            base=lerp(through,body,Surface.z*.72);
        }
    }
    base=lerp(base,sky,reflectionWeight);
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
    base+=LightColor.rgb*spec*shore*surface;
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
