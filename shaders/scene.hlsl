sampler2D Image : register(s0);
sampler2D Depth : register(s1);
sampler2D AO : register(s2);
sampler2D Rays : register(s3);
sampler2D LocalField : register(s4);
sampler2D SkyBackdrop : register(s5);
float4 Camera : register(c0);
float4 Viewport : register(c1);
float4 Texel : register(c2);
float4 Occlusion : register(c3); // strength, radius, bias, far fade
float4 FogParameters : register(c4); // density, start, height, falloff
float4 FogTint : register(c5);
float4 Lens : register(c6); // focus, range, radius, near strength
float4 Enabled : register(c7); // AO, fog, DoF, rays
float4 SunPosition : register(c8); // screen xy, enabled, rays strength
float4 RayParameters : register(c9); // length, decay, samples
float4 SunColor : register(c10);
float4 Eye : register(c11);
row_major float4x4 View : register(c12);
float4 Illumination : register(c16);
float4 Resolution : register(c17);
float4 Quality : register(c18);
float4 Local : register(c19); // count, strength, radius scale
float4 SourceLife[8] : register(c44);
float4 SourceToEye : register(c52);
float4 FogBackdrop : register(c53);
float4 NativeFog : register(c54);
float4 LightPositions[8] : register(c20);
float4 LightColors[8] : register(c28);
float4 LightAttenuation[8] : register(c36);

float RawDepth(float2 uv) {return tex2Dlod(Depth,float4(uv,0,0)).r;}
float Z(float2 uv) {return min(10000,Camera.w/min(-.000001,RawDepth(uv)-Camera.z));}
float3 Point(float2 uv,float z) {
    float2 ndc=(uv-Viewport.xy)/Viewport.zw*float2(2,-2)+float2(-1,1);
    return float3(ndc/Camera.xy,1)*z;
}
float3 SurfaceNormal(float2 uv,float3 p) {
    // Pick the same surface on each axis; derivatives across silhouettes invent slanted receivers.
    float2 dx=float2(Texel.x,0),dy=float2(0,Texel.y);
    float3 l=Point(uv-dx,Z(uv-dx)),r=Point(uv+dx,Z(uv+dx));
    float3 u=Point(uv-dy,Z(uv-dy)),d=Point(uv+dy,Z(uv+dy));
    float3 x=abs(l.z-p.z)<abs(r.z-p.z)?p-l:r-p;
    float3 y=abs(u.z-p.z)<abs(d.z-p.z)?p-u:d-p;
    float3 n=normalize(cross(x,y)+float3(0,0,.000001));
    return dot(n,-p)>0?n:-n;
}
float2 Kernel(int i) {
    float a=i*2.39996323;
    return float2(cos(a),sin(a));
}
float4 SceneAO(float2 uv:TEXCOORD0):COLOR0 {
    float raw=RawDepth(uv),z=Z(uv);
    float3 p=Point(uv,z);
    float occlusion=0;
    float2 radius=min(.12,Occlusion.y*Camera.xy/max(.1,z)*.5)*Viewport.zw;
    [loop]for(int i=0;i<16;++i){
        if(i>=Quality.x)break;
        float2 q=uv+Kernel(i)*radius*(.2+.8*(i+.5)/Quality.x);
        float3 delta=Point(q,Z(q))-p;
        float distance=length(delta);
        float horizon=max(0,dot(SurfaceNormal(uv,p),delta)-Occlusion.z)/max(.01,distance);
        float valid=all(q>=Viewport.xy)&&all(q<=Viewport.xy+Viewport.zw)&&RawDepth(q)<.999999;
        occlusion+=horizon*saturate(1-distance/Occlusion.y)*valid;
    }
    float fade=saturate(1-z/Occlusion.w)*(raw<.999999);
    return float4((1-saturate(occlusion*(4*Occlusion.x/Quality.x)*fade)).xxx,1);
}
float4 SceneRays(float2 uv:TEXCOORD0):COLOR0 {
    float2 delta=(SunPosition.xy-uv)*RayParameters.x/24;
    float sum=0,weight=1;
    [loop]for(int i=0;i<24;++i){
        float2 q=uv+delta*i;
        float2 fromSun=(q-SunPosition.xy)*float2(Texel.y/Texel.x,1);
        float source=exp(-dot(fromSun,fromSun)*160);
        float valid=all(q>=Viewport.xy)&&all(q<=Viewport.xy+Viewport.zw);
        sum+=source*(RawDepth(q)>.999999)*weight*valid;
        weight*=RayParameters.y;
    }
    return float4(SunColor.rgb*sum*(SunPosition.w*.12*SunPosition.z),1);
}
float BilateralAO(float2 uv,float z) {
    float sum=0,weights=0;
    [unroll]for(int i=0;i<4;++i){
        float2 q=uv+float2(frac(i*.5)*2-.5,floor(i*.5)-.5)*Resolution.xy;
        float w=rcp(1+abs(Z(q)-z)*4);
        sum+=tex2Dlod(AO,float4(q,0,0)).r*w;weights+=w;
    }
    return sum/max(.001,weights);
}
float3 BilateralLight(float2 uv,float z){
    float3 sum=0;float weights=0;
    float2 cell=uv/Resolution.zw-.5,base=floor(cell),f=frac(cell);
    [unroll]for(int i=0;i<4;++i){
        float2 corner=float2(fmod(i,2),floor(i*.5));
        float2 q=(base+corner+.5)*Resolution.zw;
        float2 bilinear=lerp(1-f,f,corner);
        float w=bilinear.x*bilinear.y*exp(-abs(Z(q)-z)*8);
        sum+=tex2Dlod(LocalField,float4(q,0,0)).rgb*w;weights+=w;
    }
    return sum/max(.001,weights);
}
float4 SceneComposite(float2 uv:TEXCOORD0):COLOR0 {
    float4 source=tex2D(Image,uv);
    float raw=RawDepth(uv),z=Z(uv);
    float3 p=Point(uv,z);
    if(any(uv<Viewport.xy)||any(uv>Viewport.xy+Viewport.zw))return source;
    float3 rgb=pow(max(0,source.rgb),2.2);
    // Input already includes the game's night lighting. Bounded reflectance recovery lets an
    // additional lamp illuminate a dark receiver without adding an unmasked screen-space glow.
    float ambientLevel=max(.12,dot(Illumination.rgb,float3(.2126,.7152,.0722)));
    float3 reflectance=min(.8,rgb/ambientLevel);
    if(Enabled.z>.5){
        float coc=saturate((abs(z-Lens.x)-Lens.y)/max(1,Lens.y))*(z<Lens.x?Lens.w:1);
        float3 sum=rgb;float weights=1;
        [loop]for(int i=0;i<12;++i){
            float2 q=uv+Kernel(i)*sqrt((i+.5)/12)*Texel.xy*(Lens.z*coc);
            float sampleZ=Z(q);
            // Prevent a blurred background from smearing sharp foreground silhouettes.
            float w=sampleZ>=z-max(1,z*.03);
            sum+=pow(max(0,tex2Dlod(Image,float4(q,0,0)).rgb),2.2)*w;weights+=w;
        }
        rgb=sum/weights;
    }
    if(Enabled.x>.5 && raw<.999999)rgb*=BilateralAO(uv,z);
    if(Local.w>.5 && raw<.999999)rgb+=reflectance*BilateralLight(uv,z);
    if(Enabled.y>.5 && raw<.999999){
        float3 worldDelta=mul((float3x3)View,p);
        float density=FogParameters.x*exp(clamp(-(Eye.y+worldDelta.y*.5-FogParameters.z)*FogParameters.w,-8,8));
        float fog=1-exp(-max(0,length(p)-FogParameters.y)*density);
        float3 inscatter=pow(max(0,FogTint.rgb),2.2)*Illumination.rgb;
        if(FogBackdrop.x>.5){
            // Converge to the actual sky behind this pixel, not a differently colored flat wall.
            inscatter=pow(max(0,tex2D(SkyBackdrop,uv).rgb),2.2);
            // Fade the engine's already-flat native fog silhouettes into that same backdrop.
            if(FogBackdrop.y>.5 && FogBackdrop.w>FogBackdrop.z)
                fog=max(fog,smoothstep(FogBackdrop.z,FogBackdrop.w,NativeFog.w>.5?length(p):p.z));
            inscatter*=lerp(FogTint.rgb/float3(.56,.66,.76),1,saturate(fog));
        }
        rgb=lerp(rgb,inscatter,saturate(fog));
    }
    if(Enabled.w>.5)rgb+=tex2D(Rays,uv).rgb;
    return float4(pow(saturate(rgb),1/2.2),source.a);
}
// Exposure reduction stores mean log2 luminance, mapped from [-12,0] to [0,1].
float4 Luminance(float2 uv:TEXCOORD0):COLOR0 {
    float sum=0;
    [unroll]for(int i=0;i<4;++i){
        float2 q=uv+float2(frac(i*.5)*2-.5,floor(i*.5)-.5)*Camera.xy;
        float l=dot(pow(max(0,tex2D(Image,q).rgb),2.2),float3(.2126,.7152,.0722));
        sum+=saturate((log2(max(.00024414,l))+12)/12);
    }
    return float4((sum*.25).xxx,1);
}
float4 Reduce(float2 uv:TEXCOORD0):COLOR0 {
    float sum=0;
    [unroll]for(int y=0;y<4;++y)[unroll]for(int x=0;x<4;++x)
        sum+=tex2D(Image,uv+float2(x-1.5,y-1.5)*Camera.xy).r;
    return float4((sum/16).xxx,1);
}
float4 Adapt(float2 uv:TEXCOORD0):COLOR0 {
    float average=tex2D(Image,float2(.5,.5)).r*12-12;
    float target=clamp(log2(Camera.x)-average,Camera.y,Camera.z);
    if(Camera.w<-1.5)return float4(target,0,0,1);
    if(Camera.w<0)return float4(0,0,0,1);
    float previous=tex2D(Depth,float2(.5,.5)).r;
    float speed=target<previous?Viewport.x:Viewport.y;
    float ev=lerp(previous,target,1-exp(-Camera.w*speed));
    return float4(ev,0,0,1);
}
row_major float4x4 SkyWorld : register(c0);
row_major float4x4 SkyView : register(c4);
row_major float4x4 SkyProjection : register(c8);
row_major float4x4 SkyUV : register(c12);
float4 SkyLayout : register(c16);
struct SkyInput{float3 position:POSITION;float4 color:COLOR0;float2 uv:TEXCOORD0;};
struct SkyOutput{float4 position:POSITION;float4 color:COLOR0;float2 uv:TEXCOORD0;float3 direction:TEXCOORD1;};
SkyOutput SkyVS(SkyInput input){
    SkyOutput o;float4 w=mul(float4(input.position,1),SkyWorld);
    o.position=mul(mul(w,SkyView),SkyProjection);o.direction=w.xyz;
    o.color=input.color;o.uv=SkyLayout.x>.5?mul(float4(input.uv,1,0),SkyUV).xy:input.uv;return o;
}
float4 SkyColorStage:register(c20);
float4 SkyAlphaStage:register(c21);
float4 SkyFactor:register(c22);
float4 SkyConstant:register(c23);
float4 SkyArgument(float argument,float4 textureValue,float4 diffuse){
    float base=fmod(argument,16);
    float4 value=base==2?textureValue:base==3?SkyFactor:base==6?SkyConstant:diffuse;
    if(argument>=32)value=value.aaaa;
    if(fmod(floor(argument/16),2)>0)value=1-value;
    return value;
}
float4 SkyOperation(float op,float4 a,float4 b){
    if(op<2.5)return a;
    if(op<3.5)return b;
    if(op>6.5)return a+b;
    return a*b*(op>5.5?4:op>4.5?2:1);
}
float4 SkyPS(float2 uv:TEXCOORD0,float3 direction:TEXCOORD1,float4 color:COLOR0):COLOR0 {
    float4 texel=tex2D(Image,uv);
    float3 nativeRGB=SkyOperation(SkyColorStage.x,SkyArgument(SkyColorStage.y,texel,color),SkyArgument(SkyColorStage.z,texel,color)).rgb;
    float nativeAlpha=SkyColorStage.w<1.5?color.a:SkyOperation(SkyColorStage.w,SkyArgument(SkyAlphaStage.x,texel,color),SkyArgument(SkyAlphaStage.y,texel,color)).a;
    float4 source=float4(nativeRGB,nativeAlpha);
    float3 d=normalize(direction);
    if(Viewport.x>.5){
        source.rgb=saturate((source.rgb-.5)*Camera.z+.5);
        return source;
    }
    float height=pow(saturate(d.y),Camera.y);
    // Preserve native clouds/stars and luminance. Object diffuse lighting is not sky radiance.
    float3 weights=float3(.2126,.7152,.0722);
    float luminance=dot(source.rgb,weights);
    float blue=smoothstep(.015,.18,source.b-min(source.r,source.g));
    float3 gradient=lerp(float3(.72,.82,.95),float3(.28,.50,.88),height);
    gradient*=luminance/max(.01,dot(gradient,weights));
    float3 rgb=lerp(source.rgb,gradient,Camera.x*.65*blue);
    rgb=lerp(rgb,luminance.xxx,pow(1-height,4)*Camera.x*.08*blue);
    float halo=pow(saturate(dot(d,normalize(Eye.xyz+float3(0,.000001,0)))),128)*Camera.w*Eye.w;
    rgb+=SunColor.rgb*halo;
    return float4(saturate(rgb),source.a);
}
float4 FlarePS(float2 uv:TEXCOORD0,float4 color:COLOR0):COLOR0 {
    float2 p=(uv-.5)*2;
    float radius=dot(p,p);
    float halo=exp(-radius*7)*pow(saturate(1-radius),2);
    float brightness=dot(color.rgb,float3(.2126,.7152,.0722));
    return float4(float3(1,.91,.76)*brightness,halo*color.a*Camera.x);
}
float4 EmitterFlags : register(c12);
SkyOutput EmitterVS(SkyInput input){
    SkyOutput o;float4 p=mul(float4(input.position,1),SkyWorld);
    // Each native draw owns a 32x32 tile, rasterized in material UV space, not camera space.
    // Reduction produces one source per draw regardless of projected size or camera heading.
    o.position=float4(input.uv*float2(2,-2)+float2(-1-1/32.0,1+1/32.0),0,1);
    o.direction=p.xyz;o.uv=input.uv;
    o.color=EmitterFlags.x>.5?input.color:1;return o;
}
struct Emission{float4 color:COLOR0;float4 position:COLOR1;};
float4 EmitterColorStage:register(c1);
float4 EmitterAlphaStage:register(c2);
float4 EmitterFactor:register(c3);
float4 EmitterConstant:register(c4);
float4 EmitterArgument(float argument,float4 textureValue,float4 diffuse){
    float base=fmod(argument,16);
    float4 value=base==2?textureValue:base==3?EmitterFactor:base==6?EmitterConstant:diffuse;
    if(argument>=32)value=value.aaaa;
    if(fmod(floor(argument/16),2)>0)value=1-value;
    return value;
}
float4 EmitterOperation(float operation,float4 a,float4 b){
    if(operation<2.5)return a;
    if(operation<3.5)return b;
    if(operation>6.5)return a+b;
    return a*b*(operation>5.5?4:operation>4.5?2:1);
}
Emission EmitterPS(float2 uv:TEXCOORD0,float3 p:TEXCOORD1,float4 color:COLOR0){
    float4 texelColor=tex2D(Image,uv);
    float4 a=EmitterArgument(EmitterColorStage.y,texelColor,color),b=EmitterArgument(EmitterColorStage.z,texelColor,color);
    float3 rgb=pow(saturate(EmitterOperation(EmitterColorStage.x,a,b).rgb),Camera.y>.5?1:2.2);
    float4 aa=EmitterArgument(EmitterAlphaStage.x,texelColor,color),ab=EmitterArgument(EmitterAlphaStage.y,texelColor,color);
    float nativeAlpha=EmitterColorStage.w<1.5?color.a:EmitterOperation(EmitterColorStage.w,aa,ab).a;
    float alpha=lerp(1,saturate(nativeAlpha),Camera.x);
    float weight=saturate((max(rgb.r,max(rgb.g,rgb.b))-.015)*10)*alpha;
    Emission o;o.color=float4(rgb*alpha*weight,weight);o.position=float4(p*weight,weight);return o;
}
float4 EmitterSum(float2 uv:TEXCOORD0):COLOR0 {
    float4 sum=0;
    [unroll]for(int y=0;y<4;++y)[unroll]for(int x=0;x<4;++x)
        if(x<Camera.z&&y<Camera.z)sum+=tex2D(Image,uv+(float2(x,y)-(Camera.z-1)*.5)*Camera.xy);
    return sum/(Camera.z*Camera.z);
}
float4 EmitterLighting(float2 uv:TEXCOORD0):COLOR0 {
    float raw=RawDepth(uv),z=Z(uv);float3 p=Point(uv,z),n=SurfaceNormal(uv,p);
    float3 light=0;
    [loop]for(int j=0;j<8;++j){
        if(j>=Local.x)break;
        float3 delta=LightPositions[j].xyz-p;
        float d=length(delta),r=LightPositions[j].w*Local.z;
        float falloff=pow(saturate(1-d/max(.01,r)),2);
        float attenuation=max(.1,dot(LightAttenuation[j].xyz,float3(1,d,d*d)));
        light+=LightColors[j].rgb*(saturate(dot(n,delta/max(.001,d)))*falloff/attenuation);
    }
    [loop]for(int i=0;i<32;++i){
        if(Quality.y<.5)break;
        float2 q=float2(frac(i/8.0)+(1/16.0),floor(i/8.0)/4+(1/8.0));
        float4 position=tex2Dlod(AO,float4(q,0,0));
        if(position.w>.00001){
            float3 source=mul(float4(position.xyz/position.w+SourceToEye.xyz,0),View).xyz;
            float3 delta=source-p;float distance=length(delta),range=Local.z*8;
            if(distance<range){
                float3 color=tex2Dlod(Rays,float4(q,0,0)).rgb/position.w;
                float falloff=pow(saturate(1-distance/range),2);
                light+=min(3,color)*falloff*saturate(dot(n,delta/max(.01,distance)))*SourceLife[i/4][i%4];
            }
        }
    }
    // Keep receiver derivatives outside the sparse-source branch; alpha also records the receiver.
    return float4(min(3,light*Local.y)*(raw<.999999),saturate(n.z*.5+.5));
}
float Luma(float3 rgb){return dot(rgb,float3(.299,.587,.114));}
float4 FXAA(float2 uv:TEXCOORD0):COLOR0 {
    float4 original=tex2D(Image,uv);float3 center=original.rgb;
    float n=Luma(tex2D(Image,uv-float2(0,Camera.y)).rgb),s=Luma(tex2D(Image,uv+float2(0,Camera.y)).rgb);
    float e=Luma(tex2D(Image,uv+float2(Camera.x,0)).rgb),w=Luma(tex2D(Image,uv-float2(Camera.x,0)).rgb),m=Luma(center);
    float lo=min(m,min(min(n,s),min(e,w))),hi=max(m,max(max(n,s),max(e,w)));
    if(hi-lo<max(.025,hi*Viewport.y))return original;
    float2 direction=float2(n-s,e-w);
    float reduce=max(.005,(n+s+e+w)*.03125);
    direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduce),-8,8)*Camera.xy;
    float3 a=(tex2D(Image,uv-direction/6).rgb+tex2D(Image,uv+direction/6).rgb)*.5;
    float3 b=a*.5+(tex2D(Image,uv-direction*.5).rgb+tex2D(Image,uv+direction*.5).rgb)*.25;
    float l=Luma(b);float3 smooth=l<lo||l>hi?a:b;
    return float4(lerp(center,smooth,Viewport.x),original.a);
}
