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
float4 Screen : register(c7); // inverse viewport, snapshot valid, wake strength
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
float4 SfxPS(float2 uv:TEXCOORD0,float fogDepth:TEXCOORD1,float4 color:COLOR0,float2 pixel:VPOS):COLOR0 {
 float4 original=Native(tex2D(Art,uv),color);
 float2 offset=Texel.xy*1.5;
 float2 low=Bounds.xy+Texel.xy*.5,high=max(low,Bounds.zw-Texel.xy*.5);
 float4 left=Native(tex2D(Art,clamp(uv-float2(offset.x,0),low,high)),color);
 float4 right=Native(tex2D(Art,clamp(uv+float2(offset.x,0),low,high)),color);
 float4 up=Native(tex2D(Art,clamp(uv-float2(0,offset.y),low,high)),color);
 float4 down=Native(tex2D(Art,clamp(uv+float2(0,offset.y),low,high)),color);
 float4 average=(left+right+up+down)*.25;
 float4 result=original;
 if(Texel.z>2.5 && Screen.z>.5) {
  float2 screenUV=(pixel+.5)*Screen.xy;
  float signal=dot(original.rgb,float3(.2126,.7152,.0722))*original.a;
  float2 gradient=float2(ddx(signal),ddy(signal));
  gradient/=max(.02,length(gradient));
  float mask=saturate(signal*Texel.w*Screen.w);
  float3 surface=tex2D(Background,screenUV+gradient*Screen.xy*(4.5*Screen.w*Texel.w)).rgb;
  float light=dot(surface,float3(.2126,.7152,.0722));
  result=float4(surface+float3(.75,.88,1)*(.03+.06*light),mask);
  return result;
 }
 float safeUV=all(uv>=Bounds.xy)&&all(uv<=Bounds.zw);
 float smoke=Texel.z>1.5;
 float4 softened=lerp(original,average,Style.w*(smoke?.3:.10)*safeUV);
 float luminance=dot(softened.rgb,float3(.2126,.7152,.0722));
 float3 rgb=max(0,lerp(luminance.xxx,softened.rgb,Style.z))*Style.x;
 rgb+=average.rgb*(Style.y*(smoke?.08:.45))*safeUV;
 float compression=saturate(abs(Style.x-1)+Style.y+abs(Style.z-1)+Style.w)*.18;
 rgb/=1+max(0,max(rgb.r,max(rgb.g,rgb.b))-.85)*compression;
 result.rgb=lerp(original.rgb,rgb,Texel.w);
 result.a=lerp(original.a,softened.a,Texel.w);
 if(smoke<.5)result.a=max(result.a,average.a*Style.y*.25*Texel.w*safeUV);
 float visibility=1;
 if(Fog.x>.5) {
  if(Fog.y>2.5)visibility=saturate((Fog.w-fogDepth)/max(.001,Fog.w-Fog.z));
  else if(Fog.y>1.5)visibility=exp(-pow(FogExtra.x*fogDepth,2));
  else visibility=exp(-FogExtra.x*fogDepth);
 }
 result.rgb=lerp(FogColor.rgb,result.rgb,visibility);
 return result;
}
