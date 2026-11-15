#include "native_mover.hpp"
#include <cassert>
#include <cstdio>
void Log(const char *) {}
using visual_transform::Vec;
static int stateCalls=0;
static float speed(void *,float) { return .08f; }
static void __fastcall nativeState(void *action,void *,DWORD mask,float speed) {
    ++stateCalls;
    auto *mover=native_mover::field<void *>(action,0xc);
    if(mask==4) native_mover::field<float>(action,0x50)=speed;
    if(mask==0x100) native_mover::field<float>(mover,0x18)+=4;
    if(auto *model=native_mover::field<void *>(mover,0x178)) {
        auto &pause=native_mover::field<int>(model,0x94);if(pause>0) --pause;
    }
}
int main() {
    using namespace native_mover;
    installed=true;getSpeed=&speed;originalState=reinterpret_cast<State>(&nativeState);
    native_animation::enabled=true;
    for(int fps:{30,60,120,144,240,500,1000}) {
        alignas(16) unsigned char action[512]{},mover[8192]{},model[512]{};
        void *table[3]={nullptr,nullptr,reinterpret_cast<void *>(&stateHook)};
        field<void **>(action,0)=table;field<void *>(action,0xc)=mover;
        field<void *>(mover,0x178)=model;field<DWORD>(action,4)=4;
        field<int>(action,0x44)=1;
        hybrid_time::Clock clock;Vec position{};
        unsigned expectedLegacy=0;
        for(int i=0;i<=fps*2;++i) {
            hybrid_time::frame=clock.advance(int64_t(i)*1000000/fps,1000000);
            hybrid_time::frame.thread=GetCurrentThreadId();hybrid_time::frame.stage=hybrid_time::Stage::World;
            expectedLegacy+=hybrid_time::frame.legacy;
            field<Vec>(action,0x68)={};field<Vec>(action,0x74)=position;
            assert(actionHook(action,nullptr,&position)==1);
            position=position+field<Vec>(action,0x68);
            // Native ground collision resolves this after ProcessAction.
            position.y=0;field<float>(action,0x54)=0;field<int>(action,0x44)=1;
        }
        assert(std::fabs(position.x-10.08f)<.001f && position.y==0);
        assert(field<DWORD>(action,0x40)==expectedLegacy && expectedLegacy==126);
    }
    // A fractional native turn changes the actual angle immediately, and an
    // existing hit-stop consumes integer legacy time rather than render calls.
    alignas(16) unsigned char action[512]{},mover[8192]{},model[512]{};
    field<void *>(action,0xc)=mover;field<void *>(mover,0x178)=model;
    hybrid_time::frame={};hybrid_time::frame.active=true;hybrid_time::frame.thread=GetCurrentThreadId();
    hybrid_time::frame.stage=hybrid_time::Stage::World;hybrid_time::frame.ticks=.25f;hybrid_time::frame.legacy=0;
    field<int>(model,0x94)=4;
    stateHook(action,nullptr,0x100,.02f);
    assert(field<float>(mover,0x18)==1 && field<int>(model,0x94)==4);
    hybrid_time::frame.legacy=2;stateHook(action,nullptr,0x100,.02f);
    assert(field<float>(mover,0x18)==2 && field<int>(model,0x94)==2);
    // The fractional integrator is the affine power of the native one-tick
    // gravity and drag maps; identical elapsed time gives identical endpoints.
    float referenceVelocity=.2f,referenceHeight=0;
    Vec referenceFlight{.2f,0,0},referenceDistance{};
    for(int i=0;i<120;++i) {
        referenceHeight+=integrateGravity(referenceVelocity,1);
        referenceDistance=referenceDistance+integrateFlight(referenceFlight,{},1,false,false,false);
    }
    for(int divisions:{2,4,16,64}) {
        float velocity=.2f,height=0;Vec flight{.2f,0,0},distance{};
        for(int i=0;i<120*divisions;++i) {
            const float dt=1.f/divisions;
            height+=integrateGravity(velocity,dt);
            distance=distance+integrateFlight(flight,{},dt,false,false,false);
        }
        assert(std::fabs(height-referenceHeight)<.004f && std::fabs(velocity-referenceVelocity)<.0001f);
        assert(std::fabs(distance.x-referenceDistance.x)<.003f && std::fabs(flight.x-referenceFlight.x)<.0001f);
    }
    float terminalVelocity=-.498f;
    assert(std::fabs(integrateGravity(terminalVelocity,1)+.5f)<1e-6f && terminalVelocity==-.5f);
    installed=false;assert(!install()); // A test EXE must never pass the game's native ABI guards.
    puts("Native mover: authoritative motion at 30..1000 FPS, legacy action clocks, fractional turns/hit-stop, gravity and flight drag passed.");
}
