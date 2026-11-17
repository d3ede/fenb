// Instanced, tapered ribbons: real 3D silhouettes, pinned roots and world-space wind.
sampler2D Ground:register(s0);
row_major float4x4 ViewProjection:register(c0);
float4 Eye:register(c4);
float4 Map:register(c5); // map center xz, radius, resolution
float4 Shape:register(c6); // height, width, variation, slope cosine
float4 Band:register(c7); // spacing, inner, outer, fade width
float4 Wind:register(c8); // direction xz, strength, time
float4 SunDirection:register(c9);
float4 SunColor:register(c10);
float4 Ambient:register(c11);
float4 Look:register(c12); // brightness, saturation, dryness, base shading
float4 Fog:register(c13);
float4 FogColor:register(c14);
float4 Extra:register(c15); // wind speed, cell anchor xz, unused
float4 Forward:register(c16);
struct Input {float4 blade:POSITION;float2 offset:TEXCOORD0;float height:TEXCOORD1;float2 cell:TEXCOORD2;};
struct Output {float4 position:POSITION;float4 color:COLOR0;float2 leaf:TEXCOORD0;};
float Hash(float2 p) {
    float3 v=frac(float3(p.x,p.y,p.x)*.1031);
    v+=dot(v,v.yzx+33.33);
    return frac((v.x+v.y)*v.z);
}
float4 Surface(float2 uv) {
    float2 p=uv*Map.w-.5,base=floor(p),f=frac(p);
    float2 a=(base+.5)/Map.w,b=1/Map.w;
    float4 v0=tex2Dlod(Ground,float4(a,0,0));
    float4 v1=tex2Dlod(Ground,float4(a+float2(b.x,0),0,0));
    float4 v2=tex2Dlod(Ground,float4(a+float2(0,b.y),0,0));
    float4 v3=tex2Dlod(Ground,float4(a+b,0,0));
    return lerp(lerp(v0,v1,f.x),lerp(v2,v3,f.x),f.y);
}
float FogFactor(float3 p) {
    float mode=abs(Fog.x);
    if(mode<.5)return 1;
    float d=Fog.x<0?length(p-Eye.xyz):dot(p-Eye.xyz,Forward.xyz);
    if(mode<1.5)return saturate(exp(-d*Fog.y));
    if(mode<2.5)return saturate(exp(-d*d*Fog.y*Fog.y));
    return saturate((Fog.w-d)/max(.0001,Fog.w-Fog.z));
}
Output GrassBladeVS(Input i) {
    Output o=(Output)0;
    float2 cell=i.cell+Extra.yz;
    float r=Hash(cell),r2=Hash(cell+17.3),r3=Hash(cell+41.7);
    float2 root=(cell+float2(r,r2)*.8+.1)*Band.x;
    float distance=length(root-Eye.xz);
    float2 uv=float2((root.x-Map.x)/(2*Map.z)+.5,.5-(root.y-Map.y)/(2*Map.z));
    float4 surface=Surface(uv);
    if(any(uv<0)||any(uv>1)||surface.g<=Hash(cell+83.1)||surface.b<Shape.w||distance>Band.z||distance<Band.y) {
        o.position=float4(2,2,2,1);return o;
    }
    float fade=1-smoothstep(Band.z-Band.w,Band.z,distance);
    if(Band.y>0)fade*=smoothstep(Band.y,Band.y+Band.w,distance);
    float growth=sqrt(fade);
    float angle=i.blade.z+r*6.2831853;
    float sn,cs;sincos(angle,sn,cs);
    float2 radial=float2(cs,sn),side=float2(-sn,cs);
    float t=i.blade.y;
    float height=Shape.x*i.height*lerp(1,r2*.9+.55,Shape.z)*growth;
    float2 offset=float2(i.offset.x*cs-i.offset.y*sn,i.offset.x*sn+i.offset.y*cs)*Shape.x;
    float phase=dot(root,float2(.19,.13))+Wind.w*Extra.x*1.35;
    float sway=(sin(phase)*.65+sin(phase*1.7+r3*6.2831853)*.35)*Wind.z;
    float2 lean=radial*(.28+r3*.2)*height+Wind.xy*sway*height;
    float width=Shape.y*(1-t)*(.8+.4*sin(t*3.14159265))*growth*max(1,distance/18);
    float3 p=float3(root.x+offset.x,surface.r-.018,root.y+offset.y);
    p.xz+=lean*t*t+side*i.blade.x*width*.5;
    p.y+=height*t;
    float3 n=normalize(float3(radial.x,.35+t*.35,radial.y));
    float light=.35+.65*abs(dot(n,SunDirection.xyz));
    float3 tint=lerp(float3(.32,.55,.065),float3(.60,.48,.19),Look.z);
    tint*=lerp(.78,1.22,r3);
    float3 color=tint*(Ambient.rgb+SunColor.rgb*light);
    color*=lerp(1-Look.w*.7*saturate(1-distance/50),1.23,t);
    color*=.8+surface.a*.4;
    float luma=dot(color,float3(.2126,.7152,.0722));
    color=lerp(luma,color,Look.y)*Look.x;
    float fog=FogFactor(p);
    o.position=mul(float4(p,1),ViewProjection);
    o.color=float4(color,fog);
    o.leaf=float2(i.blade.x,t);
    return o;
}
float4 GrassBladePS(Output i):COLOR0 {
    float rib=.88+.12*(1-abs(i.leaf.x));
    return float4(lerp(FogColor.rgb,i.color.rgb*rib,i.color.a),1);
}
