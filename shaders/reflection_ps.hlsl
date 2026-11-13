#include "water_ps.hlsl"
// A fixed cap keeps this nested search within the DX9 32-temporary-register limit.
static const float ReflectionRange=512.0;
// MSAA color coverage can include a foreground leaf while the copied depth
// sample belongs to the bank behind it. Reconstruct edge radiance from nearby
// samples whose depth and four-neighbor footprint support the same source face.
float4 SurfaceRadiance(float2 uv,float2 center,float raw,float2 gradient,float tolerance){
    float2 base=(floor(uv/Screen.xy)+.5)*Screen.xy;
    float4 sum=0;float intact=1;
    [loop]for(int i=0;i<9;++i){
        float2 offset=float2(i%3-1,i/3-1);
        float2 tap=base+offset*Screen.xy;
        float expected=raw+dot(gradient,(tap-center)/Screen.xy);
        float observed=tex2Dlod(SceneDepth,float4(tap,0,0)).r;
        float4 sides=float4(tex2Dlod(SceneDepth,float4(tap-float2(Screen.x,0),0,0)).r,
                            tex2Dlod(SceneDepth,float4(tap+float2(Screen.x,0),0,0)).r,
                            tex2Dlod(SceneDepth,float4(tap-float2(0,Screen.y),0,0)).r,
                            tex2Dlod(SceneDepth,float4(tap+float2(0,Screen.y),0,0)).r);
        float4 predicted=expected+float4(-gradient.x,gradient.x,-gradient.y,gradient.y);
        float4 residual=abs(sides-predicted);
        float error=max(abs(observed-expected),max(max(residual.x,residual.y),max(residual.z,residual.w)));
        float support=1-smoothstep(tolerance,tolerance*2,error);intact=min(intact,support);
        float weight=support/(1+dot(offset,offset));
        if(weight>.001)sum+=float4(tex2Dlod(SceneColor,float4(tap,0,0)).rgb,1)*weight;
    }
    if(intact>.999)return float4(tex2Dlod(SceneColor,float4(uv,0,0)).rgb,1);
    return float4(sum.rgb/max(sum.a,.0001),saturate(sum.a*4));
}
float4 ResolveReflection(float2 hitUV,float3 start,float3 direction,float minimumHitT,bool radiance) {
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
    float faceContinuity=min(max(continuity.x,continuity.y),max(continuity.z,continuity.w));
    float2 gradient=float2(errors.x<=errors.y?slopes.x:slopes.y,errors.z<=errors.w?slopes.z:slopes.w);
    gradient=lerp(gradient,float2((outer.y-outer.x)*.25,(outer.w-outer.z)*.25),
                  float2(max(errors.x,errors.y),max(errors.z,errors.w))<quantum*3);
    float2 ndc=(hitUV-Screen.xy*.5-Viewport.xy)/Viewport.zw*2-1;
    float2 scaled=gradient/Screen.xy*Viewport.zw*.5;
    float3 plane=float3(scaled*Camera.xy*float2(1,-1),hitRaw-Camera.z-dot(scaled,ndc));
    float denominator=dot(plane,direction);
    if(abs(denominator)<=.0000001)return 0;
    float hitT=(Camera.w-dot(plane,start))/denominator;
    if(hitT<=minimumHitT || hitT>=min(ReflectionRange,Trace.y))return 0;
    float3 hit=start+direction*hitT;
    if(hit.z<=.1)return 0;
    float2 refinedUV=ProjectUV(hit);
    float2 refinedCenter=(floor(refinedUV/Screen.xy)+.5)*Screen.xy;
    float expectedRaw=hitRaw+dot(gradient,(refinedCenter-hitUV)/Screen.xy);
    float confirmedRaw=tex2Dlod(SceneDepth,float4(refinedCenter,0,0)).r;
    float matchTolerance=(quantum*3+dot(abs(gradient),float2(.15,.15)))*ReflectionControl.y;
    float valid=1-smoothstep(matchTolerance,matchTolerance*2,abs(confirmedRaw-expectedRaw));
    valid*=all(abs(refinedUV-hitUV)<Screen.xy*1.5) && confirmedRaw<.999999;
    float inverseNormal=rsqrt(dot(plane,plane));
    float normalSign=dot(plane,-hit)<0?-1:1;
    float2 edge=min((refinedUV-Viewport.xy)/Viewport.zw,1-(refinedUV-Viewport.xy)/Viewport.zw);
    float confidence=saturate(min(edge.x,edge.y)*14)*saturate(1-hitT/ReflectionRange);
    confidence*=valid*smoothstep(.005,.05,-dot(plane,direction)*inverseNormal*normalSign);
    // A foreground/background depth jump can fit a fictitious face almost
    // edge-on to the source camera. Its projected footprint has no reliable
    // surface samples and stretches leaf colors into long reflection streaks.
    // Fade that ill-conditioned source projection, not the water's wave normal.
    confidence*=smoothstep(.02,.05,-dot(plane,hit)*inverseNormal*normalSign*rsqrt(dot(hit,hit)));
    confidence*=faceContinuity;
    if(confidence<=.001)return 0;
    if(!radiance)return float4(hitUV,0,confidence);
    float4 sampled=SurfaceRadiance(refinedUV,hitUV,hitRaw,gradient,quantum*.5+dot(abs(gradient),float2(.05,.05)));
    confidence=saturate((confidence*sampled.a-ReflectionControl.z)/max(.001,1-ReflectionControl.z));
    return float4(sampled.rgb*confidence,confidence);
}

float3 TraceReflection(float3 start,float3 direction,float minimumHitT) {
    float previousT=ReflectionControl.x;
    float3 initial=start+direction*previousT;
    float previousDelta=initial.z-DepthZ(ProjectUV(initial));
    if(previousDelta>=0)return 0;
    [loop] for(int i=0;i<64;++i) {
        if(i>=Trace.x) break;
        float u=(i+1)/Trace.x;
        float t=ReflectionControl.x+u*u*min(ReflectionRange,Trace.y);
        float3 ray=start+direction*t;
        if(ray.z<=.1) break;
        float2 uv=ProjectUV(ray);
        float2 localUV=(uv-Viewport.xy)/Viewport.zw;
        if(any(localUV<=.002)||any(localUV>=.998)) break;
        float sceneZ=DepthZ(uv);
        float delta=ray.z-sceneZ;
        if(delta>=0 && previousDelta<0) {
            float lo=previousT,hi=t;
            [loop] for(int j=0;j<16;++j) {
                if(j>=ReflectionControl.w)break;
                float mid=(lo+hi)*.5;
                float3 q=start+direction*mid;
                float z=DepthZ(ProjectUV(q));
                if(q.z>z)hi=mid;else lo=mid;
            }
            float3 q=start+direction*((lo+hi)*.5);
            float2 hitUV=(floor(ProjectUV(q)/Screen.xy)+.5)*Screen.xy;
            // At a rasterized crease the nearest texel may belong to the adjacent face.
            // Retry a one-texel cross, still requiring the actual ray/plane intersection
            // to agree with copied depth. Never smear colors across different surfaces.
            [loop] for(int sample=0;sample<5;++sample){
                float2 offset=sample==1?float2(-1,0):sample==2?float2(1,0):sample==3?float2(0,-1):sample==4?float2(0,1):0;
                float4 result=ResolveReflection(hitUV+offset*Screen.xy,start,direction,minimumHitT,false);
                if(result.a>.001)return float3(result.xy,result.a);
            }
            // A rejected silhouette is not a hit. Search later crossings rather than
            // terminating with zero confidence and punching an artificial hole.
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
    float minimumHitT=.01/max(.000001,dot(reflected,geometric));
    // Advance along the ray in TraceReflection, not along the normal: a normal
    // offset changes the actual mirror plane and detaches nearby reflections.
    float3 start=mul(float4(relative,0),View).xyz;
    float3 direction=mul(float4(reflected,0),View).xyz;
    float3 hit=TraceReflection(start,direction,minimumHitT);
    return float4(hit.x+floor(saturate(hit.z)*255)*2,hit.y,0,1);
}

// Re-evaluate the accepted face before reading radiance. Retain the original face
// anchor: refitting a different face at the refined UV can borrow foreground leaves.
float4 ReflectionColorPS(float3 relative:TEXCOORD1,float2 pixel:VPOS):COLOR0 {
 float2 packed=tex2Dlod(SceneReflection,float4((pixel+.5)*ReflectionTarget.xy,0,0)).rg;
 float code=floor(packed.x*.5);
 if(code<1)return 0;
 float2 anchor=float2(packed.x-code*2,packed.y);
 anchor=(floor(anchor/Screen.xy)+.5)*Screen.xy;
 float3 v=normalize(-relative);
 float3 geometric=normalize(cross(ddx(relative),ddy(relative))+float3(0,.00000001,0));
 if(geometric.y<0)geometric=-geometric;
 float3 reflected=reflect(-v,geometric);
 float minimumHitT=.01/max(.000001,dot(reflected,geometric));
 float supported=saturate((code/255-ReflectionControl.z)/max(.001,1-ReflectionControl.z));
 float4 best=0;
 [loop]for(int i=0;i<5;++i){
   float2 offset=i==1?float2(-1,0):i==2?float2(1,0):i==3?float2(0,-1):i==4?float2(0,1):0;
   float4 candidate=ResolveReflection(anchor+offset*Screen.xy,mul(float4(relative,0),View).xyz,mul(float4(reflected,0),View).xyz,minimumHitT,true);
   if(candidate.a>best.a)best=candidate;
   if(best.a>=supported*.99)break;
 }
 return best;
}
