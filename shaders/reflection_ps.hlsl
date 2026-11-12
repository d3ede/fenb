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
            float2 hitUV=(floor(ProjectUV(q)/Screen.xy)+.5)*Screen.xy;
            float hitRaw=tex2Dlod(SceneDepth,float4(hitUV,0,0)).r;
            if(hitRaw>=.999999)return 0; // Clear depth is sky, not an opaque plane at the far clip.
            float2 dx=float2(Screen.x,0),dy=float2(0,Screen.y);
            float4 neighbors=float4(tex2Dlod(SceneDepth,float4(hitUV-dx,0,0)).r,
                                    tex2Dlod(SceneDepth,float4(hitUV+dx,0,0)).r,
                                    tex2Dlod(SceneDepth,float4(hitUV-dy,0,0)).r,
                                    tex2Dlod(SceneDepth,float4(hitUV+dy,0,0)).r);
            // A continuous low-poly bank may change slope at every triangle edge.
            // Test each side's extrapolation independently instead of turning that crease
            // into a missing-reflection wire. One stable side per axis is sufficient:
            // the other side can belong to the adjacent face or an occluded surface.
            float4 outer=float4(tex2Dlod(SceneDepth,float4(hitUV-2*dx,0,0)).r,
                                tex2Dlod(SceneDepth,float4(hitUV+2*dx,0,0)).r,
                                tex2Dlod(SceneDepth,float4(hitUV-2*dy,0,0)).r,
                                tex2Dlod(SceneDepth,float4(hitUV+2*dy,0,0)).r);
            float quantum=max(.0000001,Trace.w*abs(Camera.w)*2);
            float4 allowance=abs(neighbors-outer)*1.25+quantum*3;
            float4 continuity=saturate(1-max(0,abs(2*neighbors-outer-hitRaw)-allowance)/allowance);
            // Fit one continuous face in raw perspective depth, which is affine on that face.
            // A nearest-depth bisection alone converges to texel stair steps, not geometry.
            float4 slopes=float4(hitRaw-outer.x,outer.y-hitRaw,hitRaw-outer.z,outer.w-hitRaw)*.5;
            float4 errors=abs(2*neighbors-outer-hitRaw);
            float2 gradient=float2(errors.x<=errors.y?slopes.x:slopes.y,errors.z<=errors.w?slopes.z:slopes.w);
            gradient=lerp(gradient,float2((outer.y-outer.x)*.25,(outer.w-outer.z)*.25),
                          float2(max(errors.x,errors.y),max(errors.z,errors.w))<quantum*3);
            float2 ndc=(hitUV-Screen.xy*.5-Viewport.xy)/Viewport.zw*2-1;
            float2 scaled=gradient/Screen.xy*Viewport.zw*.5;
            float3 plane=float3(scaled*Camera.xy*float2(1,-1),hitRaw-Camera.z-dot(scaled,ndc));
            float denominator=dot(plane,direction);
            float hitT=(Camera.w-dot(plane,start))/(abs(denominator)>.0000001?denominator:.0000001);
            float3 hit=start+direction*hitT;
            float2 refinedUV=ProjectUV(hit);
            float2 refinedCenter=(floor(refinedUV/Screen.xy)+.5)*Screen.xy;
            float expectedRaw=hitRaw+dot(gradient,(refinedCenter-hitUV)/Screen.xy);
            float confirmedRaw=tex2Dlod(SceneDepth,float4(refinedCenter,0,0)).r;
            float matchTolerance=quantum*3+dot(abs(gradient),float2(.15,.15));
            float valid=1-smoothstep(matchTolerance,matchTolerance*2,abs(confirmedRaw-expectedRaw));
            valid*=hitT>0 && hitT<Trace.y && dot(hit-start,planeNormal)>.01 &&
                   all(abs(refinedUV-hitUV)<Screen.xy*1.5) && confirmedRaw<.999999;
            float3 hitNormal=normalize(plane);
            if(dot(hitNormal,-hit)<0)hitNormal=-hitNormal;
            float2 edge=min((refinedUV-Viewport.xy)/Viewport.zw,1-(refinedUV-Viewport.xy)/Viewport.zw);
            float confidence=saturate(min(edge.x,edge.y)*14)*saturate(1-hitT/Trace.y);
            confidence*=valid*smoothstep(.005,.05,-dot(hitNormal,direction));
            confidence*=min(max(continuity.x,continuity.y),max(continuity.z,continuity.w));
            float3 radiance=tex2Dlod(SceneColor,float4(refinedUV,0,0)).rgb;
            [branch] if(max(max(errors.x,errors.y),max(errors.z,errors.w))>quantum*3){
                // A bilinear color fetch must not borrow a foreground character from
                // the neighboring depth surface at a bank/object silhouette.
                float2 grid=refinedUV/Screen.xy-.5;
                float2 baseUV=(floor(grid)+.5)*Screen.xy,f=frac(grid);
                float4 weights=float4((1-f.x)*(1-f.y),f.x*(1-f.y),(1-f.x)*f.y,f.x*f.y);
                float4 raw=float4(tex2Dlod(SceneDepth,float4(baseUV,0,0)).r,
                                  tex2Dlod(SceneDepth,float4(baseUV+dx,0,0)).r,
                                  tex2Dlod(SceneDepth,float4(baseUV+dy,0,0)).r,
                                  tex2Dlod(SceneDepth,float4(baseUV+dx+dy,0,0)).r);
                float expected=hitRaw+dot(gradient,(baseUV-hitUV)/Screen.xy);
                float4 predicted=expected+float4(0,gradient.x,gradient.y,gradient.x+gradient.y);
                weights*=1-smoothstep(matchTolerance,matchTolerance*2,abs(raw-predicted));
                float sum=dot(weights,1);
                radiance=(tex2Dlod(SceneColor,float4(baseUV,0,0)).rgb*weights.x+
                          tex2Dlod(SceneColor,float4(baseUV+dx,0,0)).rgb*weights.y+
                          tex2Dlod(SceneColor,float4(baseUV+dy,0,0)).rgb*weights.z+
                          tex2Dlod(SceneColor,float4(baseUV+dx+dy,0,0)).rgb*weights.w)/max(.0001,sum);
                confidence*=saturate(sum*2);
            }
            return float4(radiance,confidence);
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
    // Advance along the ray in TraceReflection, not along the normal: a normal
    // offset changes the actual mirror plane and detaches nearby reflections.
    float3 start=mul(float4(relative,0),View).xyz;
    float3 direction=mul(float4(reflected,0),View).xyz;
    return TraceReflection(start,direction,planeNormal);
}
