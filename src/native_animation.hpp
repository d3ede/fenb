#pragma once
#include "common.hpp"
#include "client_layout.hpp"
#include "hybrid_time.hpp"
#include "native_patch.hpp"
#include <array>
#include <cstring>
#include <limits>

// Native simulation adapter for the inspected Homesickness model ABI.
// No render-time phase substitutions: the authoritative float frame advances once
// per hybrid update, while native action consumers observe the previous interval.
namespace native_animation {
template<class T> T &field(void *p,size_t at) { return *reinterpret_cast<T *>(static_cast<char *>(p)+at); }
using Move=void(__thiscall *)(void *,void *,float,int);
using Destroy=void *(__thiscall *)(void *,unsigned);
using Query=int(__thiscall *)(void *);
using QueryAt=int(__thiscall *)(void *,float);
using TrailProcess=void(__thiscall *)(void *);
using Sound=void(*)(int,void *);
using Quake=void(*)(bool);
using BoneSampler=void(*)(void *,float,void *,void *,int,int,int,float);
struct Attribute { DWORD flags;int sound;float key; };
static_assert(sizeof(Attribute)==12);
struct Attributes { Attribute *data=nullptr;int maximum=0;bool motion=false; };
inline bool enabled=false;
inline void **table=nullptr;
inline Move originalMove=nullptr;
inline Destroy originalDestroy=nullptr;
inline Query originalHit=nullptr,originalSound=nullptr,originalQuake=nullptr;
inline QueryAt originalHitAt=nullptr,originalSoundAt=nullptr,originalQuakeAt=nullptr;
inline TrailProcess trailProcess=nullptr;
inline Sound playSound=nullptr;
inline Quake setQuake=nullptr;
inline BoneSampler boneSampler=nullptr;
inline void *boneEntry=nullptr;
inline unsigned char *imageBase=nullptr;
inline bool active() { return enabled && hybrid_time::frame.active && hybrid_time::frame.thread==GetCurrentThreadId() &&
    hybrid_time::frame.stage==hybrid_time::Stage::World; }
inline float ticks() { return std::isfinite(hybrid_time::frame.ticks) ? std::clamp(hybrid_time::frame.ticks,0.f,15.625f) : 0.f; }
struct BoneKey {
    uint64_t serial=0;
    void *motion=nullptr,*oldMotion=nullptr,*output=nullptr;
    float frame=0,blend=0;
    int maximum=0,next=0,oldAnchor=0,blending=0,count=0;
    bool operator==(const BoneKey &) const=default;
};
inline std::unordered_map<void *,BoneKey> boneCache;
inline std::mutex boneMutex;
inline void nativeBones(void *motion,float frame,void *output,void *oldMotion,int next,int oldAnchor,int blending,float blend) {
#if FENB_NATIVE_X86_ASM
    // Identical to native Render's sampler call; the native callee returns RET24.
    __asm {
        push blend
        push blending
        push oldAnchor
        push next
        push oldMotion
        push output
        mov ecx,motion
        movss xmm3,frame
        call boneEntry
    }
#endif
}
inline bool refreshBones(void *model) {
    if(!active() || !model || !table || field<void **>(model,0)!=table || !boneSampler) return false;
    BoneKey key;key.serial=hybrid_time::frame.serial;
    key.motion=field<void *>(model,0x108);key.oldMotion=field<void *>(model,0x104);key.output=field<void *>(model,0x118);
    key.frame=field<float>(model,0xa0);key.maximum=field<int>(model,0xa8);
    key.oldAnchor=field<int>(model,0x114);key.blending=field<int>(model,0xe4);key.blend=field<float>(model,0xe8);
    auto skeleton=field<void *>(model,0x100);
    if(!key.motion || !key.output || !skeleton || key.maximum<1 || key.maximum>100000 ||
       !std::isfinite(key.frame) || key.frame<0 || key.frame>=float(key.maximum) || !std::isfinite(key.blend)) return false;
    key.count=field<int>(key.motion,0x54);
    if(key.count<1 || key.count>256 || field<int>(skeleton,0x44)<key.count ||
       field<int>(key.motion,8)!=key.maximum || !field<void *>(key.motion,0x64) || !field<void *>(key.motion,0x68)) return false;
    if(key.oldMotion && (field<int>(key.oldMotion,0x54)<key.count || !field<void *>(key.oldMotion,0x64))) return false;
    if(key.blending) {
        auto old=key.oldMotion ? key.oldMotion : key.motion;
        if(key.oldAnchor<0 || key.oldAnchor>=field<int>(old,8) || key.blend<0 || key.blend>1) return false;
    }
    key.next=int(key.frame)+1;
    if(key.next>=key.maximum) key.next=(field<int>(model,0xb4)&4) ? 0 : key.maximum-1;
    {
        std::lock_guard lock(boneMutex);
        auto found=boneCache.find(model);if(found!=boneCache.end() && found->second==key) return true;
    }
    // Only writes the engine's existing bone output buffer. Native Render later
    // samples the same authoritative phase again; no frame/events are advanced.
    boneSampler(key.motion,key.frame,key.output,key.oldMotion,key.next,key.oldAnchor,key.blending,key.blend);
    {
        std::lock_guard lock(boneMutex);boneCache[model]=key;
    }
    return true;
}
inline void *__fastcall destroyHook(void *model,void *,unsigned flags) {
    { std::lock_guard lock(boneMutex);boneCache.erase(model); }
    return originalDestroy(model,flags);
}
inline Attributes attributes(void *p) {
    Attributes a;a.maximum=field<int>(p,0xa8);
    if(a.maximum<1 || a.maximum>100000) return {};
    if(auto motion=field<void *>(p,0x108)) { a.data=field<Attribute *>(motion,4);a.motion=true; }
    else {
        auto begin=field<unsigned char *>(p,0xf4),end=field<unsigned char *>(p,0xf8);
        if(begin && begin!=end) if(auto object=field<void *>(begin,4)) {
            a.data=field<Attribute *>(object,0x48);
            a.maximum=field<int>(object,0xc1c);
        }
    }
    if(!a.data || a.maximum<1 || a.maximum>100000) return {};
    return a;
}
template<class F> void crossed(const Attributes &a,float old,float now,DWORD mask,F &&visit) {
    if(!a.data || !std::isfinite(old) || !std::isfinite(now) || now<0 || now>=a.maximum || old>now) return;
    // Large discontinuities are clip resets, not thousands of pending effects.
    const double first=std::floor(double(old)),last=std::floor(double(now));
    if(first<double(std::numeric_limits<int>::min()+1) || last-first>4096) return;
    for(int step=int(first),lastStep=int(last);step<=lastStep;++step) {
        int index=step%a.maximum;if(index<0) index+=a.maximum;
        auto &attr=a.data[index];
        if(!(attr.flags&mask) || !std::isfinite(attr.key) || attr.key<float(index) || attr.key>=float(index+1)) continue;
        const double key=double(step-index)+double(attr.key);
        if(key>old && key<=now) visit(attr);
    }
}
inline Attribute *firstCrossed(void *p,float now,DWORD mask) {
    Attribute *result=nullptr;
    crossed(attributes(p),field<float>(p,0xa4),now,mask,[&](Attribute &a) { if(!result) result=&a; });
    return result;
}
inline int flagsCrossed(void *p,float now,DWORD mask) {
    DWORD result=0;
    crossed(attributes(p),field<float>(p,0xa4),now,mask,[&](Attribute &a) { result|=a.flags; });
    return int(result);
}
inline int __fastcall hitHook(void *p,void *) { return active() ? flagsCrossed(p,field<float>(p,0xa0),1) : originalHit(p); }
inline int __fastcall hitAtHook(void *p,void *,float f) { return active() ? flagsCrossed(p,f,1) : originalHitAt(p,f); }
inline int __fastcall soundHook(void *p,void *) { return active() ? int(reinterpret_cast<uintptr_t>(firstCrossed(p,field<float>(p,0xa0),2))) : originalSound(p); }
inline int __fastcall soundAtHook(void *p,void *,float f) { return active() ? int(reinterpret_cast<uintptr_t>(firstCrossed(p,f,2))) : originalSoundAt(p,f); }
inline int __fastcall quakeHook(void *p,void *) { return active() ? flagsCrossed(p,field<float>(p,0xa0),4) : originalQuake(p); }
inline int __fastcall quakeAtHook(void *p,void *,float f) { return active() ? flagsCrossed(p,f,4) : originalQuakeAt(p,f); }

// ProcessState decrements pause in the native action routines, before FrameMove.
// The caller surrounds that action dispatch with these helpers. Assigning a new
// hit stop is preserved; an existing one-step decrement consumes legacy time.
inline int capturePause(void *model) { return model ? field<int>(model,0x94) : 0; }
inline void settlePause(void *model,int before) {
    if(!active() || !model || before<=0) return;
    auto &after=field<int>(model,0x94);
    if(after==before-1) after=hybrid_time::frame.legacy>=unsigned(before) ? 0 : before-int(hybrid_time::frame.legacy);
}
inline void advancePhase(void *p,float speed,float delta) {
    const int maximum=field<int>(p,0xa8);
    auto &current=field<float>(p,0xa0),&old=field<float>(p,0xa4);
    if(!std::isfinite(current) || current<0 || maximum<1 || maximum>100000) return;
    old=current;
    if(field<int>(p,0x94) || !std::isfinite(speed) || speed<0 || !std::isfinite(delta) || delta<=0) return;
    const float per=field<float>(p,0xac);
    const double advance=field<int>(p,0x98) ? .1*double(delta) : double(per)*double(speed)*double(delta);
    if(!std::isfinite(advance) || advance<0 || advance>4096) return;
    double next=double(current)+advance;
    const bool loop=(field<int>(p,0xb4)&4)!=0;
    if(next>double(maximum-1)) field<int>(p,0x9c)=1;
    if(loop) {
        const double cycles=std::floor(next/double(maximum));
        next-=cycles*maximum;
        old=float(double(current)-cycles*maximum);
    } else next=std::min(next,double(maximum-1));
    current=float(next);
    field<float>(p,0xb0)=current-std::floor(current);
}
inline void advanceBlend(void *p,float speed,float delta) {
    if(!field<int>(p,0xe4) || !std::isfinite(speed) || !std::isfinite(delta)) return;
    // Shining uses the supplied animation speed here, including normalized time.
    auto &blend=field<float>(p,0xe8);
    if(!std::isfinite(blend)) return;
    blend+=std::max(0.f,speed)*std::max(0.f,delta)*.05f;
    if(blend>=1) { field<int>(p,0xe4)=0;blend=0; }
}
inline void advanceTrails(void *p) {
    // Existing trail reveal counters are integer spline indices. Keep their
    // cadence in the legacy domain; native Process only rebuilds vertex colors.
    const unsigned steps=std::min(hybrid_time::frame.legacy,16u);
    for(size_t at : {size_t(0x10c),size_t(0x110)}) if(auto trail=field<void *>(p,at)) {
        if(!field<int>(p,0x94)) for(unsigned i=0;i<steps;++i) {
            const int maximum=field<int>(trail,4);
            if(maximum<=0) break;
            auto &draw=field<int>(trail,8);
            draw=draw>std::numeric_limits<int>::max()-8 ? maximum : draw+8;
            if(draw>maximum) draw=(field<int>(p,0xb4)&4) ? 0 : maximum;
        }
        if(steps && trailProcess) trailProcess(trail);
    }
}
inline void __fastcall moveHook(void *p,void *,void *soundPosition,float speed,int silent) {
    if(!active()) { originalMove(p,soundPosition,speed,silent);return; }
    const auto attr=attributes(p);
    if(!field<int>(p,0x94)) {
        const float old=field<float>(p,0xa4),current=field<float>(p,0xa0);
        if(!silent && playSound) crossed(attr,old,current,2,[&](Attribute &a) { if(a.sound>0) playSound(a.sound,soundPosition); });
        bool quake=false;crossed(attr,old,current,4,[&](Attribute &) { quake=true; });
        if(quake && setQuake) setQuake(attr.motion);
    }
    const float delta=ticks();
    advancePhase(p,speed,delta);advanceTrails(p);advanceBlend(p,speed,delta);
}
inline void nativeSound(int sound,void *position) {
    // The optimized client entry reads ECX=sound, EDX=position and returns with RET.
    using Play=int(__fastcall *)(int,void *);
    reinterpret_cast<Play>(imageBase+0x45070)(sound,position);
}
inline void nativeQuake(bool motion) {
    if(motion) return; // native motion branch's active-mover exclusion never selects the active mover
    auto player=*reinterpret_cast<void **>(imageBase+0x842168);
    auto camera=*reinterpret_cast<void **>(imageBase+0x847aa8);
    if(player && camera && field<int>(camera,0xe0)<=0) { field<int>(camera,0xe0)=30;field<float>(camera,0xe4)=1; }
}
inline bool replace(void **slot,void *expected,void *value) {
    DWORD protection=0,ignored=0;
    if(!VirtualProtect(slot,sizeof(void *),PAGE_READWRITE,&protection)) return false;
    auto old=InterlockedCompareExchangePointer(slot,value,expected);
    VirtualProtect(slot,sizeof(void *),protection,&ignored);return old==expected;
}
struct Patch { unsigned slot;void *native,*hook; };
inline std::array<Patch,8> patches{};
inline bool install() {
    if(enabled) return true;
    auto image=native_patch::image();client_detail::Image scan(image);
    if(!scan.bytes(0x368500,"55 8B EC 53 56 8B F1 57 83 BE 08 01 00 00 00 0F 84 E5 00 00 00") ||
       !scan.bytes(0x35d870,"55 8B EC 83 B9 94 00 00 00 00 0F 85") ||
       !scan.bytes(0x36ae50,"53 56 F3 0F 2C C1 57 8B D9 F3 0F 2C F2") ||
       !scan.bytes(0x45070,"53 8B DC 83 EC 08 83 E4 F8 83 C4 04 55") ||
       !scan.bytes(0x3697d0,"55 8B EC 83 EC 20 53 56 57 8B F9 33 DB 33 C9") ||
       !scan.bytes(0x35ab60,"55 8B EC 81 EC 04 01 00 00 A1 ?? ?? ?? ?? 33 C5 89 45 FC")) return false;
    auto base=reinterpret_cast<unsigned char *>(image);
    auto candidate=reinterpret_cast<void **>(base+0x7b7f70);
    const unsigned slots[]={0,9,10,11,12,13,14,21};
    const DWORD addresses[]={0x365f30,0x36ad10,0x36af00,0x36b080,0x36acd0,0x36ac90,0x36b000,0x368500};
    void *hooks[]={reinterpret_cast<void *>(&destroyHook),reinterpret_cast<void *>(&hitHook),reinterpret_cast<void *>(&hitAtHook),
        reinterpret_cast<void *>(&soundHook),reinterpret_cast<void *>(&soundAtHook),reinterpret_cast<void *>(&quakeHook),
        reinterpret_cast<void *>(&quakeAtHook),reinterpret_cast<void *>(&moveHook)};
    if(candidate[0]!=base+0x365f30) return false;
    for(size_t i=0;i<8;++i) { if(candidate[slots[i]]!=base+addresses[i]) return false;patches[i]={slots[i],base+addresses[i],hooks[i]}; }
    originalDestroy=reinterpret_cast<Destroy>(candidate[0]);originalHit=reinterpret_cast<Query>(candidate[9]);
    originalHitAt=reinterpret_cast<QueryAt>(candidate[10]);originalSound=reinterpret_cast<Query>(candidate[11]);
    originalSoundAt=reinterpret_cast<QueryAt>(candidate[12]);originalQuake=reinterpret_cast<Query>(candidate[13]);
    originalQuakeAt=reinterpret_cast<QueryAt>(candidate[14]);originalMove=reinterpret_cast<Move>(candidate[21]);
    size_t installed=0;
    for(auto &patch:patches) {
        if(!replace(candidate+patch.slot,patch.native,patch.hook)) {
            while(installed) { auto &undo=patches[--installed];replace(candidate+undo.slot,undo.hook,undo.native); }return false;
        }
        ++installed;
    }
    imageBase=base;table=candidate;trailProcess=reinterpret_cast<TrailProcess>(base+0x3697d0);
    playSound=&nativeSound;setQuake=&nativeQuake;boneEntry=base+0x35ab60;boneSampler=&nativeBones;
    { std::lock_guard lock(boneMutex);boneCache.clear(); }
    enabled=true;
    Log("Native hybrid animation: time-scaled phases/blends and interval events installed.");return true;
}
inline void reset() { std::lock_guard lock(boneMutex);boneCache.clear(); }
inline void uninstall() {
    enabled=false;
    if(table) for(auto it=patches.rbegin();it!=patches.rend();++it) replace(table+it->slot,it->hook,it->native);
    table=nullptr;imageBase=nullptr;boneEntry=nullptr;boneSampler=nullptr;reset();
}
}
