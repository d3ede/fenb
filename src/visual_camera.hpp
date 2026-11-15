#pragma once
#include "visual_animation.hpp"
#include "presentation_clock.hpp"

namespace visual_camera {
using visual_transform::Vec;
using Process=void(__thiscall *)(void *,void *,float);
using Transform=void(__thiscall *)(void *,void *,void *);
inline Process originalProcess=nullptr;
inline Transform originalTransform=nullptr;
inline void **entries=nullptr;
inline float *cameraVectors=nullptr;
inline void **nativePlayer=nullptr;
using TargetProvider=bool (*)(Vec &,float &,float &);
inline TargetProvider targetProvider=nullptr;
inline void setTargetProvider(TargetProvider provider) { targetProvider=provider; }
struct Pose { Vec eye,look; };
inline Pose read(void *p) { return {visual_animation::field<Vec>(p,4),visual_animation::field<Vec>(p,0x90)}; }
inline void write(void *p,Pose v) { visual_animation::field<Vec>(p,4)=v.eye; visual_animation::field<Vec>(p,0x90)=v.look; }
inline bool finite(Vec v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline bool same(Pose a,Pose b) {
    return a.eye.x==b.eye.x && a.eye.y==b.eye.y && a.eye.z==b.eye.z &&
           a.look.x==b.look.x && a.look.y==b.look.y && a.look.z==b.look.z;
}
struct Target { Vec position{};float yaw=0,pitch=0;bool valid=false; };
inline Target readNativeTarget() {
    Target result;
    if(!nativePlayer || !*nativePlayer) return result;
    auto *p=*nativePlayer;
    result.position=visual_animation::field<Vec>(p,0x160);
    result.yaw=visual_animation::field<float>(p,0x18);
    result.pitch=visual_animation::field<float>(p,0x1c);
    result.valid=finite(result.position) && std::isfinite(result.yaw) && std::isfinite(result.pitch);
    return result;
}
struct History {
    Pose previous{},current{};
    Vec previousShake{},shake{};
    unsigned long long updated=0;
    void *world=nullptr;
    Target target{};
    Vec offset{},destination{};
    float factor=4,yaw=0,pitch=0,zoom=0,shakeSize=0;
    int mode=0;
    bool locked=false,collision=false,recovering=false,shakeContinues=false;
    bool normalOrbit=true;
    uint64_t epoch=0;
};
inline std::unordered_map<void *,History> history;
// Keep the visual camera available to native culling, distance checks, sky and HUD until Present.
inline void *pending=nullptr;
inline Pose saved{},applied{};
inline std::array<float,9> savedVectors{};
inline bool active=false;
inline bool onThread() { return active && visual_animation::enabled && GetCurrentThreadId()==visual_animation::ownerThread; }
inline void finishFrame() {
    if(!pending || GetCurrentThreadId()!=visual_animation::ownerThread) return;
    if(same(read(pending),applied)) {
        write(pending,saved);
        if(cameraVectors) std::copy(savedVectors.begin(),savedVectors.end(),cameraVectors);
    } else {
        // A native camera cut/other owner changed the pose while it was presented.
        // Do not overwrite that newer state with a stale snapshot.
        history.erase(pending);
    }
    pending=nullptr;
}
inline void resetTimeline() { finishFrame();history.clear(); }
inline Pose between(Pose a,Pose b,float t) {
    using namespace visual_transform;
    Vec move=b.eye-a.eye,lookMove=b.look-a.look;
    if(dot(move,move)>=100 || dot(lookMove,lookMove)>=100) return b;
    Vec av=a.eye-a.look,bv=b.eye-b.look;
    float al=std::sqrt(dot(av,av)),bl=std::sqrt(dot(bv,bv));
    if(al<1e-4f || bl<1e-4f) return b;
    av=av*(1/al); bv=bv*(1/bl);
    float d=std::clamp(dot(av,bv),-1.f,1.f);
    if(d<-.95f) return b; // Camera cut, not a continuous mouse turn.
    Vec dir;
    if(d>.9995f) { dir=lerp(av,bv,t); dir=dir*(1/std::sqrt(dot(dir,dir))); }
    else { float angle=std::acos(d),den=std::sin(angle); dir=av*(std::sin((1-t)*angle)/den)+bv*(std::sin(t*angle)/den); }
    Vec look=lerp(a.look,b.look,t);
    return {look+dir*(al+(bl-al)*t),look};
}
inline Vec shakeSample(float size,unsigned long long tick) {
    unsigned seed=unsigned(tick)*747796405u+2891336453u;
    auto next=[&]() { seed^=seed<<13; seed^=seed>>17; seed^=seed<<5; return (float(seed&0xffff)/65535.f-.5f)*size; };
    return {next(),next(),next()};
}
inline float residualGain(float factor,float residualTicks) {
    if(!std::isfinite(factor) || factor<1 || !std::isfinite(residualTicks)) return 0;
    residualTicks=std::clamp(residualTicks,0.f,1.f);
    if(residualTicks==0) return 0;
    return factor==1 ? 1.f : 1.f-std::pow(1.f-1.f/factor,residualTicks);
}
inline Vec rotateYaw(Vec v,float degrees) {
    const float angle=degrees*0.017453292519943295f,c=std::cos(angle),s=std::sin(angle);
    return {v.x*c+v.z*s,v.y,-v.x*s+v.z*c};
}
inline Pose currentTimePose(void *p,const History &h,float residualTicks) {
    using namespace visual_transform;
    Pose result=h.current;
    if(!finite(result.eye) || !finite(result.look) || h.mode!=0 ||
       visual_animation::field<int>(p,0x100)!=h.mode) return result;
    Target target=h.target;
    if(targetProvider) {
        Vec position{};float yaw=0,pitch=0;
        if(targetProvider(position,yaw,pitch) && finite(position) && std::isfinite(yaw) && std::isfinite(pitch))
            target={position,yaw,pitch,true};
    }
    Vec displacement{};
    if(target.valid && h.target.valid) {
        displacement=target.position-h.target.position;
        if(dot(displacement,displacement)>=100.f) return result;
    }
    result.eye=result.eye+displacement;result.look=result.look+displacement;
    if(!h.normalOrbit) return result;
    Vec destination=h.destination;
    const float radius=std::sqrt(dot(destination,destination));
    if(!finite(destination) || !finite(h.offset) || radius<1e-4f) return result;
    const float rawYaw=visual_animation::field<float>(p,0xd0),rawPitch=visual_animation::field<float>(p,0xd4);
    const float zoom=visual_animation::field<float>(p,0xfc);
    if(!std::isfinite(rawYaw) || !std::isfinite(rawPitch) || !std::isfinite(zoom)) return result;
    float yawDelta=std::remainder(rawYaw-h.yaw,360.f);
    float pitchDelta=rawPitch-h.pitch+4.f*(zoom-h.zoom);
    if(target.valid && h.target.valid) {
        if(!h.locked) yawDelta-=std::remainder(target.yaw-h.target.yaw,360.f);
        pitchDelta+=std::remainder(target.pitch-h.target.pitch,360.f);
    }
    // Preserve native special/tracking/collision decisions; only adjust the normal
    // orbit destination for input that arrived since the last simulation update.
    destination=rotateYaw(destination,yawDelta);
    const float horizontal=std::sqrt(destination.x*destination.x+destination.z*destination.z);
    if(horizontal>1e-4f && std::fabs(pitchDelta)<90.f) {
        const float nativePitch=visual_animation::field<float>(p,0xdc);
        if(!std::isfinite(nativePitch)) return result;
        const float pitch=std::clamp(nativePitch+pitchDelta,-80.f,80.f)*0.017453292519943295f;
        float targetRadius=radius;
        if(h.zoom>.001f && radius>=4.f) {
            const float zoomScale=(radius-4.f)/h.zoom;
            if(zoomScale>=1.9f && zoomScale<=16.1f) targetRadius=std::max(.25f,radius+(zoom-h.zoom)*zoomScale);
        }
        const float horizontalScale=targetRadius*std::cos(pitch)/horizontal;
        destination={destination.x*horizontalScale,targetRadius*std::sin(pitch),destination.z*horizontalScale};
    }
    const Vec nativeRelative=h.current.eye-h.current.look;
    // Native look.y is player.y+1.3 while offset is relative to player.y+0.9.
    const Vec desiredRelative=destination-Vec{0,.4f,0},nativeOffset=h.offset-Vec{0,.4f,0};
    Vec relative=nativeRelative+(desiredRelative-nativeOffset)*residualGain(h.factor,residualTicks);
    if(h.collision || h.recovering) {
        const float nativeRadius=std::sqrt(dot(nativeRelative,nativeRelative));
        const float predictedRadius=std::sqrt(dot(relative,relative));
        if(predictedRadius>nativeRadius && predictedRadius>1e-4f) relative=relative*(nativeRadius/predictedRadius);
    }
    result.eye=result.look+relative;
    return finite(result.eye) && finite(result.look) ? result : h.current;
}
inline void __fastcall processHook(void *p,void *,void *device,float factor) {
    if(!onThread()) { originalProcess(p,device,factor); return; }
    finishFrame();
    const Pose before=read(p);
    originalProcess(p,device,factor);
    if(!onThread()) return;
    auto &h=history[p];
    bool continuous=h.updated+1==visual_animation::tick && h.epoch==presentation_clock::frame.epoch;
    h.previous=continuous ? before : read(p); h.current=read(p);
    h.target=readNativeTarget();h.factor=std::isfinite(factor) ? std::max(1.f,factor) : 4.f;
    h.offset=visual_animation::field<Vec>(p,0xb4);h.destination=visual_animation::field<Vec>(p,0xc0);
    h.yaw=visual_animation::field<float>(p,0xd0);h.pitch=visual_animation::field<float>(p,0xd4);
    h.zoom=visual_animation::field<float>(p,0xfc);h.mode=visual_animation::field<int>(p,0x100);
    h.locked=visual_animation::field<int>(p,0xe8)!=0;
    h.collision=visual_animation::field<int>(p,0xf4)!=0;h.recovering=visual_animation::field<int>(p,0xf8)!=0;
    h.normalOrbit=h.target.valid && h.mode==0;
    if(h.normalOrbit) {
        // Tracking/other native orbit modes may use a different yaw target. Only
        // adjust raw mouse input when this destination matches the normal orbit.
        const float nativeYaw=visual_animation::field<float>(p,0xd8);
        const float angle=(nativeYaw-(h.locked ? 0.f : h.target.yaw)+180.f)*0.017453292519943295f;
        const float horizontal=std::sqrt(h.destination.x*h.destination.x+h.destination.z*h.destination.z);
        if(!std::isfinite(angle) || horizontal<1e-4f) h.normalOrbit=false;
        else {
            const float errorX=h.destination.x/horizontal+std::sin(angle);
            const float errorZ=h.destination.z/horizontal+std::cos(angle);
            if(errorX*errorX+errorZ*errorZ>.001f) h.normalOrbit=false;
        }
    }
    if(h.updated!=visual_animation::tick) {
        h.previousShake=continuous ? h.shake : Vec{};
        auto &quake=visual_animation::field<int>(p,0xe0);
        if(quake>0) {
            float size=visual_animation::field<float>(p,0xe4);
            h.shakeSize=std::isfinite(size) ? size : 0;
            h.shake=shakeSample(h.shakeSize,visual_animation::tick);
            --quake; // Native duration is measured in simulation ticks, not render calls.
            h.shakeContinues=quake>0;
        } else { h.shake={};h.shakeSize=0;h.shakeContinues=false; }
    }
    h.updated=visual_animation::tick;
    h.epoch=presentation_clock::frame.epoch;
}
inline void __fastcall transformHook(void *p,void *,void *device,void *world) {
    if(!onThread()) { originalTransform(p,device,world); return; }
    finishFrame();
    auto found=history.find(p);
    if(found==history.end() || found->second.updated!=visual_animation::tick || found->second.epoch!=presentation_clock::frame.epoch) {
        originalTransform(p,device,world); return;
    }
    auto &h=found->second;
    if(h.world!=world) { h.previous=h.current; h.previousShake=h.shake; h.world=world; }
    saved=read(p);
    if(cameraVectors) std::copy_n(cameraVectors,9,savedVectors.begin());
    auto visual=currentTimePose(p,h,visual_animation::phase());
    const Vec nextShake=h.shakeContinues ? shakeSample(h.shakeSize,visual_animation::tick+1) : Vec{};
    visual.eye=visual.eye+visual_transform::lerp(h.shake,nextShake,visual_animation::phase());
    write(p,visual);
    if(cameraVectors) {
        const Vec direction=visual.look-visual.eye;
        const float length=std::sqrt(visual_transform::dot(direction,direction));
        std::memcpy(cameraVectors,&visual.eye,sizeof(Vec));
        std::memcpy(cameraVectors+3,&visual.look,sizeof(Vec));
        Vec normalized=length>1e-6f ? direction*(1/length) : Vec{};
        std::memcpy(cameraVectors+6,&normalized,sizeof(Vec));
    }
    const int quake=visual_animation::field<int>(p,0xe0);
    visual_animation::field<int>(p,0xe0)=0;
    originalTransform(p,device,world); // Native view, inverse view and culling all use this pose.
    visual_animation::field<int>(p,0xe0)=quake;
    applied=visual;
    pending=p;
}
inline bool install(HMODULE image=GetModuleHandleW(nullptr)) {
    if(active) return true;
    if(!visual_animation::enabled) return false;
    client_detail::Image scan(image);
    const auto binding=presentation_detail::camera(scan);if(!binding)return false;
    auto *base=reinterpret_cast<unsigned char *>(image);
    auto **table=reinterpret_cast<void **>(base+binding.table);
    originalProcess=reinterpret_cast<Process>(table[0]); originalTransform=reinterpret_cast<Transform>(table[1]);
    if(!visual_animation::replace(table,reinterpret_cast<void *>(originalProcess),reinterpret_cast<void *>(&processHook))) return false;
    if(!visual_animation::replace(table+1,reinterpret_cast<void *>(originalTransform),reinterpret_cast<void *>(&transformHook))) {
        visual_animation::replace(table,reinterpret_cast<void *>(&processHook),reinterpret_cast<void *>(originalProcess)); return false;
    }
    entries=table; cameraVectors=reinterpret_cast<float *>(base+binding.vectors);
    nativePlayer=reinterpret_cast<void **>(base+binding.player);active=true;
    Log("FPS camera: current-time target/input presentation enabled; native collision and simulation retained.");
    return true;
}
inline void uninstall() {
    finishFrame(); active=false;
    if(entries) {
        visual_animation::replace(entries,reinterpret_cast<void *>(&processHook),reinterpret_cast<void *>(originalProcess));
        visual_animation::replace(entries+1,reinterpret_cast<void *>(&transformHook),reinterpret_cast<void *>(originalTransform));
    }
    entries=nullptr; cameraVectors=nullptr;nativePlayer=nullptr;history.clear();
}
}
