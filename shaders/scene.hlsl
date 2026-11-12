sampler2D Image : register(s0);
sampler2D Depth : register(s1);
sampler2D AO : register(s2);
sampler2D Rays : register(s3);
sampler2D LocalField : register(s4);
sampler2D SkyBackdrop : register(s5);
sampler2D MeanSky : register(s6);
samplerCUBE Atmosphere : register(s7);
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
float4 NativeFogColor : register(c55);
float4 LightPositions[8] : register(c20);
float4 LightColors[8] : register(c28);
float4 LightAttenuation[8] : register(c36);

float2 DepthUV(float2 uv) {return (floor(uv/Texel.xy)+.5)*Texel.xy;}
float RawDepth(float2 uv) {return tex2Dlod(Depth,float4(DepthUV(uv),0,0)).r;}
float Z(float2 uv) {return min(10000,Camera.w/min(-.000001,RawDepth(uv)-Camera.z));}
float3 Point(float2 uv,float z) {
    uv=DepthUV(uv)-Texel.xy*.5;
    float2 ndc=(uv-Viewport.xy)/Viewport.zw*float2(2,-2)+float2(-1,1);
    return float3(ndc/Camera.xy,1)*z;
}
float3 SurfaceNormal(float2 uv,float3 p) {
    // Pick the same surface on each axis; derivatives across silhouettes invent slanted receivers.
    float2 dx=float2(max(Texel.x,Quality.z),0),dy=float2(0,max(Texel.y,Quality.w));
    float3 l=Point(uv-dx,Z(uv-dx)),r=Point(uv+dx,Z(uv+dx));
    float3 u=Point(uv-dy,Z(uv-dy)),d=Point(uv+dy,Z(uv+dy));
    float3 x=abs(l.z-p.z)<abs(r.z-p.z)?p-l:r-p;
    float3 y=abs(u.z-p.z)<abs(d.z-p.z)?p-u:d-p;
    float3 n=cross(x,y);
    n*=rsqrt(max(1e-20,dot(n,n)));
    return dot(n,-p)>0?n:-n;
}
float2 Kernel(int i) {
    float a=i*2.39996323;
    return float2(cos(a),sin(a));
}
float4 SceneAO(float2 uv:TEXCOORD0):COLOR0 {
    float raw=RawDepth(uv),z=Z(uv);
    float3 p=Point(uv,z),n=SurfaceNormal(uv,Point(uv,z));
    float occlusion=0;
    float2 radius=min(.12,Occlusion.y*Camera.xy/max(.1,z)*.5)*Viewport.zw;
    [loop]for(int i=0;i<16;++i){
        if(i>=Quality.x)break;
        float2 q=uv+Kernel(i)*radius*(.2+.8*(i+.5)/Quality.x);
        float3 delta=Point(q,Z(q))-p;
        float distance=length(delta);
        float horizon=max(0,dot(n,delta)-Occlusion.z)/max(.01,distance);
        float valid=all(q>=Viewport.xy)&&all(q<=Viewport.xy+Viewport.zw)&&RawDepth(q)<.999999;
        occlusion+=horizon*saturate(1-distance/Occlusion.y)*valid;
    }
    float fade=saturate(1-z/Occlusion.w)*(raw<.999999);
    return float4((1-saturate(occlusion*(4*Occlusion.x/Quality.x)*fade)).xxx,1);
}
float4 SceneRaySource(float2 uv:TEXCOORD0):COLOR0 {
    // Area-filter the sky classification before radial integration. A single binary depth
    // tap replicated along a ray turns leaf holes into large moving rectangular stamps.
    float sum=0;
    [unroll]for(int y=0;y<4;++y)[unroll]for(int x=0;x<4;++x){
        float2 q=uv+(float2(x,y)-1.5)*Quality.zw*.25;
        float2 fromSun=(q-SunPosition.xy)*float2(Texel.y/Texel.x,1);
        float brightness=dot(tex2Dlod(Image,float4(q,0,0)).rgb,float3(.2126,.7152,.0722));
        float brightSky=saturate((brightness-.45)/.55);
        float valid=all(q>=Viewport.xy)&&all(q<=Viewport.xy+Viewport.zw);
        sum+=brightSky*(RawDepth(q)>.999999)*exp(-dot(fromSun,fromSun)*800)*valid;
    }
    // A directional game light also exists at night: it is not evidence of a visible sun.
    // No bright sky source means no invented luminous disc in front of the camera.
    return float4((sum*(SunPosition.z/16)).xxx,1);
}
float4 SceneRays(float2 uv:TEXCOORD0):COLOR0 {
    float samples=RayParameters.w;
    float2 delta=(SunPosition.xy-uv)*RayParameters.x/samples;
    float sum=0,weight=1;
    [loop]for(int i=0;i<64;++i){
        if(i>=samples)break;
        float2 q=uv+delta*(i+.5);
        float valid=all(q>=Viewport.xy)&&all(q<=Viewport.xy+Viewport.zw);
        sum+=tex2Dlod(Image,float4(q,0,0)).r*weight*valid;
        weight*=RayParameters.z;
    }
    return float4(SunColor.rgb*sum*(SunPosition.w*.12*24/samples),1);
}
float BilateralAO(float2 uv,float z) {
    float sum=0,weights=0;
    float2 cell=uv/Resolution.xy-.5,base=floor(cell),f=frac(cell);
    [unroll]for(int i=0;i<4;++i){
        float2 corner=float2(fmod(i,2),floor(i*.5));
        float2 q=(base+corner+.5)*Resolution.xy;
        float2 bilinear=lerp(1-f,f,corner);
        float w=bilinear.x*bilinear.y*exp(-abs(Z(q)-z)/max(.1,z*.01));
        sum+=tex2Dlod(AO,float4(q,0,0)).r*w;weights+=w;
    }
    return (sum+.03)/(weights+.03);
}
float3 BilateralLight(float2 uv,float z){
    float3 sum=0;float weights=0;
    float2 cell=uv/Resolution.zw-.5,base=floor(cell),f=frac(cell);
    [unroll]for(int i=0;i<4;++i){
        float2 corner=float2(fmod(i,2),floor(i*.5));
        float2 q=(base+corner+.5)*Resolution.zw;
        float2 bilinear=lerp(1-f,f,corner);
        float w=bilinear.x*bilinear.y*exp(-abs(Z(q)-z)/max(.1,z*.01));
        sum+=tex2Dlod(LocalField,float4(q,0,0)).rgb*w;weights+=w;
    }
    // A barely matching background texel must not become a full-strength foreground light.
    return sum/(weights+.03);
}
float4 SceneComposite(float2 uv:TEXCOORD0):COLOR0 {
    float4 source=tex2D(Image,uv);
    float raw=RawDepth(uv),z=Z(uv);
    float3 p=Point(uv,z);
    if(any(uv<Viewport.xy)||any(uv>Viewport.xy+Viewport.zw))return source;
    float3 rgb=pow(max(0,source.rgb),2.2);
    float emissiveCore=smoothstep(.6,.95,max(source.r,max(source.g,source.b)));
    // Input already includes the game's night lighting. Bounded reflectance recovery lets an
    // additional lamp illuminate a dark receiver without adding an unmasked screen-space glow.
    float ambientLevel=max(.12,dot(Illumination.rgb,float3(.2126,.7152,.0722)));
    float3 reflectance=min(.8,rgb/ambientLevel);
    [branch]if(Enabled.z>.5 && emissiveCore<.999){
        float coc=saturate((abs(z-Lens.x)-Lens.y)/max(1,Lens.y))*(z<Lens.x?Lens.w:1);
        float3 sum=rgb;float weights=1;
        [loop]for(int i=0;i<12;++i){
            float2 q=uv+Kernel(i)*sqrt((i+.5)/12)*Texel.xy*(Lens.z*coc);
            float sampleZ=Z(q);
            // Prevent a blurred background from smearing sharp foreground silhouettes.
            float w=sampleZ>=z-max(1,z*.03);
            sum+=pow(max(0,tex2Dlod(Image,float4(q,0,0)).rgb),2.2)*w;weights+=w;
        }
        rgb=lerp(sum/weights,rgb,emissiveCore);
    }
    if(Enabled.x>.5 && raw<.999999)rgb*=BilateralAO(uv,z);
    if(Local.w>.5 && raw<.999999){
        float3 extra=reflectance*BilateralLight(uv,z)*(1-emissiveCore);
        // Soft bounded response avoids hard clipping and harsh polygon-shaped light patches.
        rgb+=saturate(1-rgb)*(1-exp(-extra));
    }
    if(Enabled.w>.5)rgb+=tex2D(Rays,uv).rgb;
    // Match the native DX9 fog composition space when replacing its existing air color.
    rgb=pow(saturate(rgb),1/2.2);
    float3 air=FogBackdrop.x>.5?tex2Dlod(MeanSky,float4(.5,.5,0,0)).rgb:FogTint.rgb*sqrt(Illumination.rgb);
    if(Enabled.y>.5 && FogParameters.x>0 && raw<1){
        float3 worldDelta=mul((float3x3)View,p);
        float density=FogParameters.x*exp(clamp(-(Eye.y+worldDelta.y*.5-FogParameters.z)*FogParameters.w,-8,8));
        float fog=1-exp(-max(0,length(p)-FogParameters.y)*density);
        float3 inscatter=air;
        if(FogBackdrop.x>.5){
            // Atmospheric light is diffuse. Never project moon/cloud detail through an opaque object.
            // Retint the native fog already in the image instead of fogging its attenuated
            // detail a second time. Additional extinction only fills the difference in opacity.
            float native=NativeFog.x*saturate(((NativeFog.w>.5?length(p):p.z)-NativeFog.y)/max(.001,NativeFog.z-NativeFog.y));
            rgb=max(0,rgb+(air-NativeFogColor.rgb)*native*saturate(FogParameters.x*500));
            fog=saturate((fog-native)/max(.001,1-native));
            inscatter*=lerp(FogTint.rgb/float3(.56,.66,.76),1,fog);
        }
        rgb=lerp(rgb,inscatter,fog);
    }
    if(Enabled.y>.5 && FogParameters.x>0 && FogBackdrop.x>.5 && NativeFog.x>.5 &&
       (raw>=1 || (NativeFog.w>.5?length(p):p.z)>=NativeFog.z*.995)) {
        // At the far clip, one MSAA depth sample can be sky even though color contains fogged terrain.
        // Recover only coverage consistent with the known native fog/sky mixture, preserving the sky.
        float3 sky=tex2D(SkyBackdrop,uv).rgb;
        float3 difference=NativeFogColor.rgb-sky;
        float denominator=dot(difference,difference);
        float coverage=saturate(dot(source.rgb-sky,difference)/max(.00001,denominator));
        float error=length(source.rgb-lerp(sky,NativeFogColor.rgb,coverage));
        if(denominator>.0001 && error<.025)
            rgb=lerp(sky,air,coverage);
    }
    return float4(saturate(rgb),source.a);
}
float4 SkyMean(float2 uv:TEXCOORD0):COLOR0 {
    if(Camera.w>.5){
        // World-space horizon radiance, not a view-dependent average dominated by the zenith.
        // The shared sky-only cubemap has no terrain, particles or HUD in it.
        float a=uv.x*6.2831853,y=uv.y*.15;
        float3 rgb=texCUBElod(Atmosphere,float4(cos(a),y,sin(a),0)).rgb;
        rgb=pow(max(0,rgb),2.2);
        rgb/=max(1,dot(rgb,float3(.2126,.7152,.0722))/.65);
        return float4(rgb,1);
    }
    float3 sum=0;
    [unroll]for(int y=0;y<4;++y)[unroll]for(int x=0;x<4;++x) {
        float3 rgb=tex2D(Image,uv+float2(x-1.5,y-1.5)*Camera.xy).rgb;
        if(Camera.z>.5) {
            rgb=pow(max(0,rgb),2.2);
            rgb/=max(1,dot(rgb,float3(.2126,.7152,.0722))/.65);
        }
        sum+=rgb;
    }
    float3 mean=sum/16;
    if(Camera.w<-.5)mean=pow(max(0,mean),1/2.2);
    return float4(mean,1);
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
float4 SkyTexture:register(c24); // inverse native size, detail, fine stars
float4 ReconstructSky(float2 uv){
    float2 p=uv/SkyTexture.xy-.5,base=floor(p),f=frac(p);
    float2 w0=f*(-.5+f*(1-.5*f));
    float2 w1=1+f*f*(-2.5+1.5*f);
    float2 w2=f*(.5+f*(2-1.5*f));
    float2 w3=f*f*(-.5+.5*f),w12=w1+w2;
    float3 x=float3(base.x-.5,base.x+.5+w2.x/w12.x,base.x+2.5)*SkyTexture.x;
    float3 y=float3(base.y-.5,base.y+.5+w2.y/w12.y,base.y+2.5)*SkyTexture.y;
    float3 wx=float3(w0.x,w12.x,w3.x),wy=float3(w0.y,w12.y,w3.y);
    float4 sum=0;
    [unroll]for(int j=0;j<3;++j)[unroll]for(int i=0;i<3;++i)
        sum+=tex2Dlod(Image,float4(x[i],y[j],0,0))*wx[i]*wy[j];
    return saturate(sum);
}
float3 FineSkyStars(float2 p,float2 dx,float2 dy,float height){
    // Upper-hemisphere octahedral coordinates avoid a panorama seam and pole singularity.
    float2 cell=floor(p),local=frac(p)-.5;
    float3 hash=frac(float3(cell.x,cell.y,cell.x)*float3(.1031,.1030,.0973));
    hash+=dot(hash,hash.yzx+33.33);
    hash=frac((hash.xxy+hash.yzz)*hash.zyx);
    float rarity=frac(hash.z*29.97);
    float radius=.045+.13*pow(rarity,7);
    float2 delta=local-(hash.xy-.5)*.6;
    float determinant=dx.x*dy.y-dx.y*dy.x;
    float inverse=(determinant>=0?1:-1)/max(.000001,abs(determinant));
    float2 screenDelta=float2(delta.x*dy.y-delta.y*dy.x,dx.x*delta.y-dx.y*delta.x)*inverse;
    float size=radius*radius/max(.000001,abs(determinant)),variance=size+.35;
    float star=exp(-dot(screenDelta,screenDelta)/variance)*size/variance;
    star*=step(.965,hash.z)*smoothstep(0,.12,height)*(1+rarity);
    return lerp(float3(.76,.84,1),float3(1,.9,.74),rarity*.45)*star;
}
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
    float3 d=normalize(direction);
    float2 starCoordinates=d.xz/max(.001,abs(d.x)+abs(d.y)+abs(d.z))*256;
    float2 starDx=ddx(starCoordinates),starDy=ddy(starCoordinates);
    float2 uvDx=ddx(uv),uvDy=ddy(uv);
    float footprint=max(length(uvDx/SkyTexture.xy),length(uvDy/SkyTexture.xy));
    [branch]if(SkyTexture.z>0){
        [branch]if(footprint<1.2)texel=lerp(texel,ReconstructSky(uv),SkyTexture.z);
        [branch]if((Viewport.x<.5 || Viewport.x>1.5) && Viewport.y>.01){
            float2 span=SkyTexture.xy*(Viewport.x>1.5?8:2.5);
            float3 base=min(texel.rgb,tex2Dgrad(Image,uv+span,uvDx,uvDy).rgb);
            base=min(base,tex2Dgrad(Image,uv-span,uvDx,uvDy).rgb);
            base=min(base,tex2Dgrad(Image,uv+span*float2(1,-1),uvDx,uvDy).rgb);
            base=min(base,tex2Dgrad(Image,uv+span*float2(-1,1),uvDx,uvDy).rgb);
            texel.rgb=lerp(texel.rgb,base,Viewport.y*SkyTexture.z*.9);
            // Gray cloud silhouettes baked into the panorama must occlude the new points.
            float clearSky=1-smoothstep(.08,.2,min(base.r,min(base.g,base.b)));
            if(Viewport.x<.5)
                texel.rgb+=FineSkyStars(starCoordinates,starDx,starDy,d.y)*(SkyTexture.z*SkyTexture.w*Viewport.y*clearSky);
        }
    }
    float3 nativeRGB=SkyOperation(SkyColorStage.x,SkyArgument(SkyColorStage.y,texel,color),SkyArgument(SkyColorStage.z,texel,color)).rgb;
    float nativeAlpha=SkyColorStage.w<1.5?color.a:SkyOperation(SkyColorStage.w,SkyArgument(SkyAlphaStage.x,texel,color),SkyArgument(SkyAlphaStage.y,texel,color)).a;
    float4 source=float4(nativeRGB,nativeAlpha);
    if(Viewport.x>.5){
        if(Viewport.x<1.5)source.rgb=saturate((source.rgb-.5)*Camera.z+.5);
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
        light+=LightColors[j].rgb*(saturate((dot(n,delta)+.6)/(d+.6))*falloff/attenuation);
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
                light+=min(3,color)*falloff*saturate((dot(n,delta)+.6)/(distance+.6))*SourceLife[i/4][i%4];
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
