#pragma once
#include "visual_animation.hpp"

// Keep a mover's geometry, attachments and name projection in the same presentation space.
namespace visual_mover {
using visual_transform::Vec;
using Process=void(__thiscall *)(void *);
using Render=void(__thiscall *)(void *,void *);
using Destroy=void *(__thiscall *)(void *,unsigned);
inline Process originalProcess=nullptr;
inline Render originalRender=nullptr;
inline Destroy originalDestroy=nullptr;
inline void **entries=nullptr;
inline void **activePlayerSlot=nullptr;
inline bool active=false;
struct Pose { Vec position,angles; std::array<float,16> rotation; void *model; void *world=nullptr; };
inline Pose read(void *p) {
    Pose v{visual_animation::field<Vec>(p,0x160),visual_animation::field<Vec>(p,0x18),{},visual_animation::field<void *>(p,0x178),visual_animation::field<void *>(p,0x16c)};
    std::memcpy(v.rotation.data(),static_cast<char *>(p)+0xa4,64); return v;
}
struct History {
    Pose previous{},current{},saved{},presented{};
    std::array<unsigned char,64> translation{},world{};
    unsigned long long updated=0,epoch=0;
    DWORD dirty=0;
    bool pending=false,local=false;
};
inline std::unordered_map<void *,History> history;
inline bool onThread() { return active && visual_animation::enabled && GetCurrentThreadId()==visual_animation::ownerThread; }
inline bool same(Vec a,Vec b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
inline bool finite(Vec v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline void restore(void *p,History &h) {
    if(!h.pending) return;
    auto current=read(p);
    if(current.world!=h.saved.world) { h.updated=0;h.pending=false;return; }
    const bool positionUnchanged=same(current.position,h.presented.position);
    const bool anglesUnchanged=same(current.angles,h.presented.angles);
    const bool rotationUnchanged=std::memcmp(current.rotation.data(),h.presented.rotation.data(),64)==0;
    if(positionUnchanged) visual_animation::field<Vec>(p,0x160)=h.saved.position;
    if(anglesUnchanged) visual_animation::field<Vec>(p,0x18)=h.saved.angles;
    const bool sameModel=current.model==h.saved.model;
    if(sameModel && rotationUnchanged) std::memcpy(static_cast<char *>(p)+0xa4,h.saved.rotation.data(),64);
    if(sameModel && positionUnchanged && anglesUnchanged && rotationUnchanged) {
        std::memcpy(static_cast<char *>(p)+0x24,h.translation.data(),64);
        std::memcpy(static_cast<char *>(p)+0xe4,h.world.data(),64);
        auto &flags=visual_animation::field<DWORD>(p,4); flags=(flags&~8u)|h.dirty;
    } else {
        // LoadModel or another native render callback owns its changed caches.
        visual_animation::field<DWORD>(p,4)|=8u;h.updated=0;
    }
    h.pending=false;
}
inline void finishFrame() {
    if(GetCurrentThreadId()!=visual_animation::ownerThread) return;
    for(auto &[p,h]:history) restore(p,h);
}
inline void resetTimeline() { finishFrame();history.clear(); }
inline float angleBetween(float a,float b,float alpha) { return a+std::remainder(b-a,360.f)*alpha; }
inline std::array<float,16> moverRotation(Vec angles) {
    constexpr float halfRadians=-3.14159265358979323846f/360.f;
    const float yaw=angles.x*halfRadians,pitch=std::clamp(angles.y,-45.f,45.f)*halfRadians,roll=angles.z*halfRadians;
    const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch),cr=std::cos(roll),sr=std::sin(roll);
    auto q=visual_transform::normalize({cy*sp*cr+sy*cp*sr,sy*cp*cr-cy*sp*sr,cy*cp*sr-sy*sp*cr,cy*cp*cr+sy*sp*sr});
    return visual_transform::compose({{},{1,1,1},q});
}
inline bool currentPresentation(const History &h,Pose &result) {
    result=h.current;
    if(h.previous.model!=h.current.model || h.previous.world!=h.current.world ||
       h.epoch!=presentation_clock::frame.epoch || !finite(h.previous.position) || !finite(h.current.position) ||
       !finite(h.previous.angles) || !finite(h.current.angles)) return false;
    const Vec delta=h.current.position-h.previous.position;
    // A short observed displacement only: teleports/network corrections never become velocities.
    if(visual_transform::dot(delta,delta)>4.f) return false;
    const float alpha=visual_animation::phase();
    std::array<float,16> rotation{};
    if(!h.local && !visual_animation::worldAtCurrent(h.previous.rotation.data(),h.current.rotation.data(),alpha,rotation.data())) return false;
    const float yaw=std::remainder(h.current.angles.x-h.previous.angles.x,360.f);
    const float pitch=std::remainder(h.current.angles.y-h.previous.angles.y,360.f);
    const float roll=std::remainder(h.current.angles.z-h.previous.angles.z,360.f);
    if(std::fabs(yaw)>90 || std::fabs(pitch)>90 || std::fabs(roll)>90) {
        if(!h.local) return false;
        // Local cuts use the new input heading immediately instead of the native matrix
        // which Interpolate() computed before this tick's ProcessAction.
        result.rotation=moverRotation(result.angles);return true;
    }
    result.position=h.current.position+delta*alpha;
    result.angles={h.current.angles.x+yaw*alpha,h.current.angles.y+pitch*alpha,h.current.angles.z+roll*alpha};
    result.rotation=h.local ? moverRotation(result.angles) : rotation;return true;
}
inline Pose presentation(void *p) {
    auto found=history.find(p);
    if(found==history.end()) return read(p);
    auto &h=found->second;
    if(h.pending) {
        const auto live=read(p);
        if(live.model!=h.saved.model || live.world!=h.saved.world || !same(live.position,h.presented.position) ||
           !same(live.angles,h.presented.angles) || std::memcmp(live.rotation.data(),h.presented.rotation.data(),64)!=0) restore(p,h);
    }
    // Render may already hold the presentation fields; use the saved logical endpoint.
    auto native=h.pending ? h.saved : read(p);
    if(h.updated!=visual_animation::tick || native.model!=h.current.model || native.world!=h.current.world ||
       !same(native.position,h.current.position) || !same(native.angles,h.current.angles) ||
       std::memcmp(native.rotation.data(),h.current.rotation.data(),64)!=0) return native;
    Pose result;
    return currentPresentation(h,result) ? result : native;
}
inline Vec presentedPosition(void *p) { return onThread() ? presentation(p).position : read(p).position; }
inline Vec presentedAngles(void *p) { return onThread() ? presentation(p).angles : read(p).angles; }
inline bool localTarget(Vec &position,float &yaw,float &pitch) {
    if(!onThread() || !activePlayerSlot || !*activePlayerSlot) return false;
    auto pose=presentation(*activePlayerSlot);
    if(!finite(pose.position) || !finite(pose.angles)) return false;
    position=pose.position;yaw=pose.angles.x;pitch=pose.angles.y;return true;
}
inline void __fastcall processHook(void *p,void *) {
    if(!onThread()) { originalProcess(p); return; }
    if(auto it=history.find(p);it!=history.end()) restore(p,it->second);
    Pose before=read(p);
    originalProcess(p);
    if(!onThread()) return;
    auto &h=history[p];
    const bool contiguous=h.updated+1==visual_animation::tick && h.epoch==presentation_clock::frame.epoch;
    const Vec lastDelta=h.current.position-h.previous.position;
    h.previous=before; h.current=read(p); h.updated=visual_animation::tick;h.epoch=presentation_clock::frame.epoch;
    h.local=activePlayerSlot && *activePlayerSlot==p;
    const Vec delta=h.current.position-h.previous.position;
    if(h.previous.model!=h.current.model || h.previous.world!=h.current.world || !contiguous ||
       visual_transform::dot(delta,lastDelta)<0) h.previous=h.current;
    auto model=visual_animation::history.find(h.current.model);
    if(model!=visual_animation::history.end()) model->second.ownerTransform=true;
}
inline void __fastcall renderHook(void *p,void *,void *device) {
    if(!onThread()) { originalRender(p,device); return; }
    auto it=history.find(p);
    if(it==history.end() || it->second.updated!=visual_animation::tick) { originalRender(p,device); return; }
    auto &h=it->second;
    if(h.pending) {
        auto current=read(p);
        if(current.model!=h.saved.model || current.world!=h.saved.world || !same(current.position,h.presented.position) ||
           !same(current.angles,h.presented.angles) || std::memcmp(current.rotation.data(),h.presented.rotation.data(),64)!=0) restore(p,h);
        originalRender(p,device); return;
    }
    Pose native=read(p);
    if(native.model!=h.current.model || native.world!=h.current.world || !same(native.position,h.current.position) ||
       !same(native.angles,h.current.angles) || std::memcmp(native.rotation.data(),h.current.rotation.data(),64)!=0 ||
       !currentPresentation(h,h.presented)) {
        originalRender(p,device); return;
    }
    h.saved=native;
    std::memcpy(h.translation.data(),static_cast<char *>(p)+0x24,64);
    std::memcpy(h.world.data(),static_cast<char *>(p)+0xe4,64);
    h.dirty=visual_animation::field<DWORD>(p,4)&8u; h.pending=true;
    visual_animation::field<Vec>(p,0x160)=h.presented.position;
    visual_animation::field<Vec>(p,0x18)=h.presented.angles;
    std::memcpy(static_cast<char *>(p)+0xa4,h.presented.rotation.data(),64);
    visual_animation::field<Vec>(p,0x54)=h.presented.position; // m_matTrans._41..43; native Render keeps its heel offset.
    visual_animation::field<DWORD>(p,4)|=8u;
    originalRender(p,device);
    // Native name/HUD rendering runs later and reads GetPos()/m_matRotation.
    // Restore at Present or before simulation, not immediately after the geometry draw.
}
inline void *__fastcall destroyHook(void *p,void *,unsigned flags) {
    if(GetCurrentThreadId()==visual_animation::ownerThread) {
        auto it=history.find(p); if(it!=history.end()) { restore(p,it->second); history.erase(it); }
    }
    return originalDestroy(p,flags);
}
inline bool install() {
    if(active) return true;
    if(!visual_animation::enabled) return false;
    auto image=GetModuleHandleW(nullptr);client_detail::Image scan(image);
    if(!scan.bytes(0x3098d0,"55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 81 EC D0 01 00 00") ||
       !scan.bytes(0x325470,"55 8B EC 81 EC 04 02 00 00 A1 ?? ?? ?? ?? 33 C5 89 45 FC 56 8B 75 08 57")) return false;
    auto *base=reinterpret_cast<unsigned char *>(image);auto **table=reinterpret_cast<void **>(base+0x7b5054);
    if(table[0]!=base+0x306c10 || table[6]!=base+0x3098d0 || table[9]!=base+0x325470) return false;
    originalDestroy=reinterpret_cast<Destroy>(table[0]);originalProcess=reinterpret_cast<Process>(table[6]);originalRender=reinterpret_cast<Render>(table[9]);
    struct Patch { int slot; void *native,*hook; };
    Patch patches[]={{0,reinterpret_cast<void *>(originalDestroy),reinterpret_cast<void *>(&destroyHook)},
        {6,reinterpret_cast<void *>(originalProcess),reinterpret_cast<void *>(&processHook)},
        {9,reinterpret_cast<void *>(originalRender),reinterpret_cast<void *>(&renderHook)}};
    int installed=0;
    for(auto patch:patches) {
        if(!visual_animation::replace(table+patch.slot,patch.native,patch.hook)) {
            while(installed) { auto undo=patches[--installed];visual_animation::replace(table+undo.slot,undo.hook,undo.native); } return false;
        }
        ++installed;
    }
    entries=table;activePlayerSlot=reinterpret_cast<void **>(base+0x842168);active=true;
    Log("FPS current-time presentation: shared mover/name pose and guarded native restoration installed.");return true;
}
inline void uninstall() {
    finishFrame();active=false;
    if(entries) {
        visual_animation::replace(entries+9,reinterpret_cast<void *>(&renderHook),reinterpret_cast<void *>(originalRender));
        visual_animation::replace(entries+6,reinterpret_cast<void *>(&processHook),reinterpret_cast<void *>(originalProcess));
        visual_animation::replace(entries,reinterpret_cast<void *>(&destroyHook),reinterpret_cast<void *>(originalDestroy));
    }
    entries=nullptr;activePlayerSlot=nullptr;history.clear();
}
}
