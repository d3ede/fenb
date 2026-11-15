#include "native_mover_services.hpp"
#include <cassert>
#include <cstdio>
void Log(const char *) {}
namespace {
bool invalidScale=false;int particles=0,shots=0;
float observedDivisor=0,observedTravel=0,observedIdle=0;
void __fastcall scale(void *p,void *) {
    using namespace native_mover_services;
    if(invalidScale) { field<float>(p,0xd48)=0;return; }
    const float retention=field<float>(p,0xd48);
    field<float>(p,0xc)=1+(field<float>(p,0xc)-1)*retention;
    field<float>(p,0xd48)=retention-scaleDecay;
}
void __fastcall eye(void *p,void *) {
    using namespace native_mover_services;void *m=field<void *>(p,0x178);
    if(field<int>(m,0xd8)) {
        if(field<unsigned>(m,0xe0)) field<float>(m,0xdc)-=sparkFade;
        ++field<unsigned>(m,0xe0);
    }
}
void __fastcall companion(void *,void *) {
    observedDivisor=native_mover_services::companionDivisor;observedTravel=native_mover_services::balloonTravel;observedIdle=native_mover_services::balloonIdle;
}
void __fastcall particle(void *,void *,native_mover_services::Vec) { ++particles; }
void __fastcall shoot(void *,void *,void *,int count) { assert(count==1);++shots; }
bool closeEnough(float a,float b) { return std::fabs(a-b)<1e-5f; }
}
int main() {
    using namespace native_mover_services;
    alignas(16) unsigned char mover[0x1500]{},model[512]{};
    installed=true;originalScale=reinterpret_cast<Service>(&scale);originalEye=reinterpret_cast<Service>(&eye);
    originalAngel=reinterpret_cast<Service>(&companion);originalBalloon=reinterpret_cast<Service>(&companion);originalParticle=reinterpret_cast<Particle>(&particle);
    hybrid_time::frame={.25f,0,1,GetCurrentThreadId(),true,hybrid_time::Stage::World};
    field<float>(mover,0xd48)=.81f;field<float>(mover,0xc)=2;
    scaleHook(mover,nullptr);
    assert(closeEnough(field<float>(mover,0xc),1+std::pow(.81f,.25f)));
    assert(closeEnough(field<float>(mover,0xd48),.80975f) && closeEnough(scaleDecay,.001f));
    invalidScale=true;scaleHook(mover,nullptr);assert(field<float>(mover,0xd48)==0);invalidScale=false;
    field<void *>(mover,0x178)=model;field<int>(model,0xd8)=1;field<unsigned>(model,0xe0)=20;field<float>(model,0xdc)=1;
    eyeHook(mover,nullptr);assert(field<unsigned>(model,0xe0)==20 && closeEnough(field<float>(model,0xdc),.9875f));
    hybrid_time::frame.legacy=3;eyeHook(mover,nullptr);assert(field<unsigned>(model,0xe0)==23);
    assert(closeEnough(sparkFade,.05f));
    angelHook(mover,nullptr);assert(closeEnough(observedDivisor,40) && companionDivisor==10);
    balloonHook(mover,nullptr);assert(closeEnough(observedDivisor,40) && closeEnough(observedTravel,.0015f) && closeEnough(observedIdle,.0005f));
    assert(closeEnough(balloonTravel,.006f) && closeEnough(balloonIdle,.002f));
    hybrid_time::frame.legacy=0;particleHook(mover,nullptr,{});assert(particles==0);
    hybrid_time::frame.legacy=3;particleHook(mover,nullptr,{});assert(particles==3);
    assert(hybrid_time::frame.stage==hybrid_time::Stage::World);
    alignas(16) unsigned char action[512]{};originalShoot=reinterpret_cast<Shoot>(&shoot);originalMelee=originalShoot;
    uint32_t processIndex=12;processCounter=&processIndex;
    assert(quarterSteps(12,9)==3 && quarterSteps(6,2)==0 && quarterSteps(8,2)==1 && quarterSteps(0,2)==1);
    hybrid_time::frame.legacy=9;field<int>(action,0x90)=2;shootTickHook(action,mover,model);assert(shots==2 && field<int>(action,0x90)==0);
    field<int>(action,0x90)=7;hybrid_time::frame.legacy=0;shootTickHook(action,mover,model);
    assert(shots==2 && field<int>(action,0x90)==7); // zero-tick frames neither emit nor consume continuation
    processIndex=20;field<int>(action,0x94)=9;hybrid_time::frame.legacy=8;meleeTickHook(action,mover,model);
    assert(shots==4 && field<int>(action,0x90)==5 && field<int>(action,0x94)==9); // native hit-event count remains native
    const auto identity=rotation({});std::memcpy(mover+0x64,identity.data(),64);std::memcpy(mover+0x24,identity.data(),64);
    field<Vec>(mover,0x54)={-20,-30,-40};field<Vec>(mover,0x160)={2,3,4};
    field<DWORD>(mover,0x170)=5;field<Vec>(mover,0x18)={90,0,0};afterProcess(mover);
    const auto *matrix=reinterpret_cast<float *>(mover+0xa4);
    assert(closeEnough(matrix[2],1) && closeEnough(matrix[8],-1) && (field<DWORD>(mover,4)&8));
    const auto *world=reinterpret_cast<float *>(mover+0xe4);
    assert(closeEnough(world[2],1) && closeEnough(world[8],-1) && closeEnough(world[12],2) && closeEnough(world[13],3) && closeEnough(world[14],4));
    assert(field<Vec>(mover,0x54).x==2 && field<Vec>(mover,0x54).y==3 && field<Vec>(mover,0x54).z==4);
    const auto saved=rotation({90,0,0});field<DWORD>(mover,0x170)=3;field<Vec>(mover,0x18)={180,0,0};afterProcess(mover);
    assert(std::memcmp(saved.data(),mover+0xa4,64)==0);
    hybrid_time::frame.active=false;balloonHook(mover,nullptr);
    assert(observedDivisor==10 && closeEnough(observedTravel,.006f) && closeEnough(observedIdle,.002f));
    uninstall();
    puts("Native mover services checks passed: elapsed scale/spark/follower/bob integration, native invalid-rank reset, integer particle cadence and current native rotation matrices.");
}
