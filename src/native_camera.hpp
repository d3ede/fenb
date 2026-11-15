#pragma once
#include "native_binding.hpp"
#include "hybrid_time.hpp"
#include "native_patch.hpp"
#include <limits>

// Native camera integration for the verified Homesickness x86 ABI. Camera state
// remains the engine's current state; no delayed or predicted pose is installed.
namespace native_camera {
using Process=void(__thiscall *)(void *,void *,float);
using Transform=void(__thiscall *)(void *,void *,void *);
using Flying=int(__thiscall *)(void *,DWORD,POINT);
using Factory=void *(__cdecl *)();
using Initialize=void(__cdecl *)();
using AtExit=int(__cdecl *)(void(__cdecl *)());
inline Process originalProcess=nullptr;
inline Transform originalTransform=nullptr;
inline Flying originalFlying=nullptr;
inline Factory guildDeploy=nullptr;
inline Initialize initializeDeploy=nullptr;
inline AtExit nativeAtExit=nullptr;
inline Initialize destroyDeploy=nullptr;
inline void **nativePlayer=nullptr,**nativeWorld=nullptr,**nativeCamera=nullptr;
inline void **inputPlayer=nullptr;
inline unsigned char *keys=nullptr;
inline int *cameraLock=nullptr;
inline signed char *keyUp=nullptr,*keyLeft=nullptr;
inline unsigned *deployFlags=nullptr;
inline void **deployObject=nullptr;
inline void *embeddedCamera=nullptr;
inline bool installed=false;
inline native_binding::Set patches;
// Native instructions reference these private operands. Scoped changes preserve
// native initialization/setup calls and never edit shared executable constants.
inline float impulseCoefficient=.5f,recoveryStep=.37f,rollStep=1,rollRecoveryStep=2;
template<class T> T &field(void *p,size_t offset) { return *reinterpret_cast<T *>(static_cast<unsigned char *>(p)+offset); }
inline bool onThread() { return installed && hybrid_time::frame.active && GetCurrentThreadId()==hybrid_time::frame.thread; }
inline float boundedTicks(float ticks) { return std::isfinite(ticks) ? std::clamp(ticks,0.f,15.625f) : 0.f; }
inline float impulseDecay(float ticks) { return std::pow(.5f,boundedTicks(ticks)); }
inline float followGain(float filteredTicks,bool flying) { return std::min(boundedTicks(filteredTicks),4.f)*(flying ? .0625f : .25f); }
inline int quakeRemaining(int before,unsigned steps) { return before>0 && unsigned(before)>steps ? before-int(steps) : 0; }
struct State {
    void *player=nullptr,*world=nullptr;
    float filteredTicks=1;
    uint64_t processedSerial=std::numeric_limits<uint64_t>::max();
    uint64_t quakeSerial=std::numeric_limits<uint64_t>::max();
    bool filterReady=false;
};
inline std::unordered_map<void *,State> states;
inline void reset() { states.clear();impulseCoefficient=.5f;recoveryStep=.37f;rollStep=1;rollRecoveryStep=2; }
struct Coefficients {
    float &a,&b,oldA,oldB;
    Coefficients(float &first,float &second,float nextA,float nextB):a(first),b(second),oldA(first),oldB(second) { a=nextA;b=nextB; }
    ~Coefficients() { a=oldA;b=oldB; }
    Coefficients(const Coefficients&)=delete;Coefficients&operator=(const Coefficients&)=delete;
};
inline bool flying(void *player) {
    if(!player) return false;
    auto *action=field<void *>(player,0x324);
    return action && (field<unsigned>(action,8)&8)!=0;
}
inline void __fastcall processHook(void *camera,void *,void *device,float factor) {
    if(!originalProcess) return;
    if(!onThread()) { originalProcess(camera,device,factor);return; }
    const auto &frame=hybrid_time::frame;
    if(frame.stage==hybrid_time::Stage::Legacy || frame.stage==hybrid_time::Stage::World) return;
    if(frame.stage!=hybrid_time::Stage::Camera) { originalProcess(camera,device,factor);return; }
    const float ticks=boundedTicks(frame.ticks);
    if(!camera || ticks<=0) return;
    auto &state=states[camera];
    void *player=nativePlayer ? *nativePlayer : nullptr;
    void *world=nativeWorld ? *nativeWorld : nullptr;
    if(state.player!=player || state.world!=world || frame.serial<state.processedSerial) state=State{};
    if(state.processedSerial==frame.serial) return;
    state.player=player;state.world=world;
    // Match Shining's camera delta filter and clamped follow gain. Recovery and
    // impulse lifetime use elapsed time, not this visual follow filter.
    state.filteredTicks=state.filterReady ? (state.filteredTicks+ticks)*.5f : ticks;
    state.filterReady=true;state.processedSerial=frame.serial;
    const float gain=followGain(state.filteredTicks,flying(player));
    if(gain<=0) return;
    Coefficients timing(impulseCoefficient,recoveryStep,impulseDecay(ticks),.37f*ticks);
    originalProcess(camera,device,1.f/gain);
}
inline void process(void *camera,void *device) {
    hybrid_time::Scope scope(hybrid_time::Stage::Camera);
    processHook(camera,nullptr,device,4.f);
}
inline void processCurrent(void *device) { if(nativeCamera && *nativeCamera) process(*nativeCamera,device); }
inline void __fastcall transformHook(void *camera,void *,void *device,void *world) {
    if(!originalTransform) return;
    if(!onThread() || !camera) { originalTransform(camera,device,world);return; }
    auto &state=states[camera];
    const int before=field<int>(camera,0xe0);
    originalTransform(camera,device,world);
    if(before>0) {
        const auto serial=hybrid_time::frame.serial;
        const unsigned consume=state.quakeSerial==serial ? 0 : hybrid_time::frame.legacy;
        field<int>(camera,0xe0)=quakeRemaining(before,consume);
        state.quakeSerial=serial;
    }
}
inline int __fastcall flyingHook(void *window,void *,DWORD message,POINT point) {
    if(!originalFlying) return 0;
    if(!onThread()) return originalFlying(window,message,point);
    // Native WM_MOUSEMOVE also visits held-key roll code. Its pixel displacement
    // is already integrated; only the once-per-frame input pass advances roll.
    const float ticks=hybrid_time::frame.stage==hybrid_time::Stage::Input && message==0 ? boundedTicks(hybrid_time::frame.ticks) : 0.f;
    Coefficients timing(rollStep,rollRecoveryStep,ticks,2.f*ticks);
    return originalFlying(window,message,point);
}
struct Keyboard { bool left=false,right=false,up=false,down=false,pageUp=false,pageDown=false; };
struct Orbit { float yaw=0,pitch=0,zoom=0; };
inline Orbit keyboardStep(Orbit value,const Keyboard &input,float ticks,bool allowZoom) {
    ticks=boundedTicks(ticks);
    auto wrap=[&]() { if(value.yaw>360) value.yaw-=360;if(value.yaw<0) value.yaw+=360; };
    if(input.left) { value.yaw-=4*ticks;wrap(); }
    if(input.right) { value.yaw+=4*ticks;wrap(); }
    auto limitPitch=[&]() { value.pitch=std::min(value.pitch,80.f-value.zoom*4);value.pitch=std::max(value.pitch,-80.f); };
    if(input.up) { value.pitch+=2*ticks;limitPitch(); }
    if(input.down) { value.pitch-=2*ticks;limitPitch(); }
    // Preserve native ordering and clamps even while housing blocks zoom.
    if(input.pageUp) { if(allowZoom) value.zoom-=.6f*ticks;value.zoom=std::max(value.zoom,0.f); }
    if(input.pageDown) { if(allowZoom) value.zoom+=.6f*ticks;value.zoom=std::min(value.zoom,7.f); }
    return value;
}
inline bool nativeZoomAllowed() {
    if(!deployFlags || !deployObject || !guildDeploy || !initializeDeploy || !nativeAtExit || !destroyDeploy) return false;
    // This is the native inline singleton initialization, including its CRT
    // destructor registration, normally reached by the old held-key block.
    if(!(*deployFlags&1)) { *deployFlags|=1;initializeDeploy();nativeAtExit(destroyDeploy); }
    if(*deployObject) return false;
    void *guild=guildDeploy();
    return guild && field<int>(guild,0x30)==0;
}
inline void applyKeyboard(float ticks) {
    if(!onThread() || !keys || !embeddedCamera || !nativePlayer || !*nativePlayer) return;
    const Keyboard input{keys[VK_LEFT]!=0,keys[VK_RIGHT]!=0,keys[VK_UP]!=0,keys[VK_DOWN]!=0,keys[VK_PRIOR]!=0,keys[VK_NEXT]!=0};
    Orbit value{field<float>(embeddedCamera,0xd0),field<float>(embeddedCamera,0xd4),field<float>(embeddedCamera,0xfc)};
    if(!std::isfinite(value.yaw) || !std::isfinite(value.pitch) || !std::isfinite(value.zoom)) return;
    value=keyboardStep(value,input,ticks,!(input.pageUp||input.pageDown) || nativeZoomAllowed());
    field<float>(embeddedCamera,0xd0)=value.yaw;field<float>(embeddedCamera,0xd4)=value.pitch;field<float>(embeddedCamera,0xfc)=value.zoom;
}
inline void applyKeyboard() { applyKeyboard(hybrid_time::frame.ticks); }
inline void applyKeyboard(void *window,float ticks) {
    if(onThread() && window && keys && inputPlayer && *inputPlayer && cameraLock && keyUp && keyLeft &&
       *cameraLock && !field<unsigned char>(window,0x46) &&
       (keys[static_cast<unsigned char>(*keyUp)] || keys['S'] || keys[static_cast<unsigned char>(*keyLeft)] || keys['D']) &&
       !flying(*inputPlayer)) {
        field<float>(embeddedCamera,0xd0)=-field<float>(*inputPlayer,0x18);
    }
    applyKeyboard(ticks);
}
inline void applyKeyboard(void *window) { applyKeyboard(window,hybrid_time::frame.ticks); }
inline void uninstall() {
    installed=false;patches.clear();reset();
    originalProcess=nullptr;originalTransform=nullptr;originalFlying=nullptr;
    nativePlayer=nullptr;nativeWorld=nullptr;nativeCamera=nullptr;keys=nullptr;
    inputPlayer=nullptr;cameraLock=nullptr;keyUp=nullptr;keyLeft=nullptr;
    embeddedCamera=nullptr;deployFlags=nullptr;deployObject=nullptr;
    guildDeploy=nullptr;initializeDeploy=nullptr;nativeAtExit=nullptr;destroyDeploy=nullptr;
}
inline bool install() {
    if(installed) return true;
    HMODULE module=native_patch::image();native_binding::Image scan(module);
    if(!scan.bytes(0x386050,"55 8B EC 83 E4 F0 81 EC 98 00 00 00 A1 ?? ?? ?? ?? 33 C4") ||
       !scan.bytes(0x385cb0,"55 8B EC 83 EC 2C A1 ?? ?? ?? ?? 33 C5 89 45 FC 53 8B D9 56 8B 75 08 57") ||
       !scan.bytes(0x151330,"55 8B EC 83 E4 F8 83 EC 50 A1 ?? ?? ?? ?? 33 C4") ||
       !scan.bytes(0x2a4290,"55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 83 EC 14") ||
       !scan.bytes(0xd5e00,"55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 51") ||
       !scan.bytes(0x6d5e90,"83 3D ?? ?? ?? ?? 00 74 21 6A 02") ||
       !scan.bytes(0x1400db,"83 3D ?? ?? ?? ?? 00 74 5E 80 7E 46 00 75 58 0F BE 05") ||
       !scan.bytes(0x140dbb,"80 3D ?? ?? ?? ?? 00 F3 0F 10 2D ?? ?? ?? ?? F3 0F 10 0D")) return false;
    if(scan.relative(scan.value(0x38606b))!=0x842168 || scan.relative(scan.value(0x386081))!=0x86c07c ||
       scan.relative(scan.value(0x140dbd))!=0x841d35 || scan.relative(scan.value(0x140eff))!=0x9b0e8c ||
       scan.relative(scan.value(0x140f30))!=0x9b0ed4) return false;
    auto **table=reinterpret_cast<void **>(scan.address(0x7b8cf4));
    if(table[0]!=scan.address(0x386050) || table[1]!=scan.address(0x385cb0)) return false;
    originalProcess=reinterpret_cast<Process>(table[0]);originalTransform=reinterpret_cast<Transform>(table[1]);
    originalFlying=reinterpret_cast<Flying>(scan.address(0x151330));
    auto operand=[&](DWORD rva,const char *opcode,DWORD constant,float *replacement) {
        return scan.bytes(rva,opcode) && scan.relative(scan.value(rva,4))==constant &&
            patches.pointer(reinterpret_cast<void **>(scan.address(rva)+4),scan.address(constant),replacement);
    };
    auto call=[&](DWORD rva,void *target) {
        unsigned char expected[5]={0xe8};
        const uint32_t displacement=uint32_t(0x151330-rva-5);std::memcpy(expected+1,&displacement,4);
        return patches.call(scan.address(rva),expected,target);
    };
    const bool prepared=operand(0x3862ed,"F3 0F 10 0D",0x7b9d4c,&impulseCoefficient) &&
        operand(0x386462,"F3 0F 58 05",0x7b9d30,&recoveryStep) &&
        operand(0x151a08,"F3 0F 5C 05",0x7b9da0,&rollStep) &&
        operand(0x151a6b,"F3 0F 58 05",0x7b9da0,&rollStep) &&
        operand(0x151ac1,"F3 0F 58 05",0x7b9e78,&rollRecoveryStep) &&
        operand(0x151add,"F3 0F 5C 05",0x7b9e78,&rollRecoveryStep) &&
        call(0x13f4ac,reinterpret_cast<void *>(&flyingHook)) && call(0x150702,reinterpret_cast<void *>(&flyingHook)) &&
        patches.pointer(table,reinterpret_cast<void *>(originalProcess),reinterpret_cast<void *>(&processHook)) &&
        patches.pointer(table+1,reinterpret_cast<void *>(originalTransform),reinterpret_cast<void *>(&transformHook));
    if(!prepared || !patches.apply()) { uninstall();return false; }
    nativePlayer=reinterpret_cast<void **>(scan.address(0x842168));nativeWorld=reinterpret_cast<void **>(scan.address(0x86c07c));
    nativeCamera=reinterpret_cast<void **>(scan.address(0x847aa8));embeddedCamera=scan.address(0x869f64);keys=scan.address(0x841d10);
    inputPlayer=reinterpret_cast<void **>(scan.address(0x841ccc));cameraLock=reinterpret_cast<int *>(scan.address(0x926844));
    keyUp=reinterpret_cast<signed char *>(scan.address(0x86bd9d));keyLeft=reinterpret_cast<signed char *>(scan.address(0x86bd9c));
    deployFlags=reinterpret_cast<unsigned *>(scan.address(0x9b0e8c));deployObject=reinterpret_cast<void **>(scan.address(0x9b0ed4));
    guildDeploy=reinterpret_cast<Factory>(scan.address(0xd5e00));initializeDeploy=reinterpret_cast<Initialize>(scan.address(0x2a4290));
    nativeAtExit=reinterpret_cast<AtExit>(scan.address(0x5a6b00));destroyDeploy=reinterpret_cast<Initialize>(scan.address(0x6d5e90));
    reset();installed=true;return true;
}
}
