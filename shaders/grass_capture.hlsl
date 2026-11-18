// Local world-space terrain coverage. Never changes the native world/depth draw.
sampler2D Art:register(s0);
sampler2D BlendMap:register(s1);
row_major float4x4 World:register(c0);
row_major float4x4 Tex0:register(c4);
row_major float4x4 Tex1:register(c8);
float4 Map:register(c12); // center xz, radius, texel size
float4 Material:register(c0); // coverage (2 = mixed soil), blend map, opacity, height/luminance write
float4 Contact:register(c1); // 1 = prop, 2 = paving, 3 = plaza atlas, 4 = water
float4 ContactMap:register(c2); // terrain map center xz and radius
struct Input {float3 position:POSITION;float3 normal:NORMAL;float2 uv:TEXCOORD0;float2 blend:TEXCOORD1;};
struct Output {float4 position:POSITION;float3 world:TEXCOORD0;float2 uv:TEXCOORD1;float2 blend:TEXCOORD2;};
Output GrassCaptureVS(Input i) {
    Output o;float4 p=mul(float4(i.position,1),World);
    o.world=p.xyz;
    o.position=float4((p.x-Map.x)/Map.z,(p.z-Map.y)/Map.z,.5,1);
    o.position.xy+=float2(-Map.w,Map.w);
    o.uv=mul(float4(i.uv,1,0),Tex0).xy;
    o.blend=mul(float4(i.blend,1,0),Tex1).xy;
    return o;
}
float4 GrassCapturePS(Output i):COLOR0 {
    float3 n=cross(ddx(i.world),ddy(i.world));
    float slope=abs(n.y)*rsqrt(max(dot(n,n),1e-12));
    if(Contact.x>3.5){
        clip(slope-.65);
        float2 uv=.5+(i.world.xz-ContactMap.xy)*float2(1,-1)/(2*ContactMap.z);
        float4 terrain=tex2D(BlendMap,uv);
        clip(terrain.g-.001);
        clip(i.world.y-terrain.r+.015);
        return float4(2+saturate(.5+(i.world.y-Contact.y)/1024),0,0,1);
    }
    float4 art=tex2D(Art,i.uv);
    if(Contact.x>.5){
        clip(art.a-.5);
        if(Contact.x>1.5 && slope>.8){
            // Share the exact planter exclusion used by the surface mask.
            if(Contact.x>2.5 && i.uv.x>.805 && i.uv.y>.69 && i.uv.y<.86)discard;
            // Keep paving above unrelated geometry under it in the MAX pass.
            return float4(1+saturate(.5+(i.world.y-Contact.y)/1024),0,0,1);
        }
        return float4(saturate(.5+(Contact.y-i.world.y)/1024),0,0,1);
    }
    float opacity=Material.z<0?1:Material.y>.5?tex2D(BlendMap,i.blend).a:art.a*Material.z;
    clip(opacity-.0001);
    // This mask is restricted to exactly identified mixed grass/soil atlases.
    // Warm bare soil receives no blades; source lighting cannot change coverage.
    float coverage=Material.x>1.5?saturate((art.g-art.r*.82-.015)*18)*saturate((art.g-art.b-.02)*14):abs(Material.x);
    // Snow identity belongs to luminance, never to the blended coverage channel:
    // snow and grass layers must not cancel each other's coverage at their seam.
    float luminance=dot(art.rgb,float3(.2126,.7152,.0722))*(Material.x<0?-1:1);
    return float4(i.world.y,coverage,slope,Material.w>.5?luminance:opacity);
}
