#include "native_hybrid.hpp"
#include <cassert>
#include <cstdio>
void Log(const char *) {}
namespace {
using visual_transform::Vec;
alignas(16) unsigned char actor[8192],action[512],model[512],previewModel[512],world[512],camera[512],window[9000];
alignas(16) unsigned char mob[8192],mobAction[512],mobModel[512],motion[128],mobMotion[128],sourceSfx[2048],targetSfx[2048];
native_animation::Attribute attributes[1000]{};
unsigned playerHits=0,mobHits=0;
void *actorPointer=actor,*worldPointer=world,*cameraPointer=camera,*windowPointer=window,*previewPointer=previewModel;
DWORD counter=0;
unsigned appCalls=0,worldCalls=0,networkCalls=0,inputCalls=0,cameraCalls=0;
bool resetDuringApp=false;
float speed(void *,float) { return .08f; }
void __thiscall state(void *a,DWORD mask,float speed) { if(mask==4) native_mover::field<float>(a,0x50)=speed; }
void __thiscall mover(void *p) {
    using native_mover::field;
    ++worldCalls;
    assert(hybrid_time::frame.stage==hybrid_time::Stage::World && hybrid_time::frame.ticks<=1.000001f);
    auto *action=field<void *>(p,0x324),*model=field<void *>(p,0x178);
    if(native_animation::hitHook(model,nullptr)&1)++(p==actor?playerHits:mobHits);
    const Vec position=field<Vec>(p,0x160);
    field<Vec>(action,0x74)=position;field<Vec>(action,0x68)={};
    native_mover::actionHook(action,nullptr,&position);
    auto current=position+field<Vec>(action,0x68);current.y=0;field<Vec>(p,0x160)=current;
    field<float>(action,0x54)=0;field<int>(action,0x44)=1;
    native_animation::moveHook(model,nullptr,nullptr,1,0);
    native_mover_legacy::processCountHook(p,nullptr);
}
void __thiscall sfxProcess(void *p) { assert(hybrid_time::frame.stage==hybrid_time::Stage::Legacy);++native_mover::field<unsigned>(p,0x5f0); }
void cleanup() {}
BOOL __thiscall resolve(void *,uint32_t id,void **out) { *out=id==1?actor:id==2?mob:nullptr;return *out!=nullptr; }
void __thiscall setPos(void *p,const Vec *v) { native_mover::field<Vec>(p,0x160)=*v; }
void __thiscall processWorld(void *) {
    native_world::dispatchHook(sourceSfx,nullptr);native_world::dispatchHook(targetSfx,nullptr);
    native_world::dispatchHook(actor,nullptr);native_world::dispatchHook(mob,nullptr);
    native_world::postObjectsHook();
    native_camera::processHook(camera,nullptr,nullptr,4); // Native world's call is suppressed.
}
void __thiscall receive(void *) { ++networkCalls; }
int __thiscall input(void *,int,int,int) {
    assert(hybrid_time::frame.stage==hybrid_time::Stage::Input);++inputCalls;return 1;
}
void __thiscall movePreview(void *p,void *position,float speed,int extra) {
    native_animation::moveHook(p,nullptr,position,speed,extra);
}
void __thiscall processCamera(void *,void *,float factor) {
    assert(hybrid_time::frame.stage==hybrid_time::Stage::Camera && factor>=1);++cameraCalls;
}
void __thiscall transformCamera(void *p,void *,void *) {
    auto &quake=native_camera::field<int>(p,0xe0);if(quake>0) --quake;
}
HRESULT __thiscall application(void *) {
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
        std::pair{previewModel,sizeof(previewModel)},std::pair{world,sizeof(world)},std::pair{camera,sizeof(camera)},std::pair{window,sizeof(window)},
        std::pair{mob,sizeof(mob)},std::pair{mobAction,sizeof(mobAction)},std::pair{mobModel,sizeof(mobModel)},std::pair{motion,sizeof(motion)},
        std::pair{mobMotion,sizeof(mobMotion)},std::pair{sourceSfx,sizeof(sourceSfx)},std::pair{targetSfx,sizeof(targetSfx)}})
        std::memset(block.first,0,block.second);
    counter=appCalls=worldCalls=networkCalls=inputCalls=cameraCalls=playerHits=mobHits=0;
    static void *actorTable[7]={},*actionTable[3]={},*previewTable[22]={},*sfxTable[7]={};
    actorTable[6]=reinterpret_cast<void *>(&mover);actionTable[2]=reinterpret_cast<void *>(&native_mover::stateHook);
    previewTable[21]=reinterpret_cast<void *>(&movePreview);
    field<void **>(actor,0)=actorTable;field<void **>(action,0)=actionTable;field<void **>(previewModel,0)=previewTable;
    field<void *>(actor,0x324)=action;field<void *>(actor,0x178)=model;field<void *>(actor,0x16c)=world;
    field<DWORD>(actor,0x170)=5;field<void *>(action,0xc)=actor;field<DWORD>(action,4)=4;field<int>(action,0x44)=1;
    field<void **>(mob,0)=actorTable;field<void **>(mobAction,0)=actionTable;
    field<void *>(mob,0x324)=mobAction;field<void *>(mob,0x178)=mobModel;field<void *>(mob,0x16c)=world;
    field<DWORD>(mob,0x170)=5;field<void *>(mobAction,0xc)=mob;field<DWORD>(mobAction,4)=4;field<int>(mobAction,0x44)=1;
    field<Vec>(mob,0x160)={1.5f,0,0};
    const std::array<float,16> identity={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::memcpy(actor+0x64,identity.data(),64);std::memcpy(mob+0x64,identity.data(),64);
    std::fill(std::begin(attributes),std::end(attributes),native_animation::Attribute{});
    for(int i=5;i<120;i+=10)attributes[i]={1,0,float(i)+.25f};
    field<void *>(model,0x108)=motion;field<void *>(mobModel,0x108)=mobMotion;
    field<native_animation::Attribute *>(motion,4)=attributes;field<native_animation::Attribute *>(mobMotion,4)=attributes;
    sfxTable[6]=reinterpret_cast<void *>(&sfxProcess);
    for(auto *effect:{sourceSfx,targetSfx}) { field<void **>(effect,0)=sfxTable;field<DWORD>(effect,0x170)=3;field<void *>(effect,0x16c)=world;field<uint32_t>(effect,0x5f8)=effect==sourceSfx?1:2; }
    native_sfx_follow::installed=true;native_sfx_follow::bindings.clear();native_sfx_follow::bindings[sfxTable]=native_sfx_follow::Kind::Source;
    native_sfx_follow::resolve=reinterpret_cast<native_sfx_follow::Resolve>(&resolve);native_sfx_follow::setPos=reinterpret_cast<native_sfx_follow::SetPos>(&setPos);
    native_sfx_follow::ctrlMap=reinterpret_cast<void *>(1);
    native_world::sfxRefresh=&native_sfx_follow::refreshCallback;native_world::originalCleanup=&cleanup;
    field<DWORD>(window,0x164c)=UINT32_MAX;
    for(void *p:{static_cast<void *>(model),static_cast<void *>(mobModel),static_cast<void *>(previewModel)}) { field<int>(p,0xa8)=1000;field<int>(p,0xb4)=4;field<float>(p,0xac)=1; }
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
namespace combat {
using native_app::field;
DWORD attackState=0;
unsigned idleFrames=0,starts=0,completions=0,requests=0,hits=0,cycleHits=0;
bool hitStop=false;
int __thiscall canTarget(void *p,void *target) { assert(p==actor && target==mob);return 1; }
int __thiscall request(void *p,void *target) {
    assert(p==window && target==mob && !(field<DWORD>(action,4)&0xff0000u));
    assert(!field<DWORD>(actor,0x1180));
    ++requests;field<DWORD>(actor,0x1180)=3;return 1;
}
void __thiscall process(void *) {
    // Native ordering: goal processing -> state/completion -> animation phase.
    // Selecting idle after completion but before a new request reproduces the
    // high-FPS regression. Native completion retains the finished clip itself.
    if(!(field<DWORD>(action,4)&0xff0000u)) {
        if(field<DWORD>(actor,0x1180)==3) {
            ++starts;cycleHits=0;field<DWORD>(actor,0x1180)=0;
            field<DWORD>(actor,0x1194)=77;field<DWORD>(action,4)=attackState|0xa;
            field<int>(model,0xa8)=29;field<float>(model,0xa0)=field<float>(model,0xa4)=0;
            field<int>(model,0x9c)=0;
        } else { ++idleFrames;field<int>(model,0xa8)=42; }
    }
    if(field<DWORD>(action,4)&0xff0000u) {
        if(field<int>(model,0x9c)) {
            assert(cycleHits==1);++completions;field<DWORD>(action,4)=0xa;field<DWORD>(actor,0x1194)=0;
        }
        const int pause=native_animation::capturePause(model);
        if(pause) { --field<int>(model,0x94);native_animation::settlePause(model,pause); }
        else if(native_animation::hitHook(model,nullptr)&1) {
            ++hits;++cycleHits;assert(cycleHits==1);
            if(hitStop)field<int>(model,0x94)=4;
        }
    }
    native_animation::moveHook(model,nullptr,nullptr,1.655077f,0);
}
HRESULT __thiscall app(void *) {
    ++counter;
    native_world::legacyWorldHook(world,nullptr);
    if(!(field<DWORD>(action,4)&0xff0000u) && !field<DWORD>(actor,0x1180))request(window,mob);
    return S_OK;
}
unsigned run(int fps,DWORD state,bool repeat,bool pause,bool background=false) {
    initialize();attackState=state;hitStop=pause;idleFrames=completions=requests=hits=cycleHits=0;starts=1;
    native_app::canTarget=&canTarget;native_app::requestTarget=repeat?&request:nullptr;
    native_world::originalWorld=&process;
    field<void *>(world,0x20)=mob;field<DWORD>(mob,0x2d4)=77;
    field<DWORD>(actor,0x1194)=77;field<DWORD>(window,0x2000)=1;
    field<DWORD>(action,4)=state|0xa;field<DWORD>(action,8)=1;
    field<int>(model,0xa8)=29;field<int>(model,0xb4)=1;field<float>(model,0xac)=.5f;
    std::fill(std::begin(attributes),std::end(attributes),native_animation::Attribute{});
    attributes[14]={1,0,14};float elapsed=1.f/fps;
    for(int i=0;i<fps*4;++i) {
        auto move=reinterpret_cast<HRESULT(__thiscall *)(void *)>(&app);
        assert(native_hybrid::run(nullptr,&elapsed,int64_t(i)*1000000/fps,1000000,move,[] {return true;},background)==S_OK);
    }
    assert(completions>=5 && hits>=completions && hits<=starts && starts<=completions+1);
    assert(requests==completions); // one native continuation per completed attack
    native_app::canTarget=nullptr;native_app::requestTarget=nullptr;
    return idleFrames;
}
void test() {
    assert(run(500,0x60000,false,false)>0); // fixture observes the old idle flash
    for(int fps:{30,60,144,240,500,1000})for(DWORD state:{0x10000u,0x20000u,0x30000u,0x40000u,0x50000u,0x60000u,0x70000u,0x80000u})
        for(bool pause:{false,true})for(bool background:{false,true})assert(run(fps,state,true,pause,background)==0);
}
}
int main() {
    for(bool background:{false,true})for(int fps:{30,60,144,240,500,1000}) {
        initialize();float elapsed=1.f/fps;unsigned fractionalMoves=0;
        for(int i=0;i<=fps*2;++i) {
            const float before=native_mover::field<Vec>(actor,0x160).x;const unsigned legacyBefore=appCalls;
            auto move=reinterpret_cast<HRESULT(__thiscall *)(void *)>(&application);
            assert(native_hybrid::run(nullptr,&elapsed,int64_t(i)*1000000/fps,1000000,move,[] {return true;},background)==S_OK);
            const float current=native_mover::field<Vec>(actor,0x160).x;
            if(i>0 && fps>=144)assert(current>before); // Authoritative movement on every render, including zero legacy ticks.
            if(appCalls==legacyBefore && current>before)++fractionalMoves;
            for(auto pair:{std::pair{actor,sourceSfx},std::pair{mob,targetSfx}}) {
                const Vec position=native_mover::field<Vec>(pair.first,0x160);
                const Vec skill=native_mover::field<Vec>(pair.second,0x160);
                const float *matrix=reinterpret_cast<const float *>(pair.first+0xe4);
                assert(std::fabs(matrix[12]-position.x)<1e-6f && std::fabs(skill.x-position.x)<1e-6f);
                assert(matrix[13]==position.y && matrix[14]==position.z && skill.y==position.y && skill.z==position.z);
            }
            assert(std::fabs(native_mover::field<Vec>(mob,0x160).x-current-1.5f)<.002f); // Native range and both rendered positions agree.
            if(!background) {
                native_camera::transformHook(camera,nullptr,nullptr,world);
                native_camera::transformHook(camera,nullptr,nullptr,world);
            }
            assert(native_mover::field<Vec>(actor,0x160).x==current);
            assert(hybrid_time::frame.stage==hybrid_time::Stage::Idle && elapsed==1.f/fps);
        }
        if(fps>=144)assert(fractionalMoves>0);
        assert(playerHits==12 && mobHits==12);
        assert(native_mover::field<unsigned>(sourceSfx,0x5f0)==126 && native_mover::field<unsigned>(targetSfx,0x5f0)==126);
        assert(native_mover::field<DWORD>(mob,0x11c8)==126);
        const unsigned renders=unsigned(fps*2+1);
        assert(appCalls==126 && counter==126 && networkCalls==renders*3 && inputCalls==(background?0:renders) && cameraCalls==(background?0:renders));
        assert(native_mover::field<DWORD>(actor,0x11c8)==126 && native_mover::field<DWORD>(action,0x40)==126);
        assert(std::fabs(native_mover::field<Vec>(actor,0x160).x-10.08f)<.002f);
        assert(std::fabs(native_mover::field<float>(model,0xa0)-126)<.003f);
        assert(std::fabs(native_mover::field<float>(previewModel,0xa0)-(background?0:126))<.003f);
        assert(native_mover::field<int>(camera,0xe0)==(background?1000:874));
    }
    initialize();float transitionElapsed=.002f;
    auto transitionMove=reinterpret_cast<HRESULT(__thiscall *)(void *)>(&application);
    unsigned foregroundFrames=0;
    for(int i=0;i<=200;++i) {
        const bool background=i>=50 && i<150;
        if(!background)++foregroundFrames;
        const float before=native_mover::field<Vec>(actor,0x160).x;
        assert(native_hybrid::run(nullptr,&transitionElapsed,int64_t(i)*2000,1000000,transitionMove,[] {return true;},background)==S_OK);
        const float displacement=native_mover::field<Vec>(actor,0x160).x-before;
        assert(displacement>0 && displacement<=.080001f);
    }
    assert(inputCalls==foregroundFrames && cameraCalls==foregroundFrames && counter==26);
    assert(std::fabs(native_mover::field<Vec>(actor,0x160).x-2.08f)<.001f);
    native_hybrid::reset();assert(!hybrid_time::frame.active);
    initialize();resetDuringApp=true;float elapsed=.01f;
    auto move=reinterpret_cast<HRESULT(__thiscall *)(void *)>(&application);
    assert(native_hybrid::run(nullptr,&elapsed,0,1000000,move,[] {return true;})==S_OK);
    assert(appCalls==1 && worldCalls==0 && inputCalls==0 && cameraCalls==0 && !hybrid_time::frame.active);
    combat::test();
    puts("Native hybrid integration: per-render player/mob positions, matching skill origins and world matrices, attack counts, legacy SFX lifetime, input/network frequency and multipass camera quake agree at 30..1000 FPS.");
}
