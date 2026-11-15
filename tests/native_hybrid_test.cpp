#include "native_hybrid.hpp"
#include <cassert>
#include <cstdio>
void Log(const char *) {}
namespace {
using visual_transform::Vec;
alignas(16) unsigned char actor[8192],action[512],model[512],previewModel[512],world[512],camera[512],window[9000];
void *actorPointer=actor,*worldPointer=world,*cameraPointer=camera,*windowPointer=window,*previewPointer=previewModel;
DWORD counter=0;
unsigned appCalls=0,worldCalls=0,networkCalls=0,inputCalls=0,cameraCalls=0;
bool resetDuringApp=false;
float speed(void *,float) { return .08f; }
void __fastcall state(void *a,void *,DWORD mask,float speed) { if(mask==4) native_mover::field<float>(a,0x50)=speed; }
void __fastcall mover(void *p,void *) {
    using native_mover::field;
    ++worldCalls;
    assert(hybrid_time::frame.stage==hybrid_time::Stage::World && hybrid_time::frame.ticks<=1.000001f);
    const Vec position=field<Vec>(p,0x160);
    field<Vec>(action,0x74)=position;field<Vec>(action,0x68)={};
    native_mover::actionHook(action,nullptr,&position);
    auto current=position+field<Vec>(action,0x68);current.y=0;field<Vec>(p,0x160)=current;
    field<float>(action,0x54)=0;field<int>(action,0x44)=1;
    native_animation::moveHook(model,nullptr,nullptr,1,0);
    native_mover_legacy::processCountHook(p,nullptr);
}
void __fastcall processWorld(void *,void *) {
    native_world::dispatchHook(actor,nullptr);
    native_camera::processHook(camera,nullptr,nullptr,4); // Native world's call is suppressed.
}
void __fastcall receive(void *,void *) { ++networkCalls; }
int __fastcall input(void *,void *,int,int,int) {
    assert(hybrid_time::frame.stage==hybrid_time::Stage::Input);++inputCalls;return 1;
}
void __fastcall movePreview(void *p,void *,void *position,float speed,int extra) {
    native_animation::moveHook(p,nullptr,position,speed,extra);
}
void __fastcall processCamera(void *,void *,void *,float factor) {
    assert(hybrid_time::frame.stage==hybrid_time::Stage::Camera && factor>=1);++cameraCalls;
}
void __fastcall transformCamera(void *p,void *,void *,void *) {
    auto &quake=native_camera::field<int>(p,0xe0);if(quake>0) --quake;
}
HRESULT __fastcall application(void *,void *) {
    assert(hybrid_time::frame.stage==hybrid_time::Stage::Legacy && hybrid_time::frame.ticks==1 && hybrid_time::frame.legacy==1);
    ++counter;++appCalls;
    native_app::receiveHook(reinterpret_cast<void *>(1),nullptr);
    native_world::legacyWorldHook(world,nullptr);
    native_app::controlHook(window,nullptr,0,0,0);
    native_app::previewCall(previewModel,nullptr,1,0);
    if(resetDuringApp) native_hybrid::reset();
    return S_OK;
}
void initialize() {
    using native_mover::field;
    for(auto block:{std::pair{actor,sizeof(actor)},std::pair{action,sizeof(action)},std::pair{model,sizeof(model)},
        std::pair{previewModel,sizeof(previewModel)},std::pair{world,sizeof(world)},std::pair{camera,sizeof(camera)},std::pair{window,sizeof(window)}})
        std::memset(block.first,0,block.second);
    counter=appCalls=worldCalls=networkCalls=inputCalls=cameraCalls=0;
    static void *actorTable[7]={},*actionTable[3]={},*previewTable[22]={};
    actorTable[6]=reinterpret_cast<void *>(&mover);actionTable[2]=reinterpret_cast<void *>(&native_mover::stateHook);
    previewTable[21]=reinterpret_cast<void *>(&movePreview);
    field<void **>(actor,0)=actorTable;field<void **>(action,0)=actionTable;field<void **>(previewModel,0)=previewTable;
    field<void *>(actor,0x324)=action;field<void *>(actor,0x178)=model;field<void *>(actor,0x16c)=world;
    field<DWORD>(actor,0x170)=5;field<void *>(action,0xc)=actor;field<DWORD>(action,4)=4;field<int>(action,0x44)=1;
    field<DWORD>(window,0x164c)=UINT32_MAX;
    for(void *p:{static_cast<void *>(model),static_cast<void *>(previewModel)}) { field<int>(p,0xa8)=1000;field<int>(p,0xb4)=4;field<float>(p,0xac)=1; }
    field<int>(camera,0xe0)=1000;
    native_hybrid::clock.reset();native_hybrid::installed=true;
    native_animation::enabled=true;
    native_mover::installed=true;native_mover::originalState=reinterpret_cast<native_mover::State>(&state);native_mover::getSpeed=&speed;
    native_mover_legacy::installed=true;native_mover_services::installed=true;
    native_world::active=true;native_world::worldSlot=&worldPointer;native_world::playerSlot=&actorPointer;
    native_world::cameraSlot=&cameraPointer;native_world::nativeCamera=camera;native_world::wndWorldSlot=&windowPointer;
    native_world::originalWorld=reinterpret_cast<native_world::Process>(&processWorld);
    native_world::afterObject=&native_mover_services::afterProcess;
    native_app::active=true;native_app::hasReceived=false;native_app::originalReceive=reinterpret_cast<native_app::Receive>(&receive);
    native_app::originalControl=reinterpret_cast<native_app::Control>(&input);native_app::playerSlot=&actorPointer;
    native_app::processCounter=&counter;native_app::previewSlot=&previewPointer;
    for(int i=0;i<3;++i) native_app::networkClients[i]=reinterpret_cast<void *>(i+1);
    native_camera::installed=true;native_camera::reset();native_camera::nativePlayer=&actorPointer;
    native_camera::nativeWorld=&worldPointer;native_camera::nativeCamera=&cameraPointer;
    native_camera::originalProcess=reinterpret_cast<native_camera::Process>(&processCamera);
    native_camera::originalTransform=reinterpret_cast<native_camera::Transform>(&transformCamera);
}
}
int main() {
    for(int fps:{30,60,144,240,500,1000}) {
        initialize();float elapsed=1.f/fps;
        for(int i=0;i<=fps*2;++i) {
            auto move=reinterpret_cast<HRESULT(__thiscall *)(void *)>(&application);
            assert(native_hybrid::run(nullptr,&elapsed,int64_t(i)*1000000/fps,1000000,move,[] {return true;})==S_OK);
            native_camera::transformHook(camera,nullptr,nullptr,world);
            native_camera::transformHook(camera,nullptr,nullptr,world);
            assert(hybrid_time::frame.stage==hybrid_time::Stage::Idle && elapsed==1.f/fps);
        }
        const unsigned renders=unsigned(fps*2+1);
        assert(appCalls==126 && counter==126 && networkCalls==renders*3 && inputCalls==renders && cameraCalls==renders);
        assert(native_mover::field<DWORD>(actor,0x11c8)==126 && native_mover::field<DWORD>(action,0x40)==126);
        assert(std::fabs(native_mover::field<Vec>(actor,0x160).x-10.08f)<.002f);
        assert(std::fabs(native_mover::field<float>(model,0xa0)-126)<.003f);
        assert(std::fabs(native_mover::field<float>(previewModel,0xa0)-126)<.003f);
        assert(native_mover::field<int>(camera,0xe0)==874);
    }
    native_hybrid::reset();assert(!hybrid_time::frame.active);
    initialize();resetDuringApp=true;float elapsed=.01f;
    auto move=reinterpret_cast<HRESULT(__thiscall *)(void *)>(&application);
    assert(native_hybrid::run(nullptr,&elapsed,0,1000000,move,[] {return true;})==S_OK);
    assert(appCalls==1 && worldCalls==0 && inputCalls==0 && cameraCalls==0 && !hybrid_time::frame.active);
    puts("Native hybrid integration: real action/animation phases, input/network/render frequency, legacy counters, bounded world steps and multipass camera quake agree at 30..1000 FPS.");
}
