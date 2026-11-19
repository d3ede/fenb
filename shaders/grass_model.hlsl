// Original env_grass mesh. Its position/UV data and local origin stay intact.
sampler2D Ground:register(s0);
sampler2D Contacts:register(s1);
sampler2D Summer:register(s2);
sampler2D Winter:register(s3);
row_major float4x4 ViewProjection:register(c0);
float4 Eye:register(c4);
float4 Map:register(c5);
float4 Shape:register(c6);
float4 Band:register(c7);
float4 Wind:register(c8);
float4 SunDirection:register(c9);
float4 SunColor:register(c10);
float4 Ambient:register(c11);
float4 Look:register(c12);
float4 Fog:register(c13);
float4 FogColor:register(c14);
float4 Extra:register(c15);
float4 Forward:register(c16);
float4 Center:register(c17);
float4 Lod:register(c18);
float4 CardControl:register(c19);
struct Input {float3 position:POSITION;float3 normal:NORMAL;float2 uv:TEXCOORD0;float2 anchor:TEXCOORD1;float2 cell:TEXCOORD2;};
struct Output {float4 position:POSITION;float4 color:COLOR0;float3 uv:TEXCOORD0;};
float Hash(float2 p){float3 v=frac(float3(p.x,p.y,p.x)*.1031);v+=dot(v,v.yzx+33.33);return frac((v.x+v.y)*v.z);}
float4 Surface(float2 uv){
    float2 p=uv*Map.w-.5,base=floor(p),f=frac(p),a=(base+.5)/Map.w,b=1/Map.w;
    float4 v0=tex2Dlod(Ground,float4(a,0,0)),v1=tex2Dlod(Ground,float4(a+float2(b.x,0),0,0));
    float4 v2=tex2Dlod(Ground,float4(a+float2(0,b.y),0,0)),v3=tex2Dlod(Ground,float4(a+b,0,0));
    float4 value=lerp(lerp(v0,v1,f.x),lerp(v2,v3,f.x),f.y);
    value.g=min(min(abs(v0.g),abs(v1.g)),min(abs(v2.g),abs(v3.g)))*(value.g<0?-1:1);
    float high=max(max(v0.r,v1.r),max(v2.r,v3.r)),low=min(min(v0.r,v1.r),min(v2.r,v3.r));
    if(high-low>max(.4,16*Map.z/Map.w))value.g=0;
    return value;
}
float FogFactor(float3 p){
    float mode=abs(Fog.x);if(mode<.5)return 1;
    float d=Fog.x<0?length(p-Eye.xyz):dot(p-Eye.xyz,Forward.xyz);
    if(mode<1.5)return saturate(exp(-d*Fog.y));
    if(mode<2.5)return saturate(exp(-d*d*Fog.y*Fog.y));
    return saturate((Fog.w-d)/max(.0001,Fog.w-Fog.z));
}
Output GrassModelVS(Input i){
    Output o=(Output)0;
    float2 cell=(i.cell+Extra.yz)*Lod.x;
    float r=Hash(cell),r2=Hash(cell+17.3),r3=Hash(cell+41.7);
    float2 origin=(cell+(float2(r,r2)-.5)*.35)*Lod.y;
    float bandDistance=length(origin-Center.xz);
    if(bandDistance<Band.y||bandDistance>=Band.z){o.position=float4(2,2,2,1);return o;}
    float sn,cs;sincos(r3*6.2831853,sn,cs);
    float width=clamp(Shape.y/.03,.25,4);
    float2 local=float2(i.position.x*cs-i.position.z*sn,i.position.x*sn+i.position.z*cs)*width;
    float2 anchor=float2(i.anchor.x*cs-i.anchor.y*sn,i.anchor.x*sn+i.anchor.y*cs)*width;
    float2 planted=origin+anchor;
    float2 uv=float2((planted.x-Map.x)/(2*Map.z)+.5,.5-(planted.y-Map.y)/(2*Map.z));
    float4 surface=Surface(uv);float snow=surface.a<0||Extra.w>1.5?1:0;
    float coverage=smoothstep(.02,.18,abs(surface.g));
    float slope=smoothstep(max(0,Shape.w-.07),min(1,Shape.w+.04),surface.b);
    float contact=tex2Dlod(Contacts,float4(uv,0,0)).r;
    bool water=contact>2,paving=contact>1&&!water,solid=contact>0&&!water;
    float obstacle=Center.y+(.5-contact)*1024;if(paving)obstacle=Center.y+(contact-1.5)*1024;
    float clearance=obstacle-surface.r;
    bool blocked=solid&&(paving?clearance>=-.5&&clearance<=.85:clearance>=-.10&&clearance<=.14);
    bool submerged=water&&surface.r<=Center.y+(contact-2.5)*1024+.025;
    if(any(uv<0)||any(uv>1)||coverage*slope<.001||(snow>0&&Extra.w<.5)||submerged||blocked){o.position=float4(2,2,2,1);return o;}
    float distance=length(planted-Center.xz);
    float fade=1-smoothstep(Lod.z-max(1,Lod.z*Lod.w),Lod.z,distance);
    if(fade<.001){o.position=float4(2,2,2,1);return o;}
    float height=Shape.x/.6*lerp(1,.85+.30*Hash(cell+73.2),Shape.z)*sqrt(fade*coverage*slope);
    if(solid&&!paving&&clearance>.14)height=min(height,(clearance-.04)/.88481617);
    float t=saturate(i.position.y/.88481617);
    float phase=dot(planted,float2(.19,.13))+Wind.w*Extra.x*1.35;
    float sway=(sin(phase)*.65+sin(phase*1.7+r3*6.2831853)*.35)*Wind.z;
    float3 p=float3(origin.x+local.x,surface.r+i.position.y*height,origin.y+local.y);
    p.xz+=Wind.xy*sway*height*t*t*.35;
    float light=.45+.55*abs(dot(normalize(i.normal),SunDirection.xyz));
    float3 lighting=Ambient.rgb+SunColor.rgb*light;
    lighting/=max(1,max(lighting.r,max(lighting.g,lighting.b)));
    float neutral=dot(lighting,float3(.2126,.7152,.0722));lighting=lerp(lighting,lerp(neutral,lighting,.25),snow);
    lighting*=lerp(.96,1.04,r3)*lerp(1-Look.w*.22,1.04,t);
    o.position=mul(float4(p,1),ViewProjection);o.color=float4(lighting,FogFactor(p));
    // OBJ V is bottom-up; convert the convention only, without repacking UVs.
    o.uv=float3(i.uv.x,1-i.uv.y,snow);return o;
}
float4 GrassModelPS(Output i):COLOR0 {
    float4 art=lerp(tex2D(Summer,i.uv.xy),tex2D(Winter,i.uv.xy),i.uv.z);clip(art.a-.5);
    float shade=dot(art.rgb,float3(.2126,.7152,.0722));
    float3 color=lerp(art.rgb,float3(.58,.49,.20)*lerp(.62,1.2,shade),CardControl.z*(1-i.uv.z))*i.color.rgb;
    float luma=dot(color,float3(.2126,.7152,.0722));color=lerp(luma,color,CardControl.y)*CardControl.x;
    return float4(lerp(FogColor.rgb,color,i.color.a),1);
}
