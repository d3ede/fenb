#include "native_camera.hpp"
#include <cassert>
#include <cstdio>
void Log(const char *) {}
namespace {
int processes=0,transforms=0,flyingInputs=0;
float lastFactor=0,lastImpulse=0,lastRecovery=0,lastRoll=0,lastRollRecovery=0;
void __fastcall fakeProcess(void *,void *,void *,float factor) {
    ++processes;lastFactor=factor;lastImpulse=native_camera::impulseCoefficient;lastRecovery=native_camera::recoveryStep;
}
void __fastcall fakeTransform(void *camera,void *,void *,void *) {
    ++transforms;auto &quake=native_camera::field<int>(camera,0xe0);if(quake>0) --quake;
}
int __fastcall fakeFlying(void *,void *,DWORD,POINT) {
    ++flyingInputs;lastRoll=native_camera::rollStep;lastRollRecovery=native_camera::rollRecoveryStep;return 123;
}
bool closeEnough(float a,float b) { return std::fabs(a-b)<1e-5f; }
}
int main() {
    using namespace native_camera;
    assert(closeEnough(followGain(1,false),.25f) && closeEnough(followGain(1,true),.0625f));
    assert(followGain(100,false)==1 && followGain(100,true)==.25f);
    assert(closeEnough(impulseDecay(.25f)*impulseDecay(.75f),.5f));
    assert(quakeRemaining(2,100)==0 && quakeRemaining(10,3)==7 && quakeRemaining(-1,0)==0);
    Orbit initial{1,0,2};Keyboard left;left.left=true;
    assert(closeEnough(keyboardStep(initial,left,.5f,true).yaw,359));
    Keyboard up;up.up=true;Orbit atClamp{0,71.5f,2};
    assert(closeEnough(keyboardStep(atClamp,up,.5f,true).pitch,72)); // zoom*4 is geometric, never time-scaled
    Keyboard page;page.pageDown=true;Orbit almostMax{0,0,6.9f};
    assert(keyboardStep(almostMax,page,.5f,true).zoom==7);
    assert(closeEnough(keyboardStep(almostMax,page,.5f,false).zoom,6.9f));
    Keyboard right;right.right=true;Orbit four=initial;
    for(int i=0;i<4;++i) four=keyboardStep(four,right,.25f,true);
    assert(closeEnough(four.yaw,keyboardStep(initial,right,1,true).yaw));

    alignas(16) unsigned char camera[512]{},player[0x400]{},action[32]{};
    void *playerPointer=player,*worldPointer=reinterpret_cast<void *>(1);
    nativePlayer=&playerPointer;nativeWorld=&worldPointer;
    field<void *>(player,0x324)=action;
    originalProcess=reinterpret_cast<Process>(&fakeProcess);originalTransform=reinterpret_cast<Transform>(&fakeTransform);
    originalFlying=reinterpret_cast<Flying>(&fakeFlying);installed=true;reset();
    hybrid_time::frame={.5f,0,1,GetCurrentThreadId(),true,hybrid_time::Stage::World};
    processHook(camera,nullptr,nullptr,4);assert(processes==0); // native World never consumes a second camera update
    process(camera,nullptr);assert(processes==1 && closeEnough(lastFactor,8));
    assert(closeEnough(lastImpulse,std::sqrt(.5f)) && closeEnough(lastRecovery,.185f));
    assert(closeEnough(impulseCoefficient,.5f) && closeEnough(recoveryStep,.37f));
    process(camera,nullptr);assert(processes==1); // one native camera integration per render serial
    hybrid_time::frame.serial=2;hybrid_time::frame.ticks=1.5f;
    process(camera,nullptr);assert(processes==2 && closeEnough(lastFactor,4)); // (.5+1.5)*.5
    field<unsigned>(action,8)=8;hybrid_time::frame.serial=3;hybrid_time::frame.ticks=1;
    process(camera,nullptr);assert(processes==3 && closeEnough(lastFactor,16));
    worldPointer=reinterpret_cast<void *>(2);hybrid_time::frame.serial=4;hybrid_time::frame.ticks=.25f;
    process(camera,nullptr);assert(closeEnough(lastFactor,64)); // world replacement resets delta filter

    field<int>(camera,0xe0)=10;hybrid_time::frame.legacy=0;
    transformHook(camera,nullptr,nullptr,nullptr);assert(field<int>(camera,0xe0)==10);
    hybrid_time::frame.serial=5;hybrid_time::frame.legacy=3;
    transformHook(camera,nullptr,nullptr,nullptr);assert(field<int>(camera,0xe0)==7);
    transformHook(camera,nullptr,nullptr,nullptr);assert(field<int>(camera,0xe0)==7); // extra view/cull pass does not age quake
    hybrid_time::frame.stage=hybrid_time::Stage::Input;hybrid_time::frame.ticks=.25f;
    assert(flyingHook(nullptr,nullptr,0,{0,0})==123 && closeEnough(lastRoll,.25f) && closeEnough(lastRollRecovery,.5f));
    flyingHook(nullptr,nullptr,WM_MOUSEMOVE,{10,10});assert(lastRoll==0 && lastRollRecovery==0);
    hybrid_time::frame.stage=hybrid_time::Stage::Legacy;
    flyingHook(nullptr,nullptr,0,{0,0});assert(lastRoll==0 && lastRollRecovery==0);
    assert(rollStep==1 && rollRecoveryStep==2);
    hybrid_time::frame.active=false;
    flyingHook(nullptr,nullptr,0,{0,0});assert(lastRoll==1 && lastRollRecovery==2);
    processHook(camera,nullptr,nullptr,15);assert(lastFactor==15);
    field<int>(camera,0xe0)=2;transformHook(camera,nullptr,nullptr,nullptr);assert(field<int>(camera,0xe0)==1);
    assert(transforms==4 && flyingInputs==4);
    uninstall();
    puts("Native camera checks passed: Shining follow filter, elapsed impulse/recovery, once-per-render native integration, world reset, integer quake duration, camera key clamps and mouse-independent flying roll.");
}
