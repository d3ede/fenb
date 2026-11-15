#include "visual_camera.hpp"
#include <cassert>
#include <cstdio>
void Log(const char *) {}
static int nativeProcesses=0,nativeTransforms=0;
static bool target(visual_transform::Vec &p,float &yaw,float &pitch) { p={1.5f,0,0};yaw=0;pitch=0;return true; }
static void __fastcall fakeProcess(void *,void *,void *,float) { ++nativeProcesses; }
static void __fastcall fakeTransform(void *,void *,void *,void *) { ++nativeTransforms; }
int main() {
    using namespace visual_camera;using namespace visual_transform;
    assert(residualGain(4,0)==0 && std::fabs(residualGain(4,1)-.25f)<1e-6f);
    float a=residualGain(4,.25f),b=residualGain(4,.75f);
    assert(std::fabs(a+(1-a)*b-.25f)<1e-6f);
    alignas(16) unsigned char camera[512]{};
    History h;h.current={{1,1.3f,6},{1,1.3f,0}};h.previous={{-100,1.3f,6},{-100,1.3f,0}};
    h.offset={0,.4f,6};h.destination=h.offset;h.zoom=1;h.target={{1,0,0},0,0,true};
    visual_animation::field<float>(camera,0xfc)=1;
    visual_animation::field<float>(camera,0xdc)=std::asin(.4f/std::sqrt(36.16f))*57.29577951308232f;
    write(camera,h.current);
    auto now=currentTimePose(camera,h,.5f);assert(std::fabs(now.eye.x-1)<1e-5f); // previous state never used
    setTargetProvider(&target);now=currentTimePose(camera,h,.5f);
    assert(std::fabs(now.eye.x-1.5f)<1e-5f && std::fabs(now.look.x-1.5f)<1e-5f);
    assert(read(camera).eye.x==1); // pure sampler does not mutate native state
    visual_animation::field<float>(camera,0xd0)=90;now=currentTimePose(camera,h,.5f);
    assert(now.eye.x>1.5f); // newest mouse field consumed before another native Process
    h.collision=true;h.destination={0,.4f,60};now=currentTimePose(camera,h,1);
    Vec d=now.eye-now.look;assert(dot(d,d)<=36.0001f);
    h.collision=false;h.destination=h.offset;
    visual_animation::enabled=true;visual_animation::ownerThread=GetCurrentThreadId();active=true;
    originalProcess=reinterpret_cast<Process>(&fakeProcess);originalTransform=reinterpret_cast<visual_camera::Transform>(&fakeTransform);
    h.updated=visual_animation::tick;h.epoch=presentation_clock::frame.epoch;history[camera]=h;
    visual_animation::renderPhase=.5f;
    transformHook(camera,nullptr,nullptr,nullptr);transformHook(camera,nullptr,nullptr,nullptr);
    assert(nativeProcesses==0 && nativeTransforms==2);
    finishFrame();assert(read(camera).eye.x==1);
    transformHook(camera,nullptr,nullptr,nullptr);write(camera,{{99,1,1},{99,1,0}});finishFrame();
    assert(read(camera).eye.x==99 && !history.contains(camera)); // native cut wins
    puts("Camera current-time checks passed: no delayed baseline, matching mover target, fractional follow invariance, newest input, collision bound, pass reuse, state restore and native-cut preservation.");
}
