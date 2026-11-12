#include "water_ps.hlsl"
float4 TraceReflection(float3 start,float3 direction,float3 planeNormal) {
    float previousT=.06;
    float3 initial=start+direction*previousT;
    float previousDelta=initial.z-DepthZ(ProjectUV(initial));
    if(previousDelta>=0)return 0;
    [loop] for(int i=0;i<24;++i) {
        if(i>=Trace.x) break;
        float u=(i+1)/Trace.x;
        float t=.06+u*u*Trace.y;
        float3 ray=start+direction*t;
        if(ray.z<=.1) break;
        float2 uv=ProjectUV(ray);
        float2 localUV=(uv-Viewport.xy)/Viewport.zw;
        if(any(localUV<=.002)||any(localUV>=.998)) break;
        float sceneZ=DepthZ(uv);
        float delta=ray.z-sceneZ;
        if(delta>=0 && previousDelta<0) {
            float lo=previousT,hi=t;
            [loop] for(int j=0;j<12;++j) {
                float mid=(lo+hi)*.5;
                float3 q=start+direction*mid;
                float z=DepthZ(ProjectUV(q));
                if(q.z>z)hi=mid;else lo=mid;
            }
            float3 q=start+direction*((lo+hi)*.5);
            float2 hitUV=ProjectUV(q);
            float hitZ=DepthZ(hitUV);
            float3 hit=ViewPoint(hitUV,hitZ);
            // Reject silhouette jumps and loose depth matches instead of stretching a bank into water.
            float tolerance=clamp(hitZ*hitZ*Trace.w,.03,.35);
            float2 dx=float2(Screen.x,0),dy=float2(0,Screen.y);
            float zl=DepthZ(hitUV-dx),zr=DepthZ(hitUV+dx);
            float zu=DepthZ(hitUV-dy),zd=DepthZ(hitUV+dy);
            float3 tx=abs(zl-hitZ)<abs(zr-hitZ)?hit-ViewPoint(hitUV-dx,zl):ViewPoint(hitUV+dx,zr)-hit;
            float3 ty=abs(zu-hitZ)<abs(zd-hitZ)?hit-ViewPoint(hitUV-dy,zu):ViewPoint(hitUV+dy,zd)-hit;
            float3 hitNormal=normalize(cross(tx,ty)+float3(0,0,.000001));
            if(dot(hitNormal,-hit)<0)hitNormal=-hitNormal;
            float edgeZ=max(max(abs(zl-hitZ),abs(zr-hitZ)),max(abs(zu-hitZ),abs(zd-hitZ)));
            float2 edge=min((hitUV-Viewport.xy)/Viewport.zw,1-(hitUV-Viewport.xy)/Viewport.zw);
            float confidence=saturate(min(edge.x,edge.y)*14)*saturate(1-hi/Trace.y);
            confidence*=abs(q.z-hitZ)<tolerance && dot(hit-start,planeNormal)>.01;
            // A camera-visible surface is usable only if the reflected ray hits its front face.
            // Thin bank silhouettes and grazing/backface intersections must fall back to the sky.
            confidence*=smoothstep(.03,.18,-dot(hitNormal,direction));
            // Precision of the depth format is not a bound on a sloped bank's pixel gradient.
            // Keep the silhouette rejection relative to view distance, independently of D16/D24.
            confidence*=saturate(1-edgeZ/max(.05,hitZ*.003));
            return float4(tex2Dlod(SceneColor,float4(hitUV,0,0)).rgb,confidence);
        }
        previousT=t;previousDelta=delta;
    }
    return 0;
}
float4 ReflectionPS(float2 uv:TEXCOORD0,float3 relative:TEXCOORD1,float3 normal:TEXCOORD2,float fogDepth:TEXCOORD3,float4 color:COLOR0,float2 pixel:VPOS):COLOR0 {
    float3 v=normalize(-relative);
    float3 geometric=normalize(cross(ddx(relative),ddy(relative))+float3(0,.00000001,0));
    if(geometric.y<0)geometric=-geometric;
    float3 reflected=reflect(-v,geometric);
    if(reflected.y<=.005 || v.y<=0 || geometric.y<=.45)return 0;
    float3 planeNormal=mul(float4(geometric,0),View).xyz;
    float3 start=mul(float4(relative,0),View).xyz+planeNormal*.05;
    float3 direction=mul(float4(reflected,0),View).xyz;
    return TraceReflection(start,direction,planeNormal);
}
