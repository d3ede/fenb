#include "native_app.hpp"
#include <cassert>
#include <cstdio>

void Log(const char *) {}
namespace {
using namespace native_app;
int receives=0,controls=0,previews=0,timeouts=0;
bool expired=true;
void *expectedWindow=nullptr;
void __thiscall receive(void *) { ++receives; }
int __thiscall control(void *window,int flags,int x,int y) {
    ++controls;assert(window==expectedWindow && flags==0 && x==123 && y==456);
    assert(hybrid_time::frame.stage==hybrid_time::Stage::Input);return 7;
}
void __thiscall move(void *,void *sound,float speed,int extra) {
    ++previews;assert(!sound && speed==1 && extra==0 && hybrid_time::frame.stage==hybrid_time::Stage::World);
}
BOOL __thiscall timeout(void *) { ++timeouts;return expired; }
#if FENB_NATIVE_X86_ASM
using Flow=int(__cdecl *)();
struct StackPair { DWORD before,after; };
// Keep raw ABI verification outside any C++ scope/local stack allocation. This
// wrapper explicitly preserves nonvolatile registers and reports ESP around the
// executable fixture's call, while the caller uses the declared fastcall ABI.
__declspec(naked) int __fastcall executeFlow(Flow,StackPair *) {
    __asm {
        push ebx
        push esi
        push edi
        mov esi,esp
        mov edi,edx
        call ecx
        mov dword ptr [edi],esi
        mov dword ptr [edi+4],esp
        pop edi
        pop esi
        pop ebx
        ret
    }
}
#endif
}
int main() {
    using namespace native_app;
    active=true;hybrid_time::frame={.ticks=.25f,.legacy=0,.serial=1,.thread=GetCurrentThreadId(),.active=true};
    originalReceive=&receive;
    networkClients[0]=reinterpret_cast<void *>(1);networkClients[1]=reinterpret_cast<void *>(2);networkClients[2]=reinterpret_cast<void *>(3);
    receiveAll();receiveAll();assert(receives==3);
    { hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1,1);for(void *client:networkClients) receiveHook(client,nullptr); }
    assert(receives==3);++hybrid_time::frame.serial;receiveAll();assert(receives==6);
    DWORD milliseconds=0,seconds=0;nativeMilliseconds=&milliseconds;nativeSeconds=&seconds;updateClocks();
    assert(timeGetTime()-milliseconds<1000 && seconds>1700000000);
    // Preview's virtual call occurs once in the continuous stage, never in the
    // legacy application calls. The native animation adapter supplies dt once.
    void *table[22]{};table[21]=reinterpret_cast<void *>(&move);void **model=table;void *modelPointer=&model;
    previewSlot=&modelPointer;void *player=reinterpret_cast<void *>(1);playerSlot=&player;
    { hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1,1);previewCall(&model,nullptr,1,0); }
    assert(previews==0);preview();assert(previews==1 && hybrid_time::frame.stage==hybrid_time::Stage::Idle);
#if FENB_NATIVE_X86_ASM
    using NoSound=void(__fastcall *)(void *,void *,float,int);NoSound bridge=&previewNoSound;
    { hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1,1);bridge(&model,nullptr,1,0); }
    assert(previews==1);
    // Execute the original instruction layout, including the incoming JE to
    // the CMP at offset 27. The earlier ten-byte patch overwrote that target
    // with CALL displacement bytes and crashed as soon as modelPointer==null.
    unsigned char originalFlow[]={
        0x8b,0x0d,0,0,0,0,0x85,0xc9,0x74,0x11,0x8b,0x01,0x6a,0,0x51,
        0xc7,0x04,0x24,0,0,0x80,0x3f,0x6a,0,0xff,0x50,0x54,
        0x83,0x3d,0,0,0,0,0,0x75,0x06,0xb8,0,0,0,0,0xc3,0xb8,1,0,0,0,0xc3
    };
    const uint32_t modelAddress=uint32_t(reinterpret_cast<uintptr_t>(&modelPointer)),playerAddress=uint32_t(reinterpret_cast<uintptr_t>(&player));
    std::memcpy(originalFlow+2,&modelAddress,4);std::memcpy(originalFlow+29,&playerAddress,4);
    auto *flowBytes=static_cast<unsigned char *>(VirtualAlloc(nullptr,sizeof(originalFlow),MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    assert(flowBytes);std::memcpy(flowBytes,originalFlow,sizeof(originalFlow));
    native_patch::Set previewPatch;const unsigned char oldCall[]={0x6a,0,0xff,0x50,0x54};
    assert(previewPatch.call(flowBytes+22,oldCall,reinterpret_cast<void *>(&previewNoSound)) && previewPatch.apply());
    assert(std::memcmp(flowBytes+27,originalFlow+27,7)==0 && flowBytes[8]==0x74 && flowBytes[9]==0x11);
    Flow flow=reinterpret_cast<Flow>(flowBytes);
    for(auto stage:{hybrid_time::Stage::Legacy,hybrid_time::Stage::World})
      for(bool hasModel:{false,true}) for(bool hasPlayer:{false,true}) {
        hybrid_time::Scope scope(stage,1,1);
        modelPointer=hasModel ? &model : nullptr;player=hasPlayer ? reinterpret_cast<void *>(1) : nullptr;
        const int prior=previews;StackPair stack{};const int observed=executeFlow(flow,&stack);
        assert(stack.before==stack.after && observed==int(hasPlayer));
        assert(previews==prior+int(hasModel && stage==hybrid_time::Stage::World));
        assert(hybrid_time::frame.thread==GetCurrentThreadId() && hybrid_time::frame.active && hybrid_time::frame.stage==stage);
      }
    previewPatch.restore();assert(std::memcmp(flowBytes,originalFlow,sizeof(originalFlow))==0);VirtualFree(flowBytes,0,MEM_RELEASE);
#endif
    alignas(16) unsigned char window[0x2300]{},actor[0x1400]{},world[512]{},action[128]{};
    void *timerTable[3]{};timerTable[2]=reinterpret_cast<void *>(&timeout);field<void **>(window,0x1148)=timerTable;
    field<void *>(actor,0x16c)=world;field<void *>(actor,0x324)=action;field<DWORD>(window,0x164c)=UINT32_MAX;
    assert(permitsHeldInput(window,actor,world));
    field<DWORD>(window,0x164c)=0x7fffffff;assert(!permitsHeldInput(window,actor,world));field<DWORD>(window,0x164c)=UINT32_MAX;
    field<unsigned char>(window,0x45)=1;assert(!permitsHeldInput(window,actor,world));
    field<DWORD>(window,0x1090)=1;expired=false;assert(!permitsHeldInput(window,actor,world));expired=true;
    assert(permitsHeldInput(window,actor,world));field<void *>(world,0x20)=actor;assert(!permitsHeldInput(window,actor,world));
    field<DWORD>(window,0x2000)=1;const int checks=timeouts;assert(!permitsHeldInput(window,actor,world) && timeouts==checks);
    field<void *>(world,0x20)=nullptr;assert(permitsHeldInput(window,actor,world));
    field<DWORD>(action,8)=8;assert(!permitsHeldInput(window,actor,world));field<DWORD>(window,0x1090)=0;assert(permitsHeldInput(window,actor,world));
    field<int>(window,0xa8)=123;field<int>(window,0xac)=456;
    void *wnd=window,*map=world;player=actor;native_world::wndWorldSlot=&wnd;native_world::worldSlot=&map;
    expectedWindow=window;originalControl=reinterpret_cast<Control>(&control);
    { hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1,1);assert(controlHook(window,nullptr,0,123,456)==0); }
    assert(controls==0);heldInput();assert(controls==1 && hybrid_time::frame.stage==hybrid_time::Stage::Idle);
    player=nullptr;heldInput();assert(controls==1);
    puts("Native hybrid app checks passed: once-per-frame packet receive, wall clocks, original preview JE/CMP flow for null/non-null model/player, RET8 stack balance, legacy suppression and skill/attack/flight input predicates.");
    return 0;
}
