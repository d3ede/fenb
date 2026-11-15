#pragma once
#include "common.hpp"
#include "client_layout.hpp"
#include "native_patch.hpp"
#include "hybrid_time.hpp"
#include "native_world.hpp"
#include <ctime>
#include <mmsystem.h>

// Split native application services from the continuous world update. The main
// FPS adapter owns the outer clock and calls these stages in reference order.
namespace native_app {
template<class T> inline T &field(void *p,size_t at) { return *reinterpret_cast<T *>(static_cast<char *>(p)+at); }
using Receive=void(__thiscall *)(void *);
using Control=int(__thiscall *)(void *,int,int,int);
using PreviewMove=void(__thiscall *)(void *,void *,float,int);
using TimerExpired=BOOL(__thiscall *)(void *);
inline Receive originalReceive=nullptr;
inline Control originalControl=nullptr;
inline void *networkClients[3]{};
inline DWORD *nativeMilliseconds=nullptr,*nativeSeconds=nullptr;
inline DWORD *processCounter=nullptr;
inline void **previewSlot=nullptr,**playerSlot=nullptr;
inline native_patch::Set patches;
inline bool active=false,hasReceived=false;
inline uint64_t receivedSerial=0;
inline bool onThread() { return active && hybrid_time::frame.active && hybrid_time::frame.thread==GetCurrentThreadId(); }
inline void updateClocks() {
    if(!onThread()) return;
    if(nativeMilliseconds) *nativeMilliseconds=timeGetTime();
    if(nativeSeconds) *nativeSeconds=DWORD(std::time(nullptr));
}
inline void receiveAll() {
    if(!onThread() || !originalReceive || (hasReceived && receivedSerial==hybrid_time::frame.serial)) return;
    hasReceived=true;receivedSerial=hybrid_time::frame.serial;
    for(void *client:networkClients) if(client) originalReceive(client);
}
inline void __fastcall receiveHook(void *client,void *) {
    if(onThread() && hybrid_time::frame.stage==hybrid_time::Stage::Legacy) { receiveAll();return; }
    originalReceive(client);
}
inline void previewCall(void *model,void *sound,float speed,int extra) {
    if(onThread() && hybrid_time::frame.stage==hybrid_time::Stage::Legacy) return;
    auto move=reinterpret_cast<PreviewMove>(field<void **>(model,0)[21]);
    move(model,sound,speed,extra);
}
// Replace only PUSH 0 (sound) + the three-byte virtual call. The caller has
// already pushed speed and extra. Fastcall consumes those two values (RET 8),
// while the native CMP and its incoming null-model branch remain intact.
inline void __fastcall previewNoSound(void *model,void *,float speed,int extra) {
    previewCall(model,nullptr,speed,extra);
}
inline void preview() {
    if(!onThread() || !previewSlot || !*previewSlot) return;
    hybrid_time::Scope continuous(hybrid_time::Stage::World);
    previewCall(*previewSlot,nullptr,1.f,0);
}

// This is the exact predicate selecting native ControlPlayer in CWndWorld's
// original Process. Other branches issue skills, auto-attacks or item commands;
// those branches remain in the legacy UI path and must not be cancelled here.
inline bool permitsHeldInput(void *window,void *player,void *world) {
    if(!window || !player || !world || (field<DWORD>(player,4)&1u) || field<void *>(player,0x16c)!=world) return false;
    auto *action=field<void *>(player,0x324);
    if(!action) return false;
    const bool left=field<unsigned char>(window,0x45)!=0,selected=field<DWORD>(window,0x1090)!=0;
    if(field<DWORD>(action,8)&0x108u) return !(left && selected);
    if(field<DWORD>(window,0x164c)!=UINT32_MAX) return false;
    const bool autoAttack=field<DWORD>(window,0x2000)!=0;
    if((left || autoAttack) && field<DWORD>(player,0x1180)!=5) {
        bool expired=false;
        if(selected && !autoAttack) {
            void *timer=static_cast<char *>(window)+0x1148;
            auto timeout=reinterpret_cast<TimerExpired>(field<void **>(timer,0)[2]);
            expired=timeout(timer)==TRUE;
        }
        if(!(autoAttack || (selected && expired))) return false;
        return field<void *>(world,0x20)==nullptr;
    }
    return true;
}
inline int __fastcall controlHook(void *window,void *,int flags,int mouseX,int mouseY) {
    if(onThread() && hybrid_time::frame.stage==hybrid_time::Stage::Legacy) return 0;
    return originalControl(window,flags,mouseX,mouseY);
}
inline void heldInput() {
    if(!onThread() || !originalControl || !playerSlot) return;
    void *window=native_world::activeWndWorld(),*world=native_world::getWorld(),*player=*playerSlot;
    if(!permitsHeldInput(window,player,world)) return;
    hybrid_time::Scope input(hybrid_time::Stage::Input);
    originalControl(window,0,field<int>(window,0xa8),field<int>(window,0xac));
}
inline bool install() {
    if(active) return true;
#if !defined(_MSC_VER) || !defined(_M_IX86)
    return false;
#else
    auto image=native_patch::image();client_detail::Image scan(image);
    auto *base=reinterpret_cast<unsigned char *>(image);
    if(!scan.bytes(0x3c3510,"55 8B EC 83 EC 08 53 8B D9 83 7B 04 00 75 07") ||
       !scan.bytes(0x1506c0,"55 8B EC 83 E4 F8 56 8B 35 ?? ?? ?? ?? 57 8B F9 85 F6")) return false;
    originalReceive=reinterpret_cast<Receive>(base+0x3c3510);originalControl=reinterpret_cast<Control>(base+0x1506c0);
    networkClients[0]=base+0x868f90;networkClients[1]=base+0x82cc70;networkClients[2]=base+0x82a620;
    nativeMilliseconds=reinterpret_cast<DWORD *>(base+0x841cd0);nativeSeconds=reinterpret_cast<DWORD *>(base+0x841cd4);
    processCounter=reinterpret_cast<DWORD *>(base+0x841d04);
    previewSlot=reinterpret_cast<void **>(base+0x841e10);playerSlot=reinterpret_cast<void **>(base+0x841ccc);
    const unsigned char receive0[]={0xe8,0xf8,0x8c,0x2e,0x00},receive1[]={0xe8,0x8c,0x8c,0x2e,0x00},receive2[]={0xe8,0x82,0x8c,0x2e,0x00};
    const unsigned char control0[]={0xe8,0x22,0xfd,0x00,0x00},control1[]={0xe8,0xc8,0xfc,0x00,0x00};
    bool ok=patches.call(base+0xda813,receive0,reinterpret_cast<void *>(&receiveHook)) &&
        patches.call(base+0xda87f,receive1,reinterpret_cast<void *>(&receiveHook)) &&
        patches.call(base+0xda889,receive2,reinterpret_cast<void *>(&receiveHook)) &&
        patches.call(base+0x140999,control0,reinterpret_cast<void *>(&controlHook)) &&
        patches.call(base+0x1409f3,control1,reinterpret_cast<void *>(&controlHook));
    const unsigned char oldPreview[]={0x6a,0,0xff,0x50,0x54};
    ok=ok && patches.call(base+0xdabd0,oldPreview,reinterpret_cast<void *>(&previewNoSound));
    unsigned char oldKeyboard[7]={0x80,0x3d,0,0,0,0,0},newKeyboard[7]={0xe9,0,0,0,0,0x90,0x90};
    const uint32_t keyAddress=uint32_t(reinterpret_cast<uintptr_t>(base+0x841d35)),keyboardOffset=0x140ff0-(0x140dbb+5);
    std::memcpy(oldKeyboard+2,&keyAddress,4);std::memcpy(newKeyboard+1,&keyboardOffset,4);
    ok=ok && patches.add(base+0x140dbb,oldKeyboard,newKeyboard,7);
    if(!ok || !patches.apply()) { patches.clear();return false; }
    hasReceived=false;active=true;Log("FPS native hybrid: per-render receive/input/preview services separated from legacy UI, counters and effects.");return true;
#endif
}
inline void uninstall() {
    active=false;patches.clear();hasReceived=false;receivedSerial=0;
    for(auto &client:networkClients) client=nullptr;
    nativeMilliseconds=nativeSeconds=processCounter=nullptr;previewSlot=playerSlot=nullptr;
}
}
