sampler2D Art : register(s0);
sampler2D Background : register(s1);
row_major float4x4 Clip : register(c0);
row_major float4x4 TextureMatrix : register(c4);
row_major float4x4 WorldView : register(c8);
float4 Layout : register(c12);
struct Input {float3 position:POSITION;float4 color:COLOR0;float2 uv:TEXCOORD0;};
struct Output {float4 position:POSITION;float4 color:COLOR0;float2 uv:TEXCOORD0;float depth:TEXCOORD1;};
Output SfxVS(Input input) {
 Output o;o.position=mul(float4(input.position,1),Clip);
 o.uv=Layout.y>.5?mul(float4(input.uv,1,0),TextureMatrix).xy:input.uv;
 o.color=Layout.x>.5?input.color:1;
 float3 p=mul(float4(input.position,1),WorldView).xyz;
 o.depth=Layout.z>.5?length(p):p.z;return o;
}
float4 Style : register(c0); // gain, local glow, chroma, edge softness
float4 ColorStage : register(c1);
float4 AlphaStage : register(c2);
float4 Factor : register(c3);
float4 Constant : register(c4);
float4 Bounds : register(c5);
float4 Texel : register(c6); // texel xy, kind, crowding/age attenuation
float4 Screen : register(c7); // inverse viewport, snapshot valid, wake strength / curtain time
float4 FogColor : register(c8);
float4 Fog : register(c9);
float4 FogExtra : register(c10);
float4 Argument(float argument,float4 textureValue,float4 diffuse) {
 float base=fmod(argument,16);
 float4 value=base==2?textureValue:base==3?Factor:base==6?Constant:diffuse;
 if(argument>=32)value=value.aaaa;
 if(fmod(floor(argument/16),2)>0)value=1-value;
 return value;
}
float4 Operation(float op,float4 a,float4 b) {
 if(op<2.5)return a;if(op<3.5)return b;if(op>6.5)return a+b;
 return a*b*(op>5.5?4:op>4.5?2:1);
}
float4 Native(float4 texel,float4 diffuse) {
 float3 rgb=Operation(ColorStage.x,Argument(ColorStage.y,texel,diffuse),Argument(ColorStage.z,texel,diffuse)).rgb;
 float alpha=ColorStage.w<1.5?diffuse.a:Operation(ColorStage.w,Argument(AlphaStage.x,texel,diffuse),Argument(AlphaStage.y,texel,diffuse)).a;
 return saturate(float4(rgb,alpha));
}
// Nine-tap Catmull-Rom reconstruction is confined to magnification and the current atlas cell.
float4 ReconstructArt(float2 uv,float2 low,float2 high) {
 float2 p=uv/Texel.xy-.5,base=floor(p),f=frac(p);
 float2 w0=f*(-.5+f*(1-.5*f)),w1=1+f*f*(-2.5+1.5*f);
 float2 w2=f*(.5+f*(2-1.5*f)),w3=f*f*(-.5+.5*f),w12=w1+w2;
 float3 x=float3(base.x-.5,base.x+.5+w2.x/w12.x,base.x+2.5)*Texel.x;
 float3 y=float3(base.y-.5,base.y+.5+w2.y/w12.y,base.y+2.5)*Texel.y;
 float3 wx=float3(w0.x,w12.x,w3.x),wy=float3(w0.y,w12.y,w3.y);
 float4 sum=0;
 [unroll]for(int j=0;j<3;++j)[unroll]for(int i=0;i<3;++i)
  sum+=tex2Dlod(Art,float4(clamp(float2(x[i],y[j]),low,high),0,0))*wx[i]*wy[j];
 return saturate(sum);
}
float4 SfxPS(float2 uv:TEXCOORD0,float fogDepth:TEXCOORD1,float4 color:COLOR0,float2 pixel:VPOS):COLOR0 {
 float2 low=Bounds.xy+Texel.xy*.5,high=max(low,Bounds.zw-Texel.xy*.5);
 float2 materialUV=uv;
 bool curtain=Texel.z==5;
 bool styled=Style.x!=1 || Style.y>0 || Style.z!=1 || Style.w>0;
 if(curtain && Style.y>0) {
  float flow=sin(uv.y*4.4+Screen.w*.13+sin(uv.x*8));
  materialUV=clamp(uv+float2(flow*.012,sin(uv.x*5+Screen.w*.09)*.004)*Style.y,low,high);
 }
 float4 original=Native(tex2D(Art,materialUV),color);
 if(Texel.z==3 && Screen.z>.5) {
  float2 screenUV=(pixel+.5)*Screen.xy;
  float signal=dot(original.rgb,float3(.2126,.7152,.0722))*original.a;
  float2 gradient=float2(ddx(signal),ddy(signal));
  gradient/=max(.02,length(gradient));
  float mask=saturate(signal*Texel.w*Screen.w);
  float3 surface=tex2D(Background,screenUV+gradient*Screen.xy*(4.5*Screen.w*Texel.w)).rgb;
  return float4(surface,mask);
 }
 float2 uvDx=ddx(uv),uvDy=ddy(uv);
 float footprint=max(length(uvDx/Texel.xy),length(uvDy/Texel.xy));
 float safeUV=all(materialUV>=Bounds.xy)&&all(materialUV<=Bounds.zw);
 [branch]if(styled) {
  // Detailed quality improves the actual material sampling; distant particles keep mip filtering.
  [branch]if(AlphaStage.w>.5 && footprint<1.2 && safeUV>.5)
   original=lerp(original,Native(ReconstructArt(materialUV,low,high),color),Texel.w*.65);
  float2 offset=Texel.xy*1.5;
  float4 left=Native(tex2Dgrad(Art,clamp(materialUV-float2(offset.x,0),low,high),uvDx,uvDy),color);
  float4 right=Native(tex2Dgrad(Art,clamp(materialUV+float2(offset.x,0),low,high),uvDx,uvDy),color);
  float4 up=Native(tex2Dgrad(Art,clamp(materialUV-float2(0,offset.y),low,high),uvDx,uvDy),color);
  float4 down=Native(tex2Dgrad(Art,clamp(materialUV+float2(0,offset.y),low,high),uvDx,uvDy),color);
  float3 average=(left.rgb+right.rgb+up.rgb+down.rgb)*.25;
  float smoke=Texel.z==2,peak=max(original.r,max(original.g,original.b));
  float soft=Style.w*(smoke?.18:.08)*(1-smoothstep(.08,.35,peak))*safeUV;
  float3 filtered=lerp(original.rgb,average,soft);
  // Local detail contrast separates rune strokes from their haze without changing vertex fading.
  float sharp=(1-smoke)*AlphaStage.w*.08*saturate(Style.x)*safeUV*saturate(2-footprint);
  filtered=max(0,filtered+(original.rgb-average)*sharp);
  float luminance=dot(filtered,float3(.2126,.7152,.0722));
  float3 rgb=max(0,lerp(luminance.xxx,filtered,Style.z))*Style.x;
  rgb+=average*(Style.y*(smoke?.06:.16))*pow(saturate(1-peak),2)*safeUV;
  if(curtain) {
   float tint=.5+.5*sin(uv.y*4+Screen.w*.05);
   float3 palette=lerp(float3(.35,.65,1),float3(.26,1,.68),tint);
   float filaments=pow(saturate(.5+.5*sin(uv.x*120+sin(uv.y*5-Screen.w*.1))),6);
   rgb=lerp(rgb,max(rgb.r,max(rgb.g,rgb.b))*palette,saturate(Style.y*.9));
   rgb+=peak*palette*filaments*(Style.y*.45);
  }
  // Preserve the native additive energy and bright cores. Bound only shader overshoot,
  // never replace the game's blend with Screen or remap every faint layer through a shoulder.
  rgb/=max(1,max(rgb.r,max(rgb.g,rgb.b)));
  original.rgb=lerp(original.rgb,rgb,Texel.w);
 }
 float visibility=1;
 if(Fog.x>.5) {
  if(Fog.y>2.5)visibility=saturate((Fog.w-fogDepth)/max(.001,Fog.w-Fog.z));
  else if(Fog.y>1.5)visibility=exp(-pow(FogExtra.x*fogDepth,2));
  else visibility=exp(-FogExtra.x*fogDepth);
 }
 original.rgb=lerp(FogColor.rgb,original.rgb,visibility);
 return original;
}
// Equipment refinement, pet beads and elemental weapon auras have a dedicated, cheaper
// shader. Native alpha, material animation and additive blending remain unchanged.
float4 AuraPS(float2 uv:TEXCOORD0,float fogDepth:TEXCOORD1,float4 color:COLOR0):COLOR0 {
 float4 original=Native(tex2D(Art,uv),color);
 if(Style.x!=1 || Style.y>0 || Style.z!=1 || Style.w>0 || FogExtra.y>0) {
 if(AlphaStage.w>.5 && max(length(ddx(uv)/Texel.xy),length(ddy(uv)/Texel.xy))<1.2)
  original=lerp(original,Native(ReconstructArt(uv,Bounds.xy+Texel.xy*.5,Bounds.zw-Texel.xy*.5),color),Texel.w*.65);
 float peak=max(original.r,max(original.g,original.b));
 float3 hue=saturate(original.rgb/max(.001,peak));
 float3 rich=hue*lerp(1,hue,saturate((Style.z-1)*2+.2*FogExtra.y));
 float energy=lerp(peak,sqrt(max(0,peak)),saturate(FogExtra.y)*.45);
 float gain=max(1,Style.x)*(1+FogExtra.y*.65);
 float3 rgb=rich*energy*gain;
 // Keep a colored core and a restrained halo inside the native aura geometry.
 if(Style.y>0) {
  float2 delta=Texel.xy*1.5,dx=ddx(uv),dy=ddy(uv);
  float3 sum=Native(tex2Dgrad(Art,uv+float2(delta.x,0),dx,dy),color).rgb;
  sum+=Native(tex2Dgrad(Art,uv-float2(delta.x,0),dx,dy),color).rgb;
  sum+=Native(tex2Dgrad(Art,uv+float2(0,delta.y),dx,dy),color).rgb;
  sum+=Native(tex2Dgrad(Art,uv-float2(0,delta.y),dx,dy),color).rgb;
  rgb+=sum*(Style.y*.08)*(1-saturate(peak));
 }
 original.rgb=lerp(original.rgb,rgb,Texel.w);
 }
 float visibility=1;
 if(Fog.x>.5) {
  if(Fog.y>2.5)visibility=saturate((Fog.w-fogDepth)/max(.001,Fog.w-Fog.z));
  else if(Fog.y>1.5)visibility=exp(-pow(FogExtra.x*fogDepth,2));
  else visibility=exp(-FogExtra.x*fogDepth);
 }
 original.rgb=lerp(FogColor.rgb,original.rgb,visibility);
 return original;
}
