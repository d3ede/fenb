sampler2D Art : register(s0);
sampler2D Layer : register(s1);
sampler2D Shadow : register(s2);
row_major float4x4 Clip : register(c0);
row_major float4x4 ViewWorld : register(c4);
row_major float3x3 NormalMatrix : register(c8);
float4 Ambient : register(c11);
float4 Diffuse : register(c12);
float4 Layout : register(c13); // lighting, second coordinate set, vertex fog, normalize normals
float4 LightDirections[8] : register(c14);
float4 LightColors[8] : register(c22);
float4 VertexFog : register(c30);
row_major float4x4 UV0 : register(c31);
row_major float4x4 UV1 : register(c35);
float4 Attenuation[8] : register(c39);
float4 LightAmbient[8] : register(c47);
row_major float4x4 UV2 : register(c55);
float4 ExtraCoordinates : register(c59); // source UV0 / UV1 / view position, projected component
float FogAmount(float depth, float4 settings) {
    if (settings.x < .5) return 1;
    if (settings.x < 1.5) return saturate(exp(-depth*settings.y));
    if (settings.x < 2.5) return saturate(exp(-depth*depth*settings.y*settings.y));
    return saturate((settings.w-depth)/max(.0001,settings.w-settings.z));
}
struct MaterialInput { float3 position:POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD0; float2 blend:TEXCOORD1; };
struct MaterialOutput {
    float4 position:POSITION; float4 diffuse:COLOR0; float2 uv:TEXCOORD0; float2 blend:TEXCOORD1;
    float3 view:TEXCOORD2; float3 normal:TEXCOORD3; float fog:TEXCOORD4; float4 extra:TEXCOORD5;
};
MaterialOutput MaterialVS(MaterialInput i) {
    MaterialOutput o;
    o.position=mul(float4(i.position,1),Clip);
    o.view=mul(float4(i.position,1),ViewWorld).xyz;
    float3 n=mul(i.normal,NormalMatrix);
    if(Layout.w>.5)n*=rsqrt(max(dot(n,n),.00000001));
    float3 color=Ambient.rgb;
    [unroll]for(int index=0;index<8;++index) {
        float3 direction=LightDirections[index].xyz;
        float attenuation=1;
        if(LightDirections[index].w>.5) {
            direction-=o.view;
            float distance=length(direction);
            direction/=max(.00001,distance);
            attenuation=distance<=Attenuation[index].w?1/max(.00001,dot(Attenuation[index].xyz,float3(1,distance,distance*distance))):0;
        }
        color+=(Diffuse.rgb*LightColors[index].rgb*max(0,dot(n,direction))+LightAmbient[index].rgb)*attenuation;
    }
    o.diffuse=Layout.x>.5?float4(saturate(color),Diffuse.a):1;
    o.normal=dot(n,n)>.00000001?normalize(n):float3(0,0,-1);
    o.uv=mul(float4(i.uv,1,0),UV0).xy;
    o.blend=mul(float4(Layout.y>.5?i.blend:i.uv,1,0),UV1).xy;
    float4 extra=ExtraCoordinates.x>1.5?float4(o.view,1):float4(ExtraCoordinates.x>.5?i.blend:i.uv,1,0);
    extra=mul(extra,UV2);
    o.extra=float4(extra.xy,0,ExtraCoordinates.y>3.5?extra.w:ExtraCoordinates.y>2.5?extra.z:1);
    o.fog=FogAmount(VertexFog.x<0?length(o.view):o.view.z,float4(abs(VertexFog.x),VertexFog.yzw));
    return o;
}
struct RigidMaterialInput { float3 position:POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD0; };
MaterialOutput MaterialRigidVS(RigidMaterialInput i) {
    MaterialInput full;
    full.position=i.position;full.normal=i.normal;full.uv=i.uv;full.blend=i.uv;
    return MaterialVS(full);
}
float4 Style : register(c0); // strength, relief, glints, snow appearance
float4 ArtInfo : register(c1); // texel xy, exact material kind, native lighting
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
float4 MaterialDiffuse : register(c13);
float4 Stage2 : register(c14);
float4 Alpha2 : register(c15);
float4 Constant2 : register(c16);
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
float4 MaterialPS(MaterialOutput i):COLOR0 {
    float2 dx=ddx(i.uv),dy=ddy(i.uv);
    float4 art=tex2D(Art,i.uv);
    float3 n=normalize(i.normal),light=SunDirection.xyz;
    float3 diffuse=i.diffuse.rgb;
    [branch]if(Style.x>0) {
        bool snow=ArtInfo.z==1,wood=ArtInfo.z==3;
        float2 texel=ArtInfo.xy;
        const float3 luma=float3(.2126,.7152,.0722);
        float3 west=tex2Dgrad(Art,i.uv-float2(texel.x,0),dx,dy).rgb;
        float3 east=tex2Dgrad(Art,i.uv+float2(texel.x,0),dx,dy).rgb;
        float3 north=tex2Dgrad(Art,i.uv-float2(0,texel.y),dx,dy).rgb;
        float3 south=tex2Dgrad(Art,i.uv+float2(0,texel.y),dx,dy).rgb;
        float left=dot(west,luma),right=dot(east,luma),up=dot(north,luma),down=dot(south,luma);
        // Follow the painted artwork instead of adding a second procedural surface.
        float footprint=max(length(dx/texel),length(dy/texel));
        float fade=1-smoothstep(.7,1.8,footprint);
        float2 gradient=float2(right-left,down-up)*(snow?.3:wood?.7:.8)*Style.y*fade;
        // Compress steep source edges so painted outlines do not become deep grooves.
        gradient/=1+length(gradient)*6.7;
        float3 px=ddx(i.view),py=ddy(i.view);
        float determinant=dx.x*dy.y-dx.y*dy.x;
        float3 tangent=px*dy.y-py*dx.y;
        tangent-=n*dot(n,tangent);
        tangent*=rsqrt(max(dot(tangent,tangent),1e-20));
        tangent*=determinant<0?-1:1;
        float3 bitangent=cross(n,tangent)*(determinant<0?-1:1);
        gradient*=abs(determinant)>.0000000001?1:0;
        float3 bumped=normalize(n-(tangent*gradient.x+bitangent*gradient.y));
        float delta=clamp(dot(bumped,light)-dot(n,light),-.04,.04);
        diffuse=saturate(diffuse+delta*SunColor.rgb*(ArtInfo.w>.5?MaterialDiffuse.rgb:.35)*Style.x);
        if(snow) {
            float brightness=(left+right+up+down)*.25;
            float mask=smoothstep(.23,.6,dot(art.rgb,luma));
            float3 clean=float3(.92,.96,1)*(.85+.15*saturate(brightness));
            art.rgb=lerp(art.rgb,clean,Style.w*Style.x*mask);
        } else {
            // Recover magnified source detail from the same four samples used for relief.
            float magnified=1-smoothstep(.65,1.5,footprint);
            float3 detail=art.rgb-(west+east+north+south)*.25;
            art.rgb=saturate(art.rgb+clamp(detail*Style.y*Style.x*.22*magnified,-.012,.012));
        }
        n=bumped;
    }
    // Preserve native alpha/blend-map coverage and stage saturation order.
    float4 native=NativeStage(Stage0,Alpha0,art,float4(diffuse,i.diffuse.a),float4(diffuse,i.diffuse.a),Constant0);
    float4 layer=1;
    [branch]if(Alpha1.z>.5)layer=tex2D(Layer,i.blend);
    float4 color=NativeStage(Stage1,Alpha1,layer,float4(diffuse,i.diffuse.a),native,Constant1);
    float4 shadow=1;
    [branch]if(Alpha2.z>.5)shadow=tex2Dproj(Shadow,i.extra);
    color=NativeStage(Stage2,Alpha2,shadow,float4(diffuse,i.diffuse.a),color,Constant2);
    float3 halfVector=normalize(light+normalize(-i.view));
    float sparkle=pow(saturate(dot(n,halfVector)),ArtInfo.z==1?128:ArtInfo.z==3?36:48);
    float glintScale=ArtInfo.z==1?.08:ArtInfo.z==3?.025:ArtInfo.z==2?.018:.008;
    color.rgb+=sparkle*SunColor.rgb*Style.z*Style.x*glintScale;
    float fog=PixelFog.x>.5?FogAmount(i.view.z,PixelFog):i.fog;
    color.rgb=lerp(FogColor.rgb,color.rgb,fog);
    return color;
}
