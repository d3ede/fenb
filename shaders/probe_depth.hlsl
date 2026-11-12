// Diagnostic-only lossless readback of the already captured water depth texture.
sampler2D Depth : register(s0);
float4 ProbeDepth(float2 uv : TEXCOORD0) : COLOR0 { return tex2Dlod(Depth,float4(uv,0,0)).rrrr; }
struct ProbeVertex { float4 pos:POSITION; float4 value:TEXCOORD0; };
ProbeVertex ProbeVerticesVS(float3 position:POSITION0,float2 destination:TEXCOORD1){
    ProbeVertex o;o.pos=float4(destination,0,1);o.value=float4(position,1);return o;
}
float4 ProbeVerticesPS(float4 value:TEXCOORD0):COLOR0 { return value; }
