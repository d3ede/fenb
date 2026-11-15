#pragma once
#include "common.hpp"
#include "client_layout.hpp"
#include "native_patch.hpp"
#include "hybrid_time.hpp"
#include "visual_transform.hpp"
#include <array>
#include <cstring>

// The native world keeps its object iteration, link maintenance and deletion order.
// Only the scheduling at its verified call sites changes. All addresses are RVAs
// of the inspected Homesickness ABI and are rejected when the bytes disagree.
namespace native_world {
struct Vec { float x,y,z; };
template<class T> inline T &field(void *p,size_t offset) { return *reinterpret_cast<T *>(static_cast<char *>(p)+offset); }
using Process=void(__thiscall *)(void *);
using Resolve=BOOL(__thiscall *)(void *,uint32_t,void **);
using SetPos=void(__thiscall *)(void *,const Vec *);
using EffectProcess=BOOL(__thiscall *)(void *);
using Cleanup=void(__cdecl *)();
inline Process originalWorld=nullptr,originalWeather=nullptr,originalInitFall=nullptr,originalItem=nullptr,baseObjectProcess=nullptr,originalLua=nullptr;
inline Resolve resolve=nullptr;
inline SetPos setPos=nullptr;
inline EffectProcess originalEffect=nullptr;
inline Cleanup originalCleanup=nullptr;
inline void **worldSlot=nullptr,**playerSlot=nullptr,**cameraSlot=nullptr;
inline void **activePlayerSlot=nullptr,**wndWorldSlot=nullptr;
inline void *nativeCamera=nullptr,*ctrlMap=nullptr,*shootType=nullptr;
inline int *waterCount=nullptr;
inline void **waterDefinitions=nullptr;
inline float *waterFrames=nullptr;
inline int *weatherEnabled=nullptr;
inline native_patch::Set patches;
inline bool active=false;
inline void *observedWorld=nullptr,*observedDevice=nullptr;
inline std::unordered_map<void **,bool> refreshableSfx;
using AfterObject=void (*)(void *);
inline AfterObject afterObject=nullptr;
using SfxRefresh=void (*)(void *);
inline SfxRefresh sfxRefresh=nullptr;
struct Alpha { float value=255;WORD expected=255;void *model=nullptr;uint64_t serial=0; };
inline std::unordered_map<void *,Alpha> alphaHistory;
inline std::vector<void *> followerQueue;
inline bool onThread() {
    return active && hybrid_time::frame.active && hybrid_time::frame.thread==GetCurrentThreadId();
}
inline bool worldStage() { return onThread() && hybrid_time::frame.stage==hybrid_time::Stage::World; }
inline float ticks() {
    const float value=hybrid_time::frame.ticks;
    return std::isfinite(value) ? std::clamp(value,0.f,15.625f) : 0.f;
}
template<class T> inline bool read(const void *p,T &out) {
    SIZE_T n=0;return ReadProcessMemory(GetCurrentProcess(),p,&out,sizeof(out),&n) && n==sizeof(out);
}
inline bool same(Vec a,Vec b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
inline Vec scale(Vec v,float amount) { return {v.x*amount,v.y*amount,v.z*amount}; }
inline void *getWorld() { return worldSlot ? *worldSlot : nullptr; }
inline void *getDevice() { auto *world=getWorld();return world ? field<void *>(world,0x6c) : nullptr; }
inline void *activeWndWorld() { return wndWorldSlot ? *wndWorldSlot : nullptr; }
inline std::array<float,16> rotation(Vec angles) {
    constexpr float radians=-3.14159265358979323846f/360.f;
    const float y=angles.x*radians,p=angles.y*radians,r=angles.z*radians;
    const float cy=std::cos(y),sy=std::sin(y),cp=std::cos(p),sp=std::sin(p),cr=std::cos(r),sr=std::sin(r);
    auto q=visual_transform::normalize({cy*sp*cr+sy*cp*sr,sy*cp*cr-cy*sp*sr,cy*cp*sr-sy*sp*cr,cy*cp*cr+sy*sp*sr});
    return visual_transform::compose({{},{1,1,1},q});
}
inline void itemStep(void *object) {
    baseObjectProcess(object);
    const float amount=ticks();
    auto &angles=field<Vec>(object,0x18);
    angles.x=std::fmod(angles.x+.5f*amount,360.f);if(angles.x<0) angles.x+=360.f;
    auto matrix=rotation(angles);std::memcpy(static_cast<char *>(object)+0xa4,matrix.data(),64);
    field<DWORD>(object,4)|=8u;
    const float ground=field<float>(object,0x318);
    if(ground==0 || !setPos) return;
    auto position=field<Vec>(object,0x160);
    auto &velocity=field<Vec>(object,0x31c);
    if(position.y<=ground) { position.y=ground;velocity={};setPos(object,&position);return; }
    // Fractional extension of the native update (v-=g; y+=v). This expression
    // matches one legacy tick and composes across different render partitions.
    const float displacement=float(double(velocity.y)*amount-.0075*.5*double(amount)*(double(amount)+1.0));
    velocity.y-=.0075f*amount;
    if(velocity.x!=0 || velocity.y!=0 || velocity.z!=0) {
        velocity.x=velocity.z=0;position.y+=displacement;
        if(position.y<=ground && velocity.y<=0) { position.y=ground;velocity={}; }
        setPos(object,&position);
    }
}
inline void updateAlpha(void *object,WORD before) {
    const WORD after=field<WORD>(object,0x2bc);
    if(after==before) return; // Opaque objects and saturated fades need no state.
    const float nativeChange=float(int(after)-int(before));
    float rate=nativeChange<0 ? -10.f : 10.f;
    // Native fades objects behind the player back in by 5; the occlusion branch
    // changes by 10. Reuse native occlusion's decision; no additional ray test.
    if(nativeChange>0 && activePlayerSlot && *activePlayerSlot && cameraSlot && *cameraSlot) {
        const Vec eye=field<Vec>(*cameraSlot,4),player=field<Vec>(*activePlayerSlot,0x160),position=field<Vec>(object,0x160);
        const Vec a={player.x-eye.x,player.y-eye.y,player.z-eye.z},b={position.x-eye.x,position.y-eye.y,position.z-eye.z};
        const float playerDistance=a.x*a.x+a.y*a.y+a.z*a.z,objectDistance=b.x*b.x+b.y*b.y+b.z*b.z;
        if(playerDistance<=objectDistance) rate=5.f;
    } else if(nativeChange==5.f && after<255) rate=5.f;
    auto &state=alphaHistory[object];void *model=field<void *>(object,0x178);
    if(state.model!=model || state.expected!=before ||
       (state.serial!=hybrid_time::frame.serial && state.serial+1!=hybrid_time::frame.serial)) state.value=float(before);
    state.value=std::clamp(state.value+rate*ticks(),rate<0 ? 64.f : 0.f,255.f);
    state.expected=WORD(state.value);state.model=model;state.serial=hybrid_time::frame.serial;
    field<WORD>(object,0x2bc)=state.expected;
}

// Shining adds a no-tick virtual callback to CSfx; CSfxShoot overrides it with a
// no-op. The old client has no such slot. Inspect the native RTTI base list once
// per vtable instead of treating its unrelated slot 0x50 as that new callback.
inline bool mayRefreshSfx(void *object) {
    auto **table=field<void **>(object,0);
    if(auto found=refreshableSfx.find(table);found!=refreshableSfx.end()) return found->second;
    bool allowed=false;
    void *locator=nullptr,*hierarchy=nullptr,*bases=nullptr;
    uint32_t count=0;
    if(table && read(table-1,locator) && locator &&
       read(static_cast<char *>(locator)+16,hierarchy) && hierarchy &&
       read(static_cast<char *>(hierarchy)+8,count) && count>0 && count<=128 &&
       read(static_cast<char *>(hierarchy)+12,bases) && bases) {
        allowed=true;
        for(uint32_t i=0;i<count;++i) {
            void *base=nullptr,*type=nullptr;
            if(!read(static_cast<char *>(bases)+i*sizeof(void *),base) || !base || !read(base,type)) { allowed=false;break; }
            if(type==shootType) { allowed=false;break; }
        }
    }
    refreshableSfx.emplace(table,allowed);return allowed;
}
inline void refreshSfx(void *object) {
    if(!resolve || !setPos || !ctrlMap || !mayRefreshSfx(object)) return;
    const uint32_t destination=field<uint32_t>(object,0x5fc);
    if(destination==UINT32_MAX) return;
    auto &position=field<Vec>(object,0x600);
    void *target=nullptr;
    if(resolve(ctrlMap,destination,&target) && target && !(field<DWORD>(target,4)&1u))
        position=field<Vec>(target,0x160);
    // Invalid destinations retain the last target position, exactly as Shining's
    // added CSfx callback. Lifetime, animation and hit events do not run here.
    setPos(object,&position);
}
inline void __fastcall dispatchHook(void *object,void *) {
    auto process=reinterpret_cast<Process>(field<void **>(object,0)[6]);
    if(!worldStage()) { process(object);return; }
    const DWORD type=field<DWORD>(object,0x170);
    if(type!=3) {
        const WORD alpha=field<WORD>(object,0x2bc);
        if(type==4 && process==originalItem && baseObjectProcess) itemStep(object);else process(object);
        if(type==5) { if(afterObject) afterObject(object); }
        else updateAlpha(object,alpha);
        return;
    }
    const unsigned count=hybrid_time::frame.legacy;
    // The supplied adapter selects each verified subclass's added callback.
    // Unsupported classes retain the reference no-op; never guess a follower
    // policy merely from their shared CSfx prefix.
    if(count) {
        hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1.f,1);
        for(unsigned i=0;i<count;++i) {
            if((field<DWORD>(object,4)&1u) || !field<void *>(object,0x16c)) break;
            process(object);
        }
    }
    // SFX precede movers in the native category order. Reattach after all
    // continuous mover/bone work, while every queued object is still alive.
    if(sfxRefresh) followerQueue.push_back(object);
}
inline void flushFollowers() {
    if(sfxRefresh) for(void *object:followerQueue)
        if(!(field<DWORD>(object,4)&1u) && field<void *>(object,0x16c)==observedWorld) sfxRefresh(object);
    followerQueue.clear();
}
inline void __cdecl postObjectsHook() {
    if(worldStage()) flushFollowers();
    originalCleanup();
}
inline void __fastcall legacyWorldHook(void *world,void *) {
    if(onThread() && hybrid_time::frame.stage==hybrid_time::Stage::Legacy) {
        observedWorld=world;observedDevice=field<void *>(world,0x6c);return;
    }
    originalWorld(world);
}
inline void continuousWorld() {
    if(!onThread() || !originalWorld || !worldSlot || !playerSlot) return;
    followerQueue.clear();
    void *world=*worldSlot,*player=*playerSlot;
    // Reload after legacy packet/UI work: that work may have changed maps or
    // deleted the previous player. Never retain a world pointer across frames.
    if(!world || !player || (field<DWORD>(player,4)&1u) || field<void *>(player,0x16c)!=world) return;
    observedWorld=world;observedDevice=field<void *>(world,0x6c);
    if(alphaHistory.size()>16384) alphaHistory.clear();
    if(cameraSlot) *cameraSlot=nativeCamera;
    hybrid_time::Scope continuous(hybrid_time::Stage::World);
    originalWorld(world);
}

// Native weather motion already contains collision/respawn and splash creation.
// Supplying a velocity for this time interval retains those native decisions.
inline void __fastcall weatherHook(void *sky,void *) {
    if(!worldStage()) { originalWeather(sky);return; }
    // The native Kebaras-world transition calls InitFall before integrating.
    // Perform that same transition before supplying interval-scaled velocities,
    // so the first snow frame also uses dt rather than one complete old tick.
    void *world=field<void *>(sky,0xa0);
    if(weatherEnabled && *weatherEnabled && originalInitFall && world && field<int>(world,0)==2 &&
       field<int>(sky,0x20)!=3 && !field<int>(sky,0x2b038)) {
        field<int>(sky,0x20)=3;originalInitFall(sky);
    }
    const unsigned count=std::min(field<unsigned>(sky,0x2b024),2000u);
    const float amount=ticks();
    std::array<Vec,2000> velocities;
    auto *native=reinterpret_cast<Vec *>(static_cast<char *>(sky)+0xa4);
    for(unsigned i=0;i<count;++i) { velocities[i]=native[i];native[i]=scale(native[i],amount); }
    originalWeather(sky);
    // InitFall may replace these velocities during a weather transition. Restore
    // only values that are still the exact scaled values this callback supplied.
    for(unsigned i=0;i<count;++i) if(same(native[i],scale(velocities[i],amount))) native[i]=velocities[i];
}
inline void waterStep() {
    if(!waterCount || !waterDefinitions || !waterFrames || !*waterDefinitions) return;
    const int count=std::clamp(*waterCount,0,10);
    const float amount=worldStage() ? ticks() : 1.f;
    auto *definitions=static_cast<char *>(*waterDefinitions);
    for(int i=0;i<count;++i) {
        const int maximum=field<int>(definitions,size_t(i)*12);
        const float speed=field<float>(definitions,size_t(i)*12+8);
        if(maximum<=0 || !std::isfinite(speed) || !std::isfinite(waterFrames[i])) continue;
        float value=waterFrames[i]+speed*amount;
        // Preserve this client's asset speed and valid [0,count) phase while
        // retaining fractional remainder across long render intervals.
        if(value>=float(maximum) || value<0) { value=std::fmod(value,float(maximum));if(value<0) value+=float(maximum); }
        waterFrames[i]=value;
    }
}
inline void __cdecl waterHook() { waterStep(); }
inline void __fastcall luaHook(void *manager,void *) {
    if(!worldStage()) { originalLua(manager);return; }
    const unsigned count=hybrid_time::frame.legacy;
    hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1.f,1);
    for(unsigned i=0;i<count;++i) originalLua(manager);
}
inline BOOL __fastcall effectHook(void *model,void *) {
    if(!worldStage()) return originalEffect(model);
    const unsigned count=hybrid_time::frame.legacy;
    hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1.f,1);
    for(unsigned i=0;i<count;++i) if(originalEffect(model)) field<WORD>(model,0x150)=0;
    return FALSE; // The loop has already performed the native end-of-cycle reset.
}

inline bool install() {
    if(active) return true;
    auto image=native_patch::image();client_detail::Image scan(image);
    auto *base=reinterpret_cast<unsigned char *>(image);
    if(!scan.bytes(0x38fc90,"55 8B EC 83 EC 24 A1 ?? ?? ?? ?? 33 C5 89 45 FC 53 56 8B 35 ?? ?? ?? ??") ||
       !scan.bytes(0x357da0,"55 8B EC 83 E4 F8 83 EC 14 A1 ?? ?? ?? ?? 33 C4 89 44 24 10 53 56 57") ||
       !scan.bytes(0x2d7180,"55 8B EC 83 EC 08 53 8B 5D 08 0F B6 C3 35 C5 9D 1C 81") ||
       !scan.bytes(0x32de20,"55 8B EC 83 EC 10 53 56 8B F1 F3 0F 10 86 60 01 00 00") ||
       !scan.bytes(0x388620,"55 8B EC 83 E4 F8 81 EC A8 00 00 00 A1 ?? ?? ?? ?? 33 C4") ||
       !scan.bytes(0x388240,"53 8B DC 83 EC 08 83 E4 F8 83 C4 04 55 8B 6B 04 89 6C 24 04 8B EC 6A FF") ||
       !scan.bytes(0x304f80,"55 8B EC 83 EC 0C 56 8B F1 E8 ?? ?? ?? ?? 83 BE 70 01 00 00 05") ||
       !scan.bytes(0x32d5c0,"55 8B EC 83 EC 2C A1 ?? ?? ?? ?? 33 C5 89 45 FC 56 8B F1") ||
       !scan.bytes(0x354620,"55 8B EC 83 EC 7C A1 ?? ?? ?? ?? 33 C5 89 45 FC 53 8B D9 56") ||
       !scan.bytes(0x352380,"55 8B EC 83 E4 F8 83 EC 6C 53 8B D9 33 D2 56") ||
       !scan.bytes(0x3bdd40,"8B 0D ?? ?? ?? ?? 56 33 F6 85 C9 7E 59") ||
       !scan.bytes(0x837f2c,"2E 3F 41 56 43 53 66 78 53 68 6F 6F 74 40 40 00")) return false;
    originalWorld=reinterpret_cast<Process>(base+0x38fc90);
    originalWeather=reinterpret_cast<Process>(base+0x388620);
    originalInitFall=reinterpret_cast<Process>(base+0x388240);weatherEnabled=reinterpret_cast<int *>(base+0x92674c);
    originalItem=reinterpret_cast<Process>(base+0x304f80);baseObjectProcess=reinterpret_cast<Process>(base+0x32d5c0);
    originalLua=reinterpret_cast<Process>(base+0x354620);
    originalEffect=reinterpret_cast<EffectProcess>(base+0x352380);
    originalCleanup=reinterpret_cast<Cleanup>(base+0x3bdd40);
    resolve=reinterpret_cast<Resolve>(base+0x2d7180);setPos=reinterpret_cast<SetPos>(base+0x32de20);
    ctrlMap=base+0x86c12c;shootType=base+0x837f24;
    worldSlot=reinterpret_cast<void **>(base+0x86c07c);playerSlot=reinterpret_cast<void **>(base+0x841ccc);
    activePlayerSlot=reinterpret_cast<void **>(base+0x842168);wndWorldSlot=reinterpret_cast<void **>(base+0x91d760);
    cameraSlot=reinterpret_cast<void **>(base+0x847aa8);nativeCamera=base+0x869f64;
    waterCount=reinterpret_cast<int *>(base+0x8f854c);waterDefinitions=reinterpret_cast<void **>(base+0x8f8550);
    waterFrames=reinterpret_cast<float *>(base+0x8f8554);
    const unsigned char dispatch[]={0x8b,0x01,0xff,0x50,0x18};
    const unsigned char worldCall[]={0xe8,0xf9,0xf3,0x24,0x00},weatherCall[]={0xe8,0x1f,0x87,0xff,0xff};
    const unsigned char luaCall[]={0xe8,0xb7,0x46,0xfc,0xff};
    const unsigned char effectCall[]={0xe8,0x7e,0x4d,0x02,0x00};
    const unsigned char cleanupCall[]={0xe8,0xcb,0xde,0x02,0x00};
    bool ok=patches.call(base+0x140892,worldCall,reinterpret_cast<void *>(&legacyWorldHook)) &&
        patches.call(base+0x38fd12,dispatch,reinterpret_cast<void *>(&dispatchHook)) &&
        patches.call(base+0x38fe1d,dispatch,reinterpret_cast<void *>(&dispatchHook)) &&
        patches.call(base+0x38fefc,weatherCall,reinterpret_cast<void *>(&weatherHook)) &&
        patches.call(base+0x38ff64,luaCall,reinterpret_cast<void *>(&luaHook)) &&
        patches.call(base+0x32d5fd,effectCall,reinterpret_cast<void *>(&effectHook)) &&
        patches.call(base+0x38fe70,cleanupCall,reinterpret_cast<void *>(&postObjectsHook));
    unsigned char oldWater[10]={0x33,0xd2,0x39,0x15,0,0,0,0,0x7e,0x4b};
    const uint32_t countAddress=uint32_t(reinterpret_cast<uintptr_t>(waterCount));std::memcpy(oldWater+4,&countAddress,4);
    unsigned char newWater[10]={0xe8,0,0,0,0,0xe9};
    const uint32_t callback=uint32_t(reinterpret_cast<uintptr_t>(&waterHook)-reinterpret_cast<uintptr_t>(base+0x38ff08)-5);
    const uint32_t continuation=0x38ff5d-(0x38ff08+10);
    std::memcpy(newWater+1,&callback,4);std::memcpy(newWater+6,&continuation,4);
    ok=ok && patches.add(base+0x38ff08,oldWater,newWater,sizeof(oldWater));
    if(!ok || !patches.apply()) { patches.clear();return false; }
    active=true;Log("FPS native hybrid: continuous world, legacy SFX, destination attachment refresh, delta-time weather and water installed.");return true;
}
inline void uninstall() {
    active=false;patches.clear();refreshableSfx.clear();alphaHistory.clear();followerQueue.clear();observedWorld=nullptr;observedDevice=nullptr;
    worldSlot=playerSlot=cameraSlot=activePlayerSlot=wndWorldSlot=nullptr;nativeCamera=ctrlMap=shootType=nullptr;afterObject=nullptr;sfxRefresh=nullptr;
    waterCount=nullptr;waterDefinitions=nullptr;waterFrames=nullptr;weatherEnabled=nullptr;
}
}
