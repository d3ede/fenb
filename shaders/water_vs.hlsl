row_major float4x4 World : register(c0);
row_major float4x4 View : register(c4);
row_major float4x4 Projection : register(c8);
float4 EyeTime : register(c12);
float4 Waves : register(c13); // amplitude, wavelength, speed, range fog
struct Input {float3 pos:POSITION;float3 normal:NORMAL;float4 color:COLOR0;float2 uv:TEXCOORD0;};
struct Output {float4 pos:POSITION;float2 uv:TEXCOORD0;float3 world:TEXCOORD1;float3 normal:TEXCOORD2;float fogDepth:TEXCOORD3;float4 color:COLOR0;};
Output WaterVS(Input v){
    Output o;
    float3 w=mul(float4(v.pos,1),World).xyz;
    float k=6.2831853/Waves.y;
    float t=EyeTime.w*Waves.z;
    float a=dot(w.xz,float2(.8,.6))*k+t;
    float b=dot(w.xz,float2(-.4,.9165))*k*1.57-t*1.24;
    // Fade displacement on vertical mesh edges; no CPU readback of WRITEONLY vertices.
    float horizontal=saturate(abs(mul(float4(v.normal,0),World).y));
    w.y+=Waves.x*(sin(a)+.4*sin(b))*horizontal;
    float2 slope=Waves.x*k*(cos(a)*float2(.8,.6)+.628*cos(b)*float2(-.4,.9165));
    o.normal=normalize(float3(-slope.x,1,-slope.y));
    o.world=w;o.uv=v.uv;o.color=v.color;
    float4 view=mul(float4(w,1),View);
    o.fogDepth=lerp(abs(view.z),length(view.xyz),Waves.w);o.pos=mul(view,Projection);
    return o;
}
