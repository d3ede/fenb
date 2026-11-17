// Pixel-only materials: preserve native fixed-function vertex processing and depth.
// Shader Model 2a uses the native vertex-converter interface.
sampler2D Art:register(s0);
sampler2D Layer:register(s1);
sampler2D Shadow:register(s2);
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
float4 MaterialPS(MaterialInput i):COLOR0 {
    const float3 luma=float3(.2126,.7152,.0722);
    float4 art=tex2D(Art,i.uv);
    float3 west=tex2D(Art,i.uv-float2(ArtInfo.x,0)).rgb;
    float3 east=tex2D(Art,i.uv+float2(ArtInfo.x,0)).rgb;
    float3 north=tex2D(Art,i.uv-float2(0,ArtInfo.y)).rgb;
    float3 south=tex2D(Art,i.uv+float2(0,ArtInfo.y)).rgb;
    float2 dx=ddx(i.uv),dy=ddy(i.uv);
    float fade=saturate(1-max(length(dx/ArtInfo.xy),length(dy/ArtInfo.xy))*.6);
    float2 gradient=float2(dot(east-west,luma),dot(south-north,luma))*Style.y*fade;
    gradient/=1+length(gradient)*6;
    float3 n=normalize(i.normal+float3(0,0,-.00000001));
    float3 tangent=ddx(i.view)*dy.y-ddy(i.view)*dx.y;
    tangent-=n*dot(n,tangent);tangent*=rsqrt(max(dot(tangent,tangent),1e-20));
    float determinant=dx.x*dy.y-dx.y*dy.x;
    tangent*=determinant<0?-1:1;
    float3 bitangent=cross(n,tangent)*(determinant<0?-1:1);
    float3 bumped=normalize(n-(tangent*gradient.x+bitangent*gradient.y));
    float delta=clamp(dot(bumped,SunDirection.xyz)-dot(n,SunDirection.xyz),-.025,.025);
    float4 diffuse=i.diffuse;
    diffuse.rgb=saturate(diffuse.rgb+delta*SunColor.rgb*Style.x*.35);
    if(ArtInfo.z<1.5){
        float brightness=dot(art.rgb,luma);
        // Preserve the source luminance pattern; lift its brightness without averaging it away.
        float3 clean=float3(.97,.99,1)*(brightness+.12*(1-brightness));
        art.rgb=lerp(art.rgb,clean,Style.w*Style.x);
    }else{
        float3 detail=art.rgb-(west+east+north+south)*.25;
        art.rgb=saturate(art.rgb+clamp(detail*Style.x*Style.y*fade*.12,-.008,.008));
    }
    float4 color=NativeStage(Stage0,Alpha0,art,diffuse,diffuse,Constant0);
    float4 layer=lerp(1,tex2D(Layer,i.blend),Alpha1.z);
    color=NativeStage(Stage1,Alpha1,layer,diffuse,color,Constant1);
    float4 extra=float4(i.extra.xy,0,Alpha2.w>3.5?i.extra.w:Alpha2.w>2.5?i.extra.z:1);
    float4 shadow=lerp(1,tex2Dproj(Shadow,extra),Alpha2.z);
    color=NativeStage(Stage2,Alpha2,shadow,diffuse,color,Constant2);
    float3 eye=normalize(-i.view+float3(0,0,-.00000001));
    float3 halfVector=normalize(SunDirection.xyz+eye);
    color.rgb+=pow(saturate(dot(bumped,halfVector)),64)*SunColor.rgb*Style.x*Style.z*ArtInfo.w;
    float fog=FogAmount(PixelFog.x<0?length(i.view):i.view.z,float4(abs(PixelFog.x),PixelFog.yzw));
    color.rgb=lerp(FogColor.rgb,color.rgb,fog);
    return color;
}
