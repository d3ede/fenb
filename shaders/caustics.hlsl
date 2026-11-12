sampler2D Depth : register(s0);
sampler2D Normals : register(s1);
float4 EyeTime : register(c0);
float4 Camera : register(c1);
float4 Viewport : register(c2);
row_major float4x4 View : register(c3);
float4 Waves : register(c7); // amplitude, length, speed, ripple strength
float4 Optics : register(c8); // ripple scale, caustic strength, focusing slab, dispersion
float4 LightDir : register(c9);
float4 LightColor : register(c10);

float3 WaterNormal(float2 world,float lod){
    float t=EyeTime.w*Waves.z;
    float2 p=world*Optics.x;
    float2 a=tex2Dlod(Normals,float4(p*.065+t*float2(.011,.007),0,lod)).rg*2-1;
    float2 rotated=float2(p.x*.8-p.y*.6,p.x*.6+p.y*.8);
    float2 b=tex2Dlod(Normals,float4(rotated*.109+t*float2(-.008,.009),0,lod)).rg*2-1;
    float2 ripple=a+float2(b.x*.8+b.y*.6,-b.x*.6+b.y*.8)*.5;
    float k=6.2831853/Waves.y;
    float u=dot(world,float2(.8,.6))*k+t;
    float v=dot(world,float2(-.4,.9165))*k*1.57-t*1.24;
    float2 slope=Waves.x*k*(cos(u)*float2(.8,.6)+.628*cos(v)*float2(-.4,.9165));
    return normalize(float3(-slope.x,1,-slope.y)+float3(ripple.x,0,ripple.y)*Waves.w*.12);
}
float2 RayShift(float3 incoming,float3 normal,float ior){
    float3 ray=refract(incoming,normal,rcp(ior));
    return ray.xz/max(.15,-ray.y);
}
float Focus(float3 incoming,float3 a,float3 b,float3 c,float ior,float epsilon){
    float2 center=RayShift(incoming,a,ior);
    float2 dx=(RayShift(incoming,b,ior)-center)*(Optics.z/epsilon);
    float2 dz=(RayShift(incoming,c,ior)-center)*(Optics.z/epsilon);
    float jacobian=(1+dx.x)*(1+dz.y)-dx.y*dz.x;
    return clamp(rcp(max(.22,abs(jacobian))),.25,3.5)-1;
}
float4 CausticFieldPS(float2 uv:TEXCOORD0):COLOR0 {
    float raw=tex2Dlod(Depth,float4(uv,0,0)).r;
    float z=Camera.w/min(-.000001,raw-Camera.z);
    float2 ndc=((uv-Viewport.xy)/Viewport.zw)*float2(2,-2)+float2(-1,1);
    float3 pointView=float3(ndc/Camera.xy,1)*z;
    float3 relative=mul((float3x3)View,pointView);
    float3 world=relative+EyeTime.xyz;
    float3 receiver=cross(ddx(relative),ddy(relative));
    receiver=normalize(receiver+float3(0,.00000001,0));
    if(dot(receiver,-relative)<0)receiver=-receiver;
    float2 footprint=max(abs(ddx(world.xz)),abs(ddy(world.xz)));
    float lod=clamp(log2(max(1,max(footprint.x,footprint.y)*Optics.x*128*.109)),0,7);
    float3 incoming=-normalize(LightDir.xyz);
    float3 first=WaterNormal(world.xz,lod);
    // One inverse-map iteration approximates where a refracted ray entered the surface.
    float2 q=world.xz-RayShift(incoming,first,1.333)*Optics.z;
    float epsilon=max(.025,.5/max(.25,Optics.x));
    float3 a=WaterNormal(q,lod),b=WaterNormal(q+float2(epsilon,0),lod),c=WaterNormal(q+float2(0,epsilon),lod);
    float3 focus=float3(Focus(incoming,a,b,c,1.333-.004*Optics.w,epsilon),
                        Focus(incoming,a,b,c,1.333,epsilon),
                        Focus(incoming,a,b,c,1.333+.007*Optics.w,epsilon));
    float3 ray=refract(incoming,a,1/1.333);
    float facing=saturate(dot(receiver,-ray));
    float valid=raw<.999999 && all(uv>=Viewport.xy) && all(uv<=Viewport.xy+Viewport.zw);
    float3 light=focus*LightColor.rgb*(Optics.y*facing*valid);
    // Signed multiplicative lighting, encoded around byte 128. No scene colors are modified here.
    return float4(saturate(128/255.0+light*.125),1);
}
