#include "native_world.hpp"
#include <cassert>
#include <cstdio>

void Log(const char *) {}
namespace {
using namespace native_world;
int calls=0,worldCalls=0,positionCalls=0,weatherCalls=0,initCalls=0,afterCalls=0,luaCalls=0,baseCalls=0,effectCalls=0,cleanupCalls=0;
float expectedTicks=1;
void *targetObject=nullptr;
uint32_t requestedId=0;
bool weatherReplacesVelocity=false;
void __fastcall process(void *p,void *) {
    ++calls;assert(hybrid_time::frame.ticks==expectedTicks);
    ++field<unsigned>(p,0x5f0);
}
void __fastcall processWorld(void *,void *) { ++worldCalls;assert(hybrid_time::frame.stage==hybrid_time::Stage::World); }
BOOL __fastcall getCtrl(void *,void *,uint32_t id,void **result) { requestedId=id;*result=targetObject;return targetObject!=nullptr; }
void __fastcall position(void *object,void *,const Vec *value) { ++positionCalls;field<Vec>(object,0x160)=*value; }
void __fastcall weather(void *sky,void *) {
    ++weatherCalls;
    field<Vec>(sky,0x5e64).x+=field<Vec>(sky,0xa4).x;
    if(weatherReplacesVelocity) field<Vec>(sky,0xa4)={9,8,7};
}
void __fastcall initFall(void *sky,void *) { ++initCalls;field<Vec>(sky,0xa4)={8,0,0}; }
void afterProcess(void *) { ++afterCalls; }
void __fastcall processLua(void *,void *) { ++luaCalls;assert(hybrid_time::frame.ticks==1 && hybrid_time::frame.stage==hybrid_time::Stage::Legacy); }
void __fastcall processBase(void *,void *) { ++baseCalls; }
BOOL __fastcall processEffect(void *model,void *) { ++effectCalls;return ++field<WORD>(model,0x150)==2; }
void __cdecl cleanup() { ++cleanupCalls;assert(followerQueue.empty()); }
struct Sfx {
    alignas(16) unsigned char object[2048]{},target[1024]{},world[256]{};
    std::array<void *,8> table{};
    unsigned char locator[20]{},hierarchy[16]{},descriptor[4]{};
    void *bases[1]{};
    int type=0;
    Sfx() {
        table[7]=nullptr;table[0]=locator;table[7]=nullptr;
        field<void **>(object,0)=table.data()+1;
        table[7]=reinterpret_cast<void *>(&process);
        field<void *>(locator,16)=hierarchy;
        field<unsigned>(hierarchy,8)=1;field<void *>(hierarchy,12)=bases;
        bases[0]=descriptor;field<void *>(descriptor,0)=&type;
        field<DWORD>(object,0x170)=3;field<void *>(object,0x16c)=world;
        field<uint32_t>(object,0x5fc)=17;field<Vec>(object,0x600)={1,2,3};
        field<Vec>(target,0x160)={4,5,6};
    }
};
}
int main() {
    using namespace native_world;
    active=true;hybrid_time::frame={.ticks=.25f,.legacy=0,.serial=1,.thread=GetCurrentThreadId(),.active=true,.stage=hybrid_time::Stage::World};
    resolve=reinterpret_cast<Resolve>(&getCtrl);setPos=reinterpret_cast<SetPos>(&position);ctrlMap=reinterpret_cast<void *>(123);
    sfxRefresh=&refreshSfx;
    int projectileType=0;shootType=&projectileType;
    Sfx effect;targetObject=effect.target;
    observedWorld=effect.world;
    dispatchHook(effect.object,nullptr);
    assert(positionCalls==0);flushFollowers();
    assert(calls==0 && positionCalls==1 && requestedId==17 && field<Vec>(effect.object,0x160).x==4);
    // An invalid target retains its last destination; no lifetime or event call.
    targetObject=nullptr;dispatchHook(effect.object,nullptr);flushFollowers();
    assert(calls==0 && positionCalls==2 && field<Vec>(effect.object,0x160).x==4);
    field<uint32_t>(effect.object,0x5fc)=UINT32_MAX;dispatchHook(effect.object,nullptr);flushFollowers();assert(positionCalls==2);
    // Projectile descendants inherit the no-op zero-tick callback in Shining.
    field<uint32_t>(effect.object,0x5fc)=17;field<void *>(effect.descriptor,0)=shootType;refreshableSfx.clear();
    targetObject=effect.target;dispatchHook(effect.object,nullptr);flushFollowers();assert(positionCalls==2);
    field<void *>(effect.descriptor,0)=&effect.type;refreshableSfx.clear();
    hybrid_time::frame.legacy=3;expectedTicks=1;dispatchHook(effect.object,nullptr);
    assert(calls==3 && field<unsigned>(effect.object,0x5f0)==3 && hybrid_time::frame.ticks==.25f && hybrid_time::frame.stage==hybrid_time::Stage::World);
    field<Vec>(effect.target,0x160)={20,5,6};originalCleanup=&cleanup;postObjectsHook();
    assert(field<Vec>(effect.object,0x160).x==20 && cleanupCalls==1); // current mover position after the earlier SFX category
    field<DWORD>(effect.object,4)=1;dispatchHook(effect.object,nullptr);assert(calls==3);field<DWORD>(effect.object,4)=0;
    field<DWORD>(effect.object,0x170)=5;expectedTicks=.25f;afterObject=&afterProcess;dispatchHook(effect.object,nullptr);assert(calls==4 && afterCalls==1);
    // Dropped items update actual physics and angle at the render interval.
    originalItem=reinterpret_cast<Process>(&process);baseObjectProcess=reinterpret_cast<Process>(&processBase);
    field<DWORD>(effect.object,0x170)=4;field<Vec>(effect.object,0x18)={0,0,0};
    field<Vec>(effect.object,0x160)={0,10,0};field<float>(effect.object,0x318)=1;
    dispatchHook(effect.object,nullptr);
    assert(baseCalls==1 && calls==4 && field<Vec>(effect.object,0x18).x==.125f);
    assert(std::fabs(field<Vec>(effect.object,0x31c).y-(-.0075f*.25f))<1e-7f);
    assert(std::fabs(field<Vec>(effect.object,0x160).y-(10.f-.0075f*.5f*.25f*1.25f))<1e-6f);
    field<Vec>(effect.object,0x160).y=1.00001f;dispatchHook(effect.object,nullptr);
    assert(field<Vec>(effect.object,0x160).y==1 && field<Vec>(effect.object,0x31c).y==0);
    for(int fps:{30,60,144,240,500,1000}) {
        field<Vec>(effect.object,0x160)={0,100,0};field<Vec>(effect.object,0x31c)={};field<float>(effect.object,0x318)=-1000;
        hybrid_time::frame.ticks=62.5f/float(fps);
        for(int i=0;i<fps;++i) itemStep(effect.object);
        const float expected=100.f-.0075f*.5f*62.5f*63.5f;
        assert(std::fabs(field<Vec>(effect.object,0x160).y-expected)<.002f);
    }
    hybrid_time::frame.ticks=.25f;
    field<WORD>(effect.object,0x2bc)=105;hybrid_time::frame.serial=1;updateAlpha(effect.object,100);
    assert(field<WORD>(effect.object,0x2bc)==101);
    field<WORD>(effect.object,0x2bc)=106;hybrid_time::frame.serial=2;updateAlpha(effect.object,101);
    assert(field<WORD>(effect.object,0x2bc)==102);
    originalLua=reinterpret_cast<Process>(&processLua);hybrid_time::frame.legacy=0;luaHook(nullptr,nullptr);assert(luaCalls==0);
    hybrid_time::frame.legacy=3;luaHook(nullptr,nullptr);assert(luaCalls==3 && hybrid_time::frame.ticks==.25f);
    unsigned char ambient[512]{};originalEffect=reinterpret_cast<EffectProcess>(&processEffect);
    hybrid_time::frame.legacy=0;assert(effectHook(ambient,nullptr)==FALSE && effectCalls==0);
    hybrid_time::frame.legacy=3;assert(effectHook(ambient,nullptr)==FALSE && effectCalls==3 && field<WORD>(ambient,0x150)==1);
    // Legacy UI encounters do not enter the world; one live world update follows.
    originalWorld=reinterpret_cast<Process>(&processWorld);
    void *currentWorld=effect.world,*player=effect.object,*camera=nullptr;
    worldSlot=&currentWorld;playerSlot=&player;cameraSlot=&camera;nativeCamera=reinterpret_cast<void *>(456);
    { hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1,1);legacyWorldHook(currentWorld,nullptr); }
    assert(worldCalls==0 && observedWorld==effect.world);
    continuousWorld();assert(worldCalls==1 && camera==nativeCamera);
    currentWorld=nullptr;continuousWorld();assert(worldCalls==1);currentWorld=effect.world;
    // Weather uses new dt for movement, then returns logical velocities unchanged.
    std::vector<unsigned char> sky(0x2b040);
    field<unsigned>(sky.data(),0x2b024)=1;field<Vec>(sky.data(),0xa4)={4,2,0};
    originalWeather=reinterpret_cast<Process>(&weather);originalInitFall=reinterpret_cast<Process>(&initFall);
    weatherHook(sky.data(),nullptr);
    assert(field<Vec>(sky.data(),0x5e64).x==1 && field<Vec>(sky.data(),0xa4).x==4 && weatherCalls==1);
    weatherReplacesVelocity=true;weatherHook(sky.data(),nullptr);
    assert(field<Vec>(sky.data(),0xa4).x==9);weatherReplacesVelocity=false;
    int option=1;weatherEnabled=&option;field<void *>(sky.data(),0xa0)=effect.world;field<int>(effect.world,0)=2;
    weatherHook(sky.data(),nullptr);assert(initCalls==1 && field<int>(sky.data(),0x20)==3 && field<Vec>(sky.data(),0xa4).x==8);
    // Water consumes elapsed ticks once, independent of legacy count and FPS.
    struct Definition { int maximum,unused;float speed; } definition{7,0,.31f};
    int count=1;void *definitions=&definition;float phases[10]{};
    waterCount=&count;waterDefinitions=&definitions;waterFrames=phases;
    for(int fps:{30,60,144,240,500,1000}) {
        phases[0]=0;hybrid_time::frame.ticks=62.5f/float(fps);hybrid_time::frame.legacy=0;
        for(int i=0;i<fps;++i) waterStep();
        assert(std::fabs(phases[0]-std::fmod(62.5f*.31f,7.f))<2e-4f);
    }
    puts("Native hybrid world checks passed: continuous dispatch, legacy SFX/event cadence, zero-tick destination following, projectile exclusion, map lifetime, weather dt/init ownership and FPS-independent water.");
}
