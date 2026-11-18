// Instanced, tapered ribbons: real 3D silhouettes, pinned roots and world-space wind.
sampler2D Ground:register(s0);
sampler2D Contacts:register(s1);
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
float4 Extra:register(c15); // wind speed, cell anchor xz, snow grass
float4 Forward:register(c16);
float4 Lod:register(c18); // nested-grid factor, base spacing, total range, fade
float4 Center:register(c17); // player position; Eye remains the actual camera for fog
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
    float4 value=lerp(lerp(v0,v1,f.x),lerp(v2,v3,f.x),f.y);
    // Reject a footprint crossing a road/prop instead of interpolating a few
    // surviving blades over its silhouette. Never interpolate across levels.
    float coverage=min(min(abs(v0.g),abs(v1.g)),min(abs(v2.g),abs(v3.g)));
    float high=max(max(v0.r,v1.r),max(v2.r,v3.r));
    float low=min(min(v0.r,v1.r),min(v2.r,v3.r));
    value.g=coverage*(value.g<0?-1:1);
    if(high-low>max(.4,16*Map.z/Map.w))value.g=0;
    return value;
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
    // Every coarse root is an exact subset of the finer world grid. Camera
    // movement cannot reseed roots, and shared blades keep their planted bases.
    float2 cell=(i.cell+Extra.yz)*Lod.x;
    float r=Hash(cell),r2=Hash(cell+17.3),r3=Hash(cell+41.7);
    float2 patchCell=floor(cell*.25);
    float2 patchOffset=(float2(Hash(patchCell+101.7),Hash(patchCell+157.3))*2-1)*1.1;
    float2 root=(cell+patchOffset+float2(r,r2)*.8+.1)*Lod.y;
    float distance=length(root-Center.xz);
    float cameraDistance=length(root-Eye.xz);
    if(distance<Band.y||distance>=Band.z){o.position=float4(2,2,2,1);return o;}
    float leaf=Hash(cell+i.blade.w*13.71+9.4);
    float patch=Hash(floor(root*.16));
    float simple=i.offset.y;
    float angle=i.blade.z+r*6.2831853+(leaf-.5)*.7;
    float sn,cs;sincos(angle,sn,cs);
    float2 radial=float2(cs,sn),side=float2(-sn,cs);
    float2 offset=radial*i.offset.x*Shape.x;
    float2 planted=root+offset;
    float2 uv=float2((planted.x-Map.x)/(2*Map.z)+.5,.5-(planted.y-Map.y)/(2*Map.z));
    float4 surface=Surface(uv);
    float snow=surface.a<0||Extra.w>1.5?1:0;
    float coverage=smoothstep(.02,.18,abs(surface.g));
    float slope=smoothstep(max(0,Shape.w-.07),min(1,Shape.w+.04),surface.b);
    float contact=tex2Dlod(Contacts,float4(uv,0,0)).r;
    float obstacle=Center.y+(0.5-contact)*1024;
    bool water=contact>2;
    bool paving=contact>1&&!water;
    if(paving)obstacle=Center.y+(contact-1.5)*1024;
    bool submerged=water&&surface.r<=Center.y+(contact-2.5)*1024+.025;
    float clearance=obstacle-surface.r;
    bool solid=contact>0&&!water;
    bool blocked=solid&&(paving?clearance>=-.5&&clearance<=.85:clearance>=-.10&&clearance<=.14);
    if(any(uv<0)||any(uv>1)||coverage*slope<.001||(snow>0&&Extra.w<.5)||submerged||blocked){
        o.position=float4(2,2,2,1);return o;
    }
    float fade=1;
    // Fade only roots that do not survive in the next nested grid. Survivors
    // cross the LOD boundary at exactly the same location and full height.
    float2 parent=frac(cell/(Lod.x*2));
    bool survives=all(parent<.25)&&Band.z<Lod.z-.01;
    if(!survives)fade=1-smoothstep(Band.z-max(Band.w,Band.z*.35),Band.z,distance);
    float growth=sqrt(fade*coverage*slope);
    float t=i.blade.y;
    float height=Shape.x*i.height*lerp(1,.8+leaf*.55+patch*.15,Shape.z)*growth;
    // A raised canopy or stall leaves room to grow. Shorten the blades
    // underneath it instead of clearing the projection of the whole object.
    if(solid&&!paving&&clearance>.14)height=min(height,clearance-.04);
    float phase=dot(root,float2(.19,.13))+Wind.w*Extra.x*1.35;
    float sway=(sin(phase)*.65+sin(phase*1.7+r3*6.2831853)*.35)*Wind.z;
    float2 lean=(radial*(.18+leaf*.6)+Wind.xy*sway)*height;
    // A fuller curved leaf reads as a blade instead of a dark needle.
    float coverScale=1+smoothstep(min(15,Lod.z*.34)*.65,min(15,Lod.z*.34),distance)+
        2*smoothstep(min(42,Lod.z*.7)*.65,min(42,Lod.z*.7),distance);
    float width=Shape.y*1.15*(1-t)*(.9+.8*sin(t*3.14159265))*(.85+leaf*.3)*growth*
        sqrt(coverScale)*max(1,cameraDistance/40);
    float3 p=float3(root.x+offset.x,surface.r+.008,root.y+offset.y);
    float ridge=1-abs(i.blade.x);
    if(simple>.5){
        // The leaves open out above the root and curl more sharply in the
        // upper half. The middle rail raises the fold out of the leaf plane.
        float spread=.50+.91*sin(3.14159265*t);
        width=Shape.y*1.85*spread*(1-t)*(.8+leaf*.42)*growth*sqrt(coverScale)*max(1,cameraDistance/45);
        float curl=t*(.16+.84*t*t);
        p.xz+=radial*(height*(.31+leaf*.40)*curl+ridge*width*.33);
        p.xz+=Wind.xy*sway*height*t*t+side*(height*(leaf-.5)*.24*t*t+i.blade.x*width*.5);
        p.y-=height*(.02+leaf*.06)*t*t*t*t;
    }else{
        p.xz+=lean*t*t+side*((leaf-.5)*height*.16*t*t+i.blade.x*width*.5);
    }
    p.y+=height*t;
    // Soft, upward-biased leaf normals keep the native directional light while
    // avoiding near-black strips at ground level. Both faces transmit some light.
    float3 n=normalize(float3(radial.x*.25,1,radial.y*.25));
    float light=.45+.55*abs(dot(n,SunDirection.xyz));
    float3 base=lerp(float3(.16,.30,.055),float3(.25,.39,.07),leaf*.35+patch*.65);
    float3 tip=lerp(float3(.43,.60,.13),float3(.56,.68,.18),leaf*.4+patch*.6);
    float3 fresh=lerp(base,tip,smoothstep(.08,.95,t));
    if(simple>.5){
        float3 green=lerp(float3(.14,.29,.055),float3(.20,.40,.075),leaf);
        float3 lime=lerp(float3(.30,.52,.095),float3(.45,.62,.135),leaf);
        float3 tip=lerp(float3(.48,.65,.14),float3(.66,.75,.20),leaf);
        fresh=lerp(lerp(green,lime,smoothstep(.05,.64,t)),tip,smoothstep(.45,1,t));
        float face=.5+.5*dot(side,SunDirection.xz);
        float fold=lerp(lerp(.87,1.03,face),lerp(1.03,.87,face),i.blade.x*.5+.5);
        fresh*=lerp(fold,1.15,ridge*(1-.65*t));
    }
    float3 tint=lerp(fresh,float3(.58,.49,.20),Look.z);
    tint=lerp(tint,lerp(float3(.52,.58,.61),float3(.79,.83,.85),t),snow);
    tint*=lerp(.96,1.04,r3);
    float3 lighting=Ambient.rgb+SunColor.rgb*light;
    // Native ambient plus sun can exceed display white. Keep the painted
    // palette readable instead of clipping a whole field to yellow/white.
    lighting/=max(1,max(lighting.r,max(lighting.g,lighting.b)));
    float neutral=dot(lighting,float3(.2126,.7152,.0722));
    lighting=lerp(lighting,lerp(neutral,lighting,.25),snow);
    lighting=lerp(lighting,lerp(neutral,lighting,.6),simple*(1-snow));
    float3 color=tint*lighting;
    color*=lerp(1-Look.w*lerp(.22,.38,snow),lerp(1.04,1.02,snow),t);
    color*=lerp(.88+abs(surface.a)*.22,.94,snow);
    float luma=dot(color,float3(.2126,.7152,.0722));
    color=lerp(luma,color,lerp(Look.y,min(Look.y,1),snow))*Look.x;
    float fog=FogFactor(p);
    o.position=mul(float4(p,1),ViewProjection);
    o.color=float4(color,fog);
    o.leaf=float2(i.blade.x,t);
    return o;
}
float4 GrassBladePS(Output i):COLOR0 {
    float rib=.99+.01*(1-abs(i.leaf.x));
    return float4(lerp(FogColor.rgb,i.color.rgb*rib,i.color.a),1);
}
