#pragma once
#include "common.hpp"
#include "native_binding.hpp"
#include "hybrid_time.hpp"
#include "native_patch.hpp"
#include <array>
#include <cstring>

// Integer services inside the continuously called, verified native CMover::Process.
// Each crossed legacy index is visited; modulo work never repeats on zero-step
// frames or disappears when a render frame spans more than one simulation step.
namespace native_mover_legacy {
template<class T> T &field(void *p,size_t at) { return *reinterpret_cast<T *>(static_cast<char *>(p)+at); }
using Process=void(__thiscall *)(void *);
using CheckFile=int(__stdcall *)(const char *);
using DestroyString=void *(__thiscall *)(void *,void *);
using Create=void *(*)(void *,unsigned,void *,int,void *,int,int,int);
inline Process pattern=nullptr,quests=nullptr,pet=nullptr;
inline CheckFile checkFile=nullptr;
inline DestroyString destroyString=nullptr;
inline Create create=nullptr;
inline void *createEntry=nullptr;
inline void **playerSlot=nullptr;
inline unsigned char **queueBegin=nullptr,**queueEnd=nullptr;
inline native_binding::Set patches;
inline bool installed=false;
inline bool active() { return installed && hybrid_time::frame.active && hybrid_time::frame.thread==GetCurrentThreadId() &&
    hybrid_time::frame.stage==hybrid_time::Stage::World; }
inline unsigned steps() { return active() ? hybrid_time::frame.legacy : 1u; }
inline void periodicOnce(void *mover,uint32_t index) {
    if(!field<int>(mover,0x31c)) return;
    if(playerSlot && *playerSlot==mover && !(index&31u) && queueBegin && queueEnd && *queueBegin && *queueEnd &&
       *queueEnd>*queueBegin && size_t(*queueEnd-*queueBegin)>=24 && checkFile && destroyString) {
        auto item=*queueEnd-24;
        const char *path=field<unsigned>(item,20)>15 ? field<const char *>(item,0) : reinterpret_cast<const char *>(item);
        checkFile(path);
        // The native caller reloads vector::end after checking the resource.
        if(*queueEnd>*queueBegin && size_t(*queueEnd-*queueBegin)>=24) {
            item=*queueEnd-24;destroyString(item,item);*queueEnd-=24;
        }
    }
    if(!(index&63u)) {
        if(pet) pet(mover);
        if(playerSlot && *playerSlot==mover) {
            auto **table=field<void **>(mover,0);
            reinterpret_cast<Process>(table[18])(mover);
        }
        auto &counter=field<uint32_t>(mover,0x9f4);if(counter) --counter;
    }
}
inline void __fastcall periodicHook(void *mover,void *) {
    const unsigned count=steps();
    auto &counter=field<uint32_t>(mover,0x11c8);
    const uint32_t start=counter;
    hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1.f,1);
    for(unsigned i=0;i<count;++i) { counter=start+i;periodicOnce(mover,counter); }
    counter=start;
}
inline void __fastcall patternHook(void *mover,void *) {
    const unsigned count=steps();hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1.f,1);
    for(unsigned i=0;i<count;++i) if(pattern) pattern(mover);
}
inline void __fastcall questsHook(void *mover,void *) {
    const unsigned count=steps();hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1.f,1);
    for(unsigned i=0;i<count;++i) if(quests) quests(mover);
}
inline void __fastcall aiHook(void *ai,void *) {
    const unsigned count=steps();hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1.f,1);
    for(unsigned i=0;i<count;++i) reinterpret_cast<Process>(field<void **>(ai,0)[2])(ai);
}
inline void __fastcall attackCountHook(void *mover,void *) {
    auto &counter=field<int>(mover,0x720);
    if(counter>0) field<uint32_t>(mover,0x720)+=steps();
}
inline void __fastcall processCountHook(void *mover,void *) { field<uint32_t>(mover,0x11c8)+=steps(); }
struct RipplePlan { void *mover=nullptr;uint64_t serial=0;unsigned events=0;unsigned char after=0; };
inline thread_local RipplePlan ripple;
inline void __fastcall waterCountHook(void *mover,void *) {
    auto &counter=field<unsigned char>(mover,0xd1c);
    if(ripple.mover==mover && ripple.serial==hybrid_time::frame.serial) { counter=ripple.after;ripple={}; }
    else counter=static_cast<unsigned char>(unsigned(counter)+steps());
}
inline bool __fastcall waterReadyHook(void *mover,void *) {
    unsigned counter=field<unsigned char>(mover,0xd1c),events=0;
    for(unsigned i=0,count=steps();i<count;++i) {
        if(counter>7) { ++events;counter=0; }
        counter=(counter+1)&255u;
    }
    ripple={mover,hybrid_time::frame.serial,events,static_cast<unsigned char>(counter)};
    return events!=0;
}
inline void *nativeCreate(void *device,unsigned id,void *position,int source,void *angles,int target,int control,int addToWorld) {
    void *result=nullptr;
#if FENB_NATIVE_X86_ASM
    // Verified optimized CreateSfx: ECX=device, EDX=id, six caller-cleaned stack args.
    __asm {
        push addToWorld
        push control
        push target
        push angles
        push source
        push position
        mov ecx,device
        mov edx,id
        call createEntry
        add esp,24
        mov result,eax
    }
#endif
    return result;
}
inline void *__cdecl rippleRelay(void *device,unsigned id,void *mover,void *,void *position,int source,void *angles,int target,int control,int add) {
    const unsigned count=ripple.mover==mover && ripple.serial==hybrid_time::frame.serial ? ripple.events : 1u;
    void *result=nullptr;
    for(unsigned i=0;i<count;++i) if(create) result=create(device,id,position,source,angles,target,control,add);
    return result;
}
#if FENB_NATIVE_X86_ASM
// These sites originally used EDI for the mover and did not pass it in ECX.
// Preserve every live integer register; water readiness also retains all volatile
// SSE registers because native water height is still held in XMM2 after its CMP.
__declspec(naked) inline void processCountThunk() {
    __asm {
        pushad
        mov ecx,edi
        call processCountHook
        popad
        ret
    }
}
__declspec(naked) inline void waterCountThunk() {
    __asm {
        pushad
        mov ecx,edi
        call waterCountHook
        popad
        ret
    }
}
__declspec(naked) inline void waterReadyThunk() {
    __asm {
        pushad
        sub esp,96
        movups [esp],xmm0
        movups [esp+16],xmm1
        movups [esp+32],xmm2
        movups [esp+48],xmm3
        movups [esp+64],xmm4
        movups [esp+80],xmm5
        mov ecx,edi
        call waterReadyHook
        mov [esp+124],eax
        movups xmm0,[esp]
        movups xmm1,[esp+16]
        movups xmm2,[esp+32]
        movups xmm3,[esp+48]
        movups xmm4,[esp+64]
        movups xmm5,[esp+80]
        add esp,96
        popad
        ret
    }
}
__declspec(naked) inline void rippleThunk() {
    __asm {
        push edi
        push edx
        push ecx
        call rippleRelay
        add esp,12
        ret
    }
}
#endif
inline bool callbackSpan(unsigned char *at,const unsigned char *expected,size_t size,void *callback,bool moverInEcx=false) {
    if(size<5+(moverInEcx ? 2u : 0u)) return false;
    std::vector<unsigned char> code(size,0x90);
    size_t call=0;
    if(moverInEcx) { code[0]=0x8b;code[1]=0xcf;call=2; }
    code[call]=0xe8;
    const uint32_t target=uint32_t(reinterpret_cast<uintptr_t>(callback)-reinterpret_cast<uintptr_t>(at+call)-5);
    std::memcpy(code.data()+call+1,&target,4);return patches.add(at,expected,code.data(),size);
}
inline bool install() {
    if(installed) return true;
#if !FENB_NATIVE_X86_ASM
    return false;
#else
    auto image=native_patch::image();native_binding::Image scan(image);
    if(!scan.bytes(0x3098d0,"55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 81 EC D0 01 00 00") ||
       !scan.bytes(0x31e1c0,"56 8B F1 8B 86 A8 11 00 00 83 F8 01 0F 85") ||
       !scan.bytes(0x307e20,"55 8B EC 83 EC 10 53 8B D9 89 5D FC") ||
       !scan.bytes(0x310540,"55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00") ||
       !scan.bytes(0x3a6c70,"55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00") ||
       !scan.bytes(0x174ac0,"56 8B F1 8B 4E 14 83 F9 0F 76") ||
       !scan.bytes(0x2fce40,"55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00")) return false;
    pattern=reinterpret_cast<Process>(scan.address(0x31e1c0));quests=reinterpret_cast<Process>(scan.address(0x307e20));
    pet=reinterpret_cast<Process>(scan.address(0x310540));checkFile=reinterpret_cast<CheckFile>(scan.address(0x3a6c70));
    destroyString=reinterpret_cast<DestroyString>(scan.address(0x174ac0));createEntry=scan.address(0x2fce40);create=&nativeCreate;
    playerSlot=reinterpret_cast<void **>(scan.address(0x842168));
    queueBegin=reinterpret_cast<unsigned char **>(scan.address(0x86c0a8));queueEnd=reinterpret_cast<unsigned char **>(scan.address(0x86c0ac));
    const unsigned char periodicOld[]={0x83,0xbf,0x1c,0x03,0,0,0,0x0f,0x84,0x83,0,0,0};
    unsigned char periodicNew[]={0x8b,0xcf,0xe8,0,0,0,0,0xe9,0,0,0,0,0x90};
    const uint32_t periodicCall=uint32_t(reinterpret_cast<uintptr_t>(&periodicHook)-reinterpret_cast<uintptr_t>(scan.address(0x309922))-5);
    const uint32_t periodicNext=uint32_t(scan.address(0x3099b0)-(scan.address(0x309920)+12));
    std::memcpy(periodicNew+3,&periodicCall,4);std::memcpy(periodicNew+8,&periodicNext,4);
    const unsigned char patternCall[]={0xe8,0xf5,0x46,1,0},questCall[]={0xe8,0xc3,0xd7,0xff,0xff},aiCall[]={0x8b,1,0xff,0x50,8};
    const unsigned char attackOld[]={0x8b,0x87,0x20,7,0,0,0x85,0xc0,0x7e,7,0x40,0x89,0x87,0x20,7,0,0};
    const unsigned char countOld[]={0xff,0x87,0xc8,0x11,0,0},waterCountOld[]={0xfe,0x87,0x1c,0x0d,0,0};
    const unsigned char waterReadyOld[]={0x80,0xbf,0x1c,0x0d,0,0,7},rippleCall[]={0xe8,0x7d,0x2e,0xff,0xff};
    unsigned char waterReadyNew[]={0xe8,0,0,0,0,0x84,0xc0};
    const uint32_t waterReadyTarget=uint32_t(reinterpret_cast<uintptr_t>(&waterReadyThunk)-reinterpret_cast<uintptr_t>(scan.address(0x309f51))-5);
    std::memcpy(waterReadyNew+1,&waterReadyTarget,4);
    patches.clear();
    bool ok=patches.add(scan.address(0x309920),periodicOld,periodicNew,sizeof(periodicOld)) &&
        patches.call(scan.address(0x309ac6),patternCall,reinterpret_cast<void *>(&patternHook)) &&
        patches.call(scan.address(0x30a658),questCall,reinterpret_cast<void *>(&questsHook)) &&
        patches.call(scan.address(0x3099ca),aiCall,reinterpret_cast<void *>(&aiHook)) &&
        callbackSpan(scan.address(0x309c48),attackOld,sizeof(attackOld),reinterpret_cast<void *>(&attackCountHook),true) &&
        callbackSpan(scan.address(0x30a7ac),countOld,sizeof(countOld),reinterpret_cast<void *>(&processCountThunk)) &&
        callbackSpan(scan.address(0x309fcd),waterCountOld,sizeof(waterCountOld),reinterpret_cast<void *>(&waterCountThunk)) &&
        patches.add(scan.address(0x309f51),waterReadyOld,waterReadyNew,sizeof(waterReadyOld)) &&
        patches.call(scan.address(0x309fbe),rippleCall,reinterpret_cast<void *>(&rippleThunk));
    if(!ok || !patches.apply()) { patches.clear();return false; }
    installed=true;Log("Native hybrid mover services: legacy patterns/AI, periodic work and water/attack counters installed.");return true;
#endif
}
inline void reset() { ripple={}; }
inline void uninstall() { installed=false;patches.clear();reset(); }
}
