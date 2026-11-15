#include "native_mover_legacy.hpp"
#include <cassert>
#include <cstdio>
#include <vector>

void Log(const char *) {}
namespace {
using namespace native_mover_legacy;
std::vector<uint32_t> files,pets,collects;
void *currentActor=nullptr;
int patterns=0,questCalls=0,aiCalls=0,destroyed=0,created=0;
void __fastcall collect(void *p,void *) { collects.push_back(field<uint32_t>(p,0x11c8)); }
void __fastcall petTick(void *p,void *) { pets.push_back(field<uint32_t>(p,0x11c8)); }
void legacyScope() { assert(hybrid_time::frame.stage==hybrid_time::Stage::Legacy && hybrid_time::frame.ticks==1 && hybrid_time::frame.legacy==1); }
void __fastcall patternTick(void *,void *) { legacyScope();++patterns; }
void __fastcall questTick(void *,void *) { legacyScope();++questCalls; }
void __fastcall aiTick(void *,void *) { legacyScope();++aiCalls; }
int __stdcall fileTick(const char *path) { assert(path[0]=='r');files.push_back(field<uint32_t>(currentActor,0x11c8));return 1; }
void *__fastcall stringTick(void *p,void *,void *same) { assert(p==same);++destroyed;return p; }
void *createTick(void *device,unsigned id,void *pos,int source,void *angles,int target,int control,int add) {
    assert(device==reinterpret_cast<void *>(1) && id==70 && pos==reinterpret_cast<void *>(2) && source==-1 &&
        angles==reinterpret_cast<void *>(3) && target==-1 && control==0 && add==1);
    ++created;return reinterpret_cast<void *>(4);
}
struct Actor {
    alignas(16) unsigned char bytes[6000]{},queue[96]{};
    void *vtable[20]{},*aiTable[3]{};
    void *player=bytes;
    unsigned char *begin=queue,*end=queue+96;
    Actor() {
        currentActor=bytes;files.clear();pets.clear();collects.clear();patterns=questCalls=aiCalls=destroyed=created=0;
        hybrid_time::frame={};hybrid_time::frame.thread=GetCurrentThreadId();hybrid_time::frame.active=true;
        hybrid_time::frame.stage=hybrid_time::Stage::World;hybrid_time::frame.serial=1;installed=true;
        field<void **>(bytes,0)=vtable;vtable[18]=reinterpret_cast<void *>(&collect);
        field<int>(bytes,0x31c)=1;field<unsigned>(bytes,0x9f4)=3;
        for(int i=0;i<4;++i) { queue[i*24]='r';field<unsigned>(queue,i*24+20)=15; }
        playerSlot=&player;queueBegin=&begin;queueEnd=&end;
        pattern=reinterpret_cast<Process>(&patternTick);quests=reinterpret_cast<Process>(&questTick);
        pet=reinterpret_cast<Process>(&petTick);checkFile=&fileTick;destroyString=reinterpret_cast<DestroyString>(&stringTick);
        create=&createTick;reset();
    }
    void time(unsigned n) { hybrid_time::frame.legacy=n;hybrid_time::frame.ticks=float(n);++hybrid_time::frame.serial; }
};
void testCounters() {
    Actor a;field<uint32_t>(a.bytes,0x11c8)=31;a.time(3);periodicHook(a.bytes,nullptr);
    assert(files==std::vector<uint32_t>{32} && pets.empty() && field<uint32_t>(a.bytes,0x11c8)==31);
    processCountHook(a.bytes,nullptr);assert(field<uint32_t>(a.bytes,0x11c8)==34);
    field<uint32_t>(a.bytes,0x11c8)=63;periodicHook(a.bytes,nullptr);
    assert(files.back()==64 && pets==std::vector<uint32_t>{64} && collects==pets && destroyed==2);
    assert(field<unsigned>(a.bytes,0x9f4)==2);
    a.time(0);field<uint32_t>(a.bytes,0x11c8)=64;periodicHook(a.bytes,nullptr);processCountHook(a.bytes,nullptr);
    assert(files.size()==2 && pets.size()==1 && field<unsigned>(a.bytes,0x9f4)==2 && field<uint32_t>(a.bytes,0x11c8)==64);
    a.time(2);field<uint32_t>(a.bytes,0x11c8)=UINT32_MAX;periodicHook(a.bytes,nullptr);processCountHook(a.bytes,nullptr);
    assert(files.back()==0 && pets.back()==0 && field<uint32_t>(a.bytes,0x11c8)==1);
    field<int>(a.bytes,0x720)=5;attackCountHook(a.bytes,nullptr);assert(field<int>(a.bytes,0x720)==7);
    a.time(0);attackCountHook(a.bytes,nullptr);assert(field<int>(a.bytes,0x720)==7);
    field<int>(a.bytes,0x720)=0;a.time(8);attackCountHook(a.bytes,nullptr);assert(field<int>(a.bytes,0x720)==0);
    a.time(3);patternHook(a.bytes,nullptr);questsHook(a.bytes,nullptr);
    a.aiTable[2]=reinterpret_cast<void *>(&aiTick);void **ai=a.aiTable;aiHook(&ai,nullptr);
    assert(patterns==3 && questCalls==3 && aiCalls==3);
    a.time(0);patternHook(a.bytes,nullptr);questsHook(a.bytes,nullptr);aiHook(&ai,nullptr);
    assert(patterns==3 && questCalls==3 && aiCalls==3);
}
void testWater() {
    Actor a;a.time(18);field<unsigned char>(a.bytes,0xd1c)=7;
    assert(waterReadyHook(a.bytes,nullptr) && ripple.events==3 && ripple.after==1);
    auto result=rippleRelay(reinterpret_cast<void *>(1),70,a.bytes,nullptr,reinterpret_cast<void *>(2),-1,reinterpret_cast<void *>(3),-1,0,1);
    assert(result==reinterpret_cast<void *>(4) && created==3);
    field<unsigned char>(a.bytes,0xd1c)=0;waterCountHook(a.bytes,nullptr);
    assert(field<unsigned char>(a.bytes,0xd1c)==1 && !ripple.mover);
    a.time(0);field<unsigned char>(a.bytes,0xd1c)=8;
    assert(!waterReadyHook(a.bytes,nullptr));waterCountHook(a.bytes,nullptr);assert(field<unsigned char>(a.bytes,0xd1c)==8);
    a.time(1);assert(waterReadyHook(a.bytes,nullptr));waterCountHook(a.bytes,nullptr);
    assert(field<unsigned char>(a.bytes,0xd1c)==1);
    a.time(3);field<unsigned char>(a.bytes,0xd1c)=254;waterCountHook(a.bytes,nullptr);
    assert(field<unsigned char>(a.bytes,0xd1c)==1);
}
#if FENB_NATIVE_X86_ASM
void testThunks() {
    Actor a;a.time(2);field<unsigned char>(a.bytes,0xd1c)=8;
    void *actor=a.bytes;unsigned ready=0;float saved[4]={1,2,3,4},after[4]{};
    __asm {
        push edi
        mov edi,actor
        movups xmm2,saved
        call waterReadyThunk
        movzx eax,al
        mov ready,eax
        movups after,xmm2
        pop edi
    }
    assert(ready==1 && std::memcmp(saved,after,sizeof(saved))==0);
    uintptr_t beforeStack=0,afterStack=0;void *result=nullptr;
    __asm {
        push edi
        mov edi,actor
        mov beforeStack,esp
        push 1
        push 0
        push -1
        push 3
        push -1
        push 2
        mov ecx,1
        mov edx,70
        call rippleThunk
        add esp,24
        mov afterStack,esp
        mov result,eax
        call waterCountThunk
        call processCountThunk
        pop edi
    }
    assert(beforeStack==afterStack && result==reinterpret_cast<void *>(4) && created==1);
    assert(field<unsigned char>(a.bytes,0xd1c)==2 && field<uint32_t>(a.bytes,0x11c8)==2);
}
#endif
}
int main() {
    testCounters();testWater();
#if FENB_NATIVE_X86_ASM
    testThunks();
#endif
    native_mover_legacy::installed=false;
    assert(!native_mover_legacy::install());
    puts("Native mover legacy: modulo crossings, zero-step gating, AI/pattern cadence, ripple remainder and x86 bridges passed.");
}
