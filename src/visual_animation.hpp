#pragma once
#include "common.hpp"
#include "client_layout.hpp"
#include "presentation_binding.hpp"
#include "visual_transform.hpp"
#include "presentation_clock.hpp"
#include <array>
#include <cstring>

// Experimental adapter for the inspected x86 model ABI. Never runs animation events.
namespace visual_animation {
using Render = int(__thiscall *)(void *, void *, const float *);
// Homesickness adds a third stack argument, absent from the older public sources.
using Move = void(__thiscall *)(void *, void *, float, int);
using Destroy = void *(__thiscall *)(void *, unsigned);
inline Render originalRender = nullptr;
inline Move originalMove = nullptr;
inline Destroy originalDestroy = nullptr;
inline void **destroyEntry = nullptr;
inline void **entries = nullptr;
inline bool enabled = false;
inline DWORD ownerThread = 0;
inline unsigned long long tick = 0;
inline LARGE_INTEGER lastTick{}, frequency{};
inline float renderPhase = 1.f;
template<class T> T &field(void *p, size_t at) { return *reinterpret_cast<T *>(static_cast<char *>(p)+at); }
struct Sample { float frame, blend; void *motion; int maximum, loop; int pause=0,blending=0; };
inline Sample sample(void *p) {
    return {field<float>(p,0xa0),field<float>(p,0xe8),field<void *>(p,0x108),field<int>(p,0xa8),field<int>(p,0xb4),field<int>(p,0x94),field<int>(p,0xe4)};
}
struct History {
    Sample previous{}, current{};
    unsigned long long updated = 0, matrixTick = 0, epoch = 0;
    std::array<float,16> oldWorld{}, world{};
    bool hasMatrix = false;
    bool ownerTransform = false, pending = false;
    float nativeFrame = 0, nativeBlend = 0;
    float presentedFrame = 0, presentedBlend = 0;
    void *pendingMotion = nullptr;
    int pendingMaximum = 0, pendingLoop = 0, pendingBoneCount = 0, pendingPause = 0, pendingBlending = 0;
    unsigned char *nativeBones = nullptr;
    std::vector<unsigned char> savedBones;
};
inline std::unordered_map<void *,History> history;
inline void restore(void *p,History &h) {
    if(!h.pending) return;
    // Native Render is allowed to restart a clip or replace its motion/bone storage.
    // Retire that sample instead of writing the old clip over the new native state.
    const bool sameIdentity=field<void *>(p,0x108)==h.pendingMotion && field<int>(p,0xa8)==h.pendingMaximum &&
        field<int>(p,0xb4)==h.pendingLoop;
    const bool sameFrame=field<float>(p,0xa0)==h.presentedFrame;
    const bool sameBlend=field<float>(p,0xe8)==h.presentedBlend && field<int>(p,0xe4)==h.pendingBlending;
    const bool samePause=field<int>(p,0x94)==h.pendingPause;
    const bool sameBones=sameIdentity && h.pendingMotion && field<unsigned char *>(p,0x118)==h.nativeBones &&
        field<int>(h.pendingMotion,0x54)==h.pendingBoneCount;
    if(sameIdentity && sameFrame) {
        field<float>(p,0xa0)=h.nativeFrame;
        if(sameBlend) field<float>(p,0xe8)=h.nativeBlend;
        if(sameBlend && samePause && sameBones) std::memcpy(h.nativeBones,h.savedBones.data(),h.savedBones.size());
    }
    if(!sameIdentity || !sameFrame || !sameBlend || !samePause || !sameBones) { h.updated=0; h.hasMatrix=false; }
    h.pending=false;
}
inline void finishFrame() {
    if(GetCurrentThreadId()!=ownerThread) return;
    for(auto &[p,h]:history) restore(p,h);
}
inline void resetTimeline() { finishFrame();history.clear(); }
inline void *__fastcall destroyHook(void *p,void *,unsigned flags) {
    if(GetCurrentThreadId()==ownerThread) {
        auto it=history.find(p);
        if(it!=history.end()) { restore(p,it->second); history.erase(it); }
    }
    return originalDestroy(p,flags);
}
inline float phase() {
    return std::isfinite(renderPhase) ? std::clamp(renderPhase,0.f,1.f) : 0.f;
}
inline float frameBetween(const Sample &a,const Sample &b,float alpha) {
    float delta=b.frame-a.frame;
    if ((b.loop&4) && delta<0) delta+=float(b.maximum);
    float result=a.frame+delta*alpha;
    if ((b.loop&4) && b.maximum>0 && result>=b.maximum) result-=float(b.maximum);
    return result;
}
inline bool continuousClip(const Sample &a,const Sample &b) {
    return a.motion==b.motion && a.maximum==b.maximum && a.loop==b.loop && b.maximum>0 &&
        std::isfinite(a.frame) && std::isfinite(b.frame) && a.frame>=0 && b.frame>=0 &&
        a.frame<float(b.maximum) && b.frame<float(b.maximum);
}
inline float frameAtCurrent(const Sample &a,const Sample &b,float residual) {
    if(!continuousClip(a,b) || b.pause || !std::isfinite(residual)) return b.frame;
    float advance=b.frame-a.frame;
    if((b.loop&4) && advance<0) advance+=float(b.maximum);
    // A restart/correction must not be mistaken for a velocity to predict forward.
    const float limit=std::min(8.f,std::max(1.f,float(b.maximum)*.5f));
    if(advance<0 || advance>limit) return b.frame;
    float value=b.frame+advance*std::clamp(residual,0.f,1.f);
    if(b.loop&4) { if(value>=b.maximum) value=std::fmod(value,float(b.maximum)); }
    else value=std::min(value,float(b.maximum-1));
    return value;
}
inline float blendAtCurrent(const Sample &a,const Sample &b,float residual) {
    if(!continuousClip(a,b) || b.pause || !b.blending || !std::isfinite(a.blend) ||
       !std::isfinite(b.blend) || !std::isfinite(residual)) return b.blend;
    float advance=b.blend-a.blend;
    if(advance<0 || advance>.1f) return b.blend;
    return std::clamp(b.blend+advance*std::clamp(residual,0.f,1.f),0.f,1.f);
}
inline bool worldAtCurrent(const float *a,const float *b,float residual,float *out) {
    visual_transform::Transform previous,current;
    if(!std::isfinite(residual) || !visual_transform::decompose(a,previous) || !visual_transform::decompose(b,current)) return false;
    const auto delta=current.position-previous.position;
    // Limit prediction to one short, observed step; no position integration or collision calls.
    if(visual_transform::dot(delta,delta)>4.f) return false;
    float cosine=std::fabs(previous.rotation.x*current.rotation.x+previous.rotation.y*current.rotation.y+
        previous.rotation.z*current.rotation.z+previous.rotation.w*current.rotation.w);
    if(cosine<.70710678f) return false; // abrupt >90-degree turns are native cuts
    residual=std::clamp(residual,0.f,1.f);
    auto value=visual_transform::compose({current.position+delta*residual,current.scale,
        visual_transform::slerp(previous.rotation,current.rotation,1.f+residual)});
    std::copy(value.begin(),value.end(),out);return true;
}
inline void __fastcall moveHook(void *p,void *,void *sound,float speed,int extra) {
    if (!enabled || GetCurrentThreadId()!=ownerThread) { originalMove(p,sound,speed,extra); return; }
    if(auto it=history.find(p);it!=history.end()) restore(p,it->second);
    const auto before=sample(p);
    originalMove(p,sound,speed,extra);
    if(!enabled) return;
    auto &h=history[p];
    if(h.updated+1!=tick || h.epoch!=presentation_clock::frame.epoch) h.hasMatrix=false;
    h.previous=before; h.current=sample(p); h.updated=tick; h.epoch=presentation_clock::frame.epoch;
}
inline int __fastcall renderHook(void *p,void *,void *device,const float *matrix) {
    if (!enabled || GetCurrentThreadId()!=ownerThread || !matrix)
        return originalRender(p,device,matrix);
    auto found=history.find(p);
    if (found==history.end())
        return originalRender(p,device,matrix);
    auto &h=found->second;
    // All passes in this render frame must see the same pose, including native glow/attachments.
    if(h.pending) {
        const bool unchanged=field<void *>(p,0x108)==h.pendingMotion && field<int>(p,0xa8)==h.pendingMaximum &&
            field<int>(p,0xb4)==h.pendingLoop && field<float>(p,0xa0)==h.presentedFrame &&
            field<float>(p,0xe8)==h.presentedBlend && field<int>(p,0x94)==h.pendingPause &&
            field<int>(p,0xe4)==h.pendingBlending && field<unsigned char *>(p,0x118)==h.nativeBones &&
            h.pendingMotion && field<int>(h.pendingMotion,0x54)==h.pendingBoneCount;
        if(!unchanged) { restore(p,h);return originalRender(p,device,matrix); }
        std::array<float,16> repeated{};std::copy_n(matrix,16,repeated.begin());
        bool same=true;for(int i=0;i<16;++i) if(matrix[i]!=h.world[i]) same=false;
        if(same && !h.ownerTransform) worldAtCurrent(h.oldWorld.data(),h.world.data(),phase(),repeated.data());
        return originalRender(p,device,repeated.data());
    }
    const auto current=sample(p);
    if (h.updated!=tick || h.epoch!=presentation_clock::frame.epoch)
        return originalRender(p,device,matrix);
    const bool interpolatePose=continuousClip(h.previous,h.current) && current.motion==h.current.motion &&
        current.maximum==h.current.maximum && current.loop==h.current.loop && current.frame==h.current.frame &&
        current.pause==h.current.pause;
    const bool interpolateBlend=current.blend==h.current.blend && current.blending==h.current.blending;
    const float alpha=phase();
    if (!h.hasMatrix) { std::copy_n(matrix,16,h.world.begin()); h.oldWorld=h.world; h.hasMatrix=true; }
    if (h.matrixTick!=tick) {
        h.oldWorld=h.world; std::copy_n(matrix,16,h.world.begin());
        if(h.matrixTick+1!=tick) h.oldWorld=h.world;
        h.matrixTick=tick;
    }
    std::array<float,16> visual{}; std::copy_n(matrix,16,visual.begin());
    // Only interpolate the matching affine world pass; retain mirrored/special passes.
    bool same=true; for(int i=0;i<16;++i) if(matrix[i]!=h.world[i]) same=false;
    if(same && !h.ownerTransform) worldAtCurrent(h.oldWorld.data(),h.world.data(),alpha,visual.data());
    if(!interpolatePose) return originalRender(p,device,visual.data());
    auto *motion=field<void *>(p,0x108);
    auto *bones=field<unsigned char *>(p,0x118);
    const int count=motion ? field<int>(motion,0x54) : 0;
    if(!bones || count<1 || count>256) return originalRender(p,device,visual.data());
    h.savedBones.resize(size_t(count)*64);
    std::memcpy(h.savedBones.data(),bones,h.savedBones.size());
    h.nativeBones=bones; h.nativeFrame=field<float>(p,0xa0); h.nativeBlend=field<float>(p,0xe8); h.pending=true;
    h.pendingMotion=motion;h.pendingMaximum=current.maximum;h.pendingLoop=current.loop;h.pendingBoneCount=count;
    h.pendingPause=current.pause;h.pendingBlending=current.blending;
    h.presentedFrame=frameAtCurrent(h.previous,h.current,alpha);
    h.presentedBlend=interpolateBlend ? blendAtCurrent(h.previous,h.current,alpha) : current.blend;
    field<float>(p,0xa0)=h.presentedFrame;
    field<float>(p,0xe8)=h.presentedBlend;
    return originalRender(p,device,visual.data());
}
inline bool replace(void **slot,void *expected,void *value) {
    DWORD protection=0,ignored=0;
    if(!VirtualProtect(slot,sizeof(void *),PAGE_READWRITE,&protection)) return false;
    auto old=InterlockedCompareExchangePointer(slot,value,expected);
    VirtualProtect(slot,sizeof(void *),protection,&ignored);
    return old==expected;
}
inline void install(HMODULE image=GetModuleHandleW(nullptr)) {
    if(enabled) return;
    client_detail::Image scan(image);
    const auto binding=presentation_detail::animation(scan);
    if(!binding)return;
    auto *base=reinterpret_cast<unsigned char *>(image);
    auto **table=reinterpret_cast<void **>(base+binding.table);
    auto **destructor=reinterpret_cast<void **>(base+binding.destroyTable);
    originalDestroy=reinterpret_cast<Destroy>(*destructor);
    originalRender=reinterpret_cast<Render>(table[0]); originalMove=reinterpret_cast<Move>(table[2]);
    if(!replace(table,reinterpret_cast<void *>(originalRender),reinterpret_cast<void *>(&renderHook))) return;
    if(!replace(table+2,reinterpret_cast<void *>(originalMove),reinterpret_cast<void *>(&moveHook))) {
        replace(table,reinterpret_cast<void *>(&renderHook),reinterpret_cast<void *>(originalRender)); return;
    }
    if(!replace(destructor,reinterpret_cast<void *>(originalDestroy),reinterpret_cast<void *>(&destroyHook))) {
        replace(table+2,reinterpret_cast<void *>(&moveHook),reinterpret_cast<void *>(originalMove));
        replace(table,reinterpret_cast<void *>(&renderHook),reinterpret_cast<void *>(originalRender)); return;
    }
    destroyEntry=destructor;
    entries=table; QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&lastTick);
    enabled=true; Log("FPS current-time presentation: native pose sampling and guarded frame-wide state installed.");
}
inline void beginTick() {
    if(!enabled) return;
    ownerThread=GetCurrentThreadId(); ++tick; QueryPerformanceCounter(&lastTick);
    if(history.size()>16384) history.clear();
    if(tick%625==0) for(auto it=history.begin();it!=history.end();) {
        if(tick-it->second.updated>625) it=history.erase(it); else ++it;
    }
}
inline void uninstall() {
    finishFrame();
    enabled=false;
    if(entries) {
        replace(entries,reinterpret_cast<void *>(&renderHook),reinterpret_cast<void *>(originalRender));
        replace(entries+2,reinterpret_cast<void *>(&moveHook),reinterpret_cast<void *>(originalMove));
    }
    if(destroyEntry) replace(destroyEntry,reinterpret_cast<void *>(&destroyHook),reinterpret_cast<void *>(originalDestroy));
    destroyEntry=nullptr;
    entries=nullptr; history.clear(); ownerThread=0;
}
}
