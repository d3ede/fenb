// Pixel-only materials: preserve native fixed-function vertex processing and depth.
// Shader Model 2a uses the native vertex-converter interface.
sampler2D Art:register(s0);
sampler2D Layer:register(s1);
sampler2D Shadow:register(s2);
sampler2D SurfaceDetail:register(s3);
float4 Style : register(c0); // strength, relief, glints, snow appearance
float4 ArtInfo : register(c1); // texel xy, exact material kind, highlight scale
float4 Stage0 : register(c2);
float4 Alpha0 : register(c3);
float4 Stage1 : register(c4);
float4 Alpha1 : register(c5);
float4 Factor : register(c6);
float4 Constant0 : register(c7);
float4 Constant1 : register(c8);
float4 FogColor : register(c9);
float4 PixelFog : register(c10);
float4 SunDirection : register(c11);
float4 SunColor : register(c12);
float4 DetailInfo : register(c13); // detail UV scale, availability, source slope scale
float4 Stage2 : register(c14);
float4 Alpha2 : register(c15);
float4 Constant2 : register(c16);
float FogAmount(float depth, float4 settings) {
    if (settings.x < .5) return 1;
    if (settings.x < 1.5) return saturate(exp(-depth*settings.y));
    if (settings.x < 2.5) return saturate(exp(-depth*depth*settings.y*settings.y));
    return saturate((settings.w-depth)/max(.0001,settings.w-settings.z));
}
struct MaterialInput {float4 diffuse:COLOR0;float2 uv:TEXCOORD0;float2 blend:TEXCOORD1;float4 extra:TEXCOORD2;float3 view:TEXCOORD3;float3 normal:TEXCOORD4;};
float4 Argument(float a,float4 texel,float4 diffuse,float4 current,float4 constant) {
    float k=fmod(a,16);
    float4 v=k==0?diffuse:k==1?current:k==2?texel:k==3?Factor:constant;
    if(a>=32)v=v.aaaa;
    if(fmod(floor(a/16),2)>0)v=1-v;
    return v;
}
float4 Operation(float op,float4 a,float4 b) {
    if(op<2.5)return a;
    if(op<3.5)return b;
    if(op>6.5)return a+b;
    return a*b*(op>5.5?4:op>4.5?2:1);
}
float4 NativeStage(float4 stage,float4 alpha,float4 art,float4 diffuse,float4 current,float4 constant) {
    if(stage.x<1.5)return current;
    float4 a=Argument(stage.y,art,diffuse,current,constant),b=Argument(stage.z,art,diffuse,current,constant);
    float3 rgb=Operation(stage.x,a,b).rgb;
    float opacity=current.a;
    if(stage.w>1.5)opacity=Operation(stage.w,Argument(alpha.x,art,diffuse,current,constant),Argument(alpha.y,art,diffuse,current,constant)).a;
    return saturate(float4(rgb,opacity));
}
float3 PaintedSnow(float3 art) {
    float brightness=dot(art,float3(.2126,.7152,.0722));
    // Only bright, neutral/cool painted snow in exact winter atlases is eligible.
    // Green needles and warm bark retain their original colors and silhouettes.
    float mask=saturate((brightness-.5)*4)*
        (1-saturate((art.g-max(art.r,art.b))*8))*saturate((art.b-art.r+.1)*8);
    return lerp(art,float3(.965,.985,1)*(.65+brightness*.35),mask*Style.w*Style.x);
}
float4 ShadeMaterial(MaterialInput i,uniform bool smoothBark) {
    const float3 luma=float3(.2126,.7152,.0722);
    float4 art=tex2D(Art,i.uv);
    float2 dx=ddx(i.uv),dy=ddy(i.uv);
    float footprint=max(length(dx/ArtInfo.xy),length(dy/ArtInfo.xy));
    float fade=saturate(1-footprint*.6);
    float bias=max(0,log2(2/max(footprint,.0001)));
    // A wider, orthogonally smoothed gradient avoids embossing individual bark texels.
    float2 radius=ArtInfo.xy*(ArtInfo.z>5.5?1.5:1);
    float3 nw=smoothBark?tex2Dbias(SurfaceDetail,float4(i.uv-radius,0,bias)).rgb:tex2D(Art,i.uv-radius).rgb;
    float3 ne=smoothBark?tex2Dbias(SurfaceDetail,float4(i.uv+float2(radius.x,-radius.y),0,bias)).rgb:tex2D(Art,i.uv+float2(radius.x,-radius.y)).rgb;
    float3 sw=smoothBark?tex2Dbias(SurfaceDetail,float4(i.uv+float2(-radius.x,radius.y),0,bias)).rgb:tex2D(Art,i.uv+float2(-radius.x,radius.y)).rgb;
    float3 se=smoothBark?tex2Dbias(SurfaceDetail,float4(i.uv+radius,0,bias)).rgb:tex2D(Art,i.uv+radius).rgb;
    float snow=smoothBark?0:ArtInfo.z<1.5?Style.w:0;
    // Within the known bark atlas, preserve its painted white snow skirt.
    float bark=ArtInfo.z>5.5?saturate((art.r-art.b)*8):0;
    float structure=saturate(ArtInfo.z-1)*saturate(4-ArtInfo.z);
    float2 detailUV=i.uv*DetailInfo.xy;
    float detailFade=DetailInfo.z*saturate(1-max(length(ddx(detailUV)*128),length(ddy(detailUV)*128))*.65);
    float4 micro=tex2D(SurfaceDetail,detailUV);
    float2 grain=(micro.rg*2-1)*detailFade;
    float2 gradient=float2(dot(ne+se-nw-sw,luma),dot(sw+se-nw-ne,luma))*.5*DetailInfo.w*fade;
    gradient.y*=1-bark*.65;
    gradient/=1+length(gradient)*(3-bark*1.5);
    // Painted snow mottling is color variation, not a field of raised bubbles.
    gradient=gradient*(1-snow)+grain*snow*.08;
    gradient*=Style.y;
    float3 n=normalize(i.normal+float3(0,0,-.0001));
    float3 tangent=ddx(i.view)*dy.y-ddy(i.view)*dx.y;
    tangent-=n*dot(n,tangent);tangent*=rsqrt(max(dot(tangent,tangent),1e-20));
    float determinant=dx.x*dy.y-dx.y*dy.x;
    tangent*=determinant<0?-1:1;
    float3 bitangent=cross(n,tangent)*(determinant<0?-1:1);
    float3 bumped=normalize(n-(tangent*gradient.x+bitangent*gradient.y));
    float limit=.025+snow*.05+bark*.25+structure*.1;
    float delta=clamp(dot(bumped,SunDirection.xyz)-dot(n,SunDirection.xyz),-limit,limit);
    float4 diffuse=i.diffuse;
    diffuse.rgb=saturate(diffuse.rgb+delta*SunColor.rgb*Style.x*(.35+snow*.4+bark+structure*.65));
    if(ArtInfo.z<1.5){
        float brightness=dot(art.rgb,luma);
        // Keep broad painted variation, but compress mottling rather than embossing it.
        float3 clean=float3(.965,.985,1)*(.65+brightness*.35);
        art.rgb=lerp(art.rgb,clean,Style.w*Style.x);
        art.rgb=saturate(art.rgb+(micro.b-.5)*detailFade*Style.w*Style.x*.02);
    }else{
        float3 detail=art.rgb-(nw+ne+sw+se)*.25;
        art.rgb=saturate(art.rgb+clamp(detail*Style.x*Style.y*fade*.1,-.025,.025));
        if(ArtInfo.z>5.5)art.rgb=PaintedSnow(art.rgb);
    }
    float4 color=NativeStage(Stage0,Alpha0,art,diffuse,diffuse,Constant0);
    float4 layer=lerp(1,tex2D(Layer,i.blend),Alpha1.z);
    color=NativeStage(Stage1,Alpha1,layer,diffuse,color,Constant1);
    float4 extra=float4(i.extra.xy,0,Alpha2.w>3.5?i.extra.w:Alpha2.w>2.5?i.extra.z:1);
    float4 shadow=lerp(1,tex2Dproj(Shadow,extra),Alpha2.z);
    color=NativeStage(Stage2,Alpha2,shadow,diffuse,color,Constant2);
    float3 eye=normalize(-i.view+float3(0,0,-.0001));
    float3 halfVector=normalize(SunDirection.xyz+eye);
    color.rgb+=pow(saturate(dot(bumped,halfVector)),64)*SunColor.rgb*Style.x*Style.z*ArtInfo.w;
    float crystals=saturate((micro.a-.75)*8)*detailFade*snow;
    color.rgb+=pow(saturate(dot(bumped,halfVector)),64)*crystals*SunColor.rgb*Style.x*Style.z*.035;
    float fog=FogAmount(PixelFog.x<0?length(i.view):i.view.z,float4(abs(PixelFog.x),PixelFog.yzw));
    color.rgb=lerp(FogColor.rgb,color.rgb,fog);
    return color;
}
float4 MaterialPS(MaterialInput i):COLOR0 {return ShadeMaterial(i,false);}
float4 MaterialBarkPS(MaterialInput i):COLOR0 {return ShadeMaterial(i,true);}
// Thin foliage cards do not receive bark embossing, grain or broad highlights.
// Preserve the native alpha, all active texture stages, lighting and fog.
float4 CoatedMaterial(MaterialInput i,float4 art) {
    float4 color=NativeStage(Stage0,Alpha0,art,i.diffuse,i.diffuse,Constant0);
    float4 layer=lerp(1,tex2D(Layer,i.blend),Alpha1.z);
    color=NativeStage(Stage1,Alpha1,layer,i.diffuse,color,Constant1);
    float4 extra=float4(i.extra.xy,0,Alpha2.w>3.5?i.extra.w:Alpha2.w>2.5?i.extra.z:1);
    float4 shadow=lerp(1,tex2Dproj(Shadow,extra),Alpha2.z);
    color=NativeStage(Stage2,Alpha2,shadow,i.diffuse,color,Constant2);
    float fog=FogAmount(PixelFog.x<0?length(i.view):i.view.z,float4(abs(PixelFog.x),PixelFog.yzw));
    color.rgb=lerp(FogColor.rgb,color.rgb,fog);
    return color;
}
float4 MaterialSnowPS(MaterialInput i):COLOR0 {
    float4 art=tex2D(Art,i.uv);art.rgb=PaintedSnow(art.rgb);
    return CoatedMaterial(i,art);
}
float4 MaterialWinterPS(MaterialInput i):COLOR0 {
    float4 art=tex2D(Art,i.uv);
    const float3 luma=float3(.2126,.7152,.0722);
    float light=dot(art.rgb,luma);
    float4 camera=float4(i.view,1);
    // This entry uses these registers for the inverse camera transform.
    float3 world=float3(dot(camera,SunColor),dot(camera,SunDirection),dot(camera,DetailInfo));
    bool floor=ArtInfo.z<2.5;
    float2 surface=world.xz;
    if(!floor)surface+=world.y*float2(.43,-.27);
    float2 a=ArtInfo.w<0?i.uv*(-ArtInfo.w):(surface+surface.yx*float2(.5,-.5))*ArtInfo.w;
    float2 b=(a+a.yx*float2(-.67,.67))*2.7+.28;
    // Different orientations and scales conceal the period of the small noise source.
    float flecks=tex2D(SurfaceDetail,a.yx*9.7+.36).r;
    float field=lerp(tex2D(SurfaceDetail,a).b,tex2D(SurfaceDetail,b).a,.36)+(flecks-.5)*.13;
    float3 up=SunDirection.xyz;
    float facing=dot(i.normal,up)*rsqrt(max(dot(i.normal,i.normal),1e-8));
    float frost=Style.z+Style.w*smoothstep(Style.x,Style.y,field);
    // Stage 0 uses only Alpha0.xy for its native alpha arguments.
    if(Alpha0.z>.5){
        // Vertical wood and masonry get a dusting. Roof slopes and ledges carry snow.
        frost*=.09+.82*smoothstep(ArtInfo.x,ArtInfo.y,facing);
    }else if(Alpha0.w>.5){
        // Leaf cards keep recognizable green shadows through their snow.
        frost*=.65+.35*smoothstep(.19,.55,light);
    }
    float3 frozen=lerp(art.rgb,light*float3(1.018,1.014,.94),FogColor.a);
    float3 snow=float3(.940,.956,.967)*(.78+.16*light+.095*flecks);
    art.rgb=lerp(frozen,snow,frost);
    return CoatedMaterial(i,art);
}
