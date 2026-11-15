#include "visual_animation.hpp"
#include "visual_camera.hpp"
#include "fixed_step.hpp"
#include <cassert>
#include <cstdio>
void Log(const char *) {}
namespace {
int calls=0, moves=0;
float observed=0;
std::array<float,16> observedWorld{};
int __fastcall fakeRender(void *p,void *,void *,const float *world) {
    ++calls; observed=visual_animation::field<float>(p,0xa0);
    std::copy_n(world,16,observedWorld.begin());
    auto bones=visual_animation::field<unsigned char *>(p,0x118);
    bones[0]=99; return 77;
}
visual_camera::Pose observedCamera{};
int cameraMoves=0,cameraRenders=0;
void __fastcall fakeCameraProcess(void *p,void *,void *device,float factor) {
    assert(device==reinterpret_cast<void *>(123) && factor==4.f); ++cameraMoves;
    auto pose=visual_camera::read(p); pose.eye.x+=2; pose.look.x+=2; visual_camera::write(p,pose);
}
void __fastcall fakeCameraTransform(void *p,void *,void *device,void *world) {
    assert(device==reinterpret_cast<void *>(123) && world==reinterpret_cast<void *>(456));
    assert(visual_animation::field<int>(p,0xe0)==0);
    observedCamera=visual_camera::read(p); ++cameraRenders;
}
void *__fastcall fakeDestroy(void *p,void *,unsigned) { return p; }
void __fastcall fakeMove(void *p,void *,void *sound,float speed,int extra) {
    assert(sound==reinterpret_cast<void *>(123)); assert(extra==456); ++moves;
    visual_animation::field<float>(p,0xa0)+=speed;
}
}
int main() {
    using namespace visual_animation;
    alignas(16) unsigned char object[512]{},motion[128]{},bones[128]{};
    field<void *>(object,0x108)=motion; field<void *>(object,0x118)=bones;
    field<int>(motion,0x54)=2;
    field<int>(object,0xa8)=20; field<int>(object,0xb4)=4;
    field<float>(object,0xa0)=5;
    originalRender=reinterpret_cast<Render>(&fakeRender);
    originalMove=reinterpret_cast<Move>(&fakeMove);
    enabled=true; ownerThread=GetCurrentThreadId(); beginTick(); renderPhase=.5f;
    moveHook(object,nullptr,reinterpret_cast<void *>(123),1.f,456);
    float matrix[16]{}; matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;
    assert(renderHook(object,nullptr,nullptr,matrix)==77);
    assert(observed==6.5f && field<float>(object,0xa0)==6 && bones[0]==0);
    renderHook(object,nullptr,nullptr,matrix); assert(observed==6.5f); // native glow/name pass sees the same phase
    finishFrame(); assert(field<float>(object,0xa0)==6 && bones[0]==0);
    renderPhase=.75f; renderHook(object,nullptr,nullptr,matrix);
    assert(observed==6.75f && moves==1 && calls==3);
    finishFrame();
    Sample a{19.5f,0,motion,20,4},b{.5f,0,motion,20,4};
    assert(frameBetween(a,b,.5f)==0);
    assert(frameAtCurrent(a,b,.5f)==1.f);
    // Run the actual pose sampler at different render rates, including loop boundaries.
    for(int fps:{30,60,144,240,500,1000}) {
        FixedStep clock;float native=0;
        for(int i=0;i<=fps*2;++i) {
            const int64_t now=int64_t(i)*1000000/fps;
            const unsigned steps=clock.advance(now,1000000);
            native=std::fmod(native+float(steps),20.f);
            Sample previous{std::fmod(native+19.f,20.f),0,motion,20,4};
            Sample current{native,0,motion,20,4};
            const float actual=frameAtCurrent(previous,current,clock.fraction(1000000));
            const float expected=std::fmod(1.f+float(double(now)/16000.0),20.f);
            assert(std::fabs(std::remainder(actual-expected,20.f))<2e-5f);
        }
    }
    Sample paused=b;paused.pause=1;assert(frameAtCurrent(a,paused,.8f)==paused.frame);
    Sample nonloopA{18,0,motion,20,0},nonloopB{19,0,motion,20,0};
    assert(frameAtCurrent(nonloopA,nonloopB,.8f)==19);
    Sample restartA{10,0,motion,20,4},restartB{0,0,motion,20,4};
    assert(frameAtCurrent(restartA,restartB,.8f)==0);
    assert(frameAtCurrent(a,b,std::nanf(""))==b.frame);
    // A new clip must render its native state, with no interpolation from the old clip.
    field<void *>(object,0x108)=nullptr;
    renderHook(object,nullptr,nullptr,matrix); assert(observed==6);
    FixedStep scheduler; scheduler.advance(0,1000000); scheduler.advance(4000,1000000);
    assert(scheduler.fraction(1000000)==.25f);
    // Shortest rotation across 360 degrees, with nonuniform scale preserved.
    using namespace visual_transform;
    constexpr float pi=3.14159265358979323846f;
    auto yaw=[](float angle) { return Quat{0,std::sin(angle/2),0,std::cos(angle/2)}; };
    auto oldMatrix=compose({{0,0,0},{2,3,4},yaw(350*pi/180)});
    auto newMatrix=compose({{2,0,0},{2,3,4},yaw(10*pi/180)});
    float interpolated[16]; assert(interpolate(oldMatrix.data(),newMatrix.data(),.5f,interpolated));
    assert(std::fabs(interpolated[0]-2)<1e-4f && std::fabs(interpolated[2])<1e-4f && interpolated[12]==1);
    for(float angle : {0.f,1.f,pi,4.f}) {
        Transform t{{2,3,4},{2,3,4},yaw(angle)},decoded;
        auto matrix=compose(t); assert(decompose(matrix.data(),decoded)); auto roundtrip=compose(decoded);
        for(int i=0;i<16;++i) assert(std::fabs(roundtrip[i]-matrix[i])<1e-4f);
    }
    // Exercise rotation through the actual model render wrapper as well as its math helper.
    field<void *>(object,0x108)=motion;
    renderPhase=.5f;
    renderHook(object,nullptr,nullptr,newMatrix.data());
    assert(observedWorld==newMatrix); // Never move geometry away from its native shadow/picking position.
    finishFrame();
    newMatrix[12]=200; assert(!interpolate(oldMatrix.data(),newMatrix.data(),.5f,interpolated));
    newMatrix=oldMatrix; newMatrix[0]*=-1; assert(!interpolate(oldMatrix.data(),newMatrix.data(),.5f,interpolated));
    // Camera callbacks use their native x86 ABI and hold one shared render pose through all passes.
    alignas(16) unsigned char camera[512]{};
    visual_camera::write(camera,{{0,0,6},{0,0,0}});
    visual_camera::originalProcess=reinterpret_cast<visual_camera::Process>(&fakeCameraProcess);
    visual_camera::originalTransform=reinterpret_cast<visual_camera::Transform>(&fakeCameraTransform);
    visual_camera::active=true;
    float vectors[9]={1,2,3,4,5,6,7,8,9}; visual_camera::cameraVectors=vectors;
    auto processCamera=[&]() { visual_camera::processHook(camera,nullptr,reinterpret_cast<void *>(123),4); };
    auto renderCamera=[&]() { visual_camera::transformHook(camera,nullptr,reinterpret_cast<void *>(123),reinterpret_cast<void *>(456)); };
    beginTick(); processCamera(); renderCamera(); visual_camera::finishFrame();
    beginTick(); processCamera(); renderPhase=.25f; renderCamera();
    assert(std::fabs(observedCamera.eye.x-4.f)<1e-5f && std::fabs(observedCamera.look.x-4.f)<1e-5f);
    renderCamera(); assert(std::fabs(observedCamera.eye.x-4.f)<1e-5f);
    visual_camera::finishFrame(); assert(visual_camera::read(camera).eye.x==4);
    for(int i=0;i<9;++i) assert(vectors[i]==float(i+1));
    // Render frequency must neither integrate the camera again nor consume camera-shake ticks.
    field<int>(camera,0xe0)=10; field<float>(camera,0xe4)=.06f;
    beginTick(); processCamera(); assert(field<int>(camera,0xe0)==9);
    for(int i=0;i<100;++i) { renderPhase=float(i)/100; renderCamera(); visual_camera::finishFrame(); }
    assert(field<int>(camera,0xe0)==9 && cameraMoves==3 && cameraRenders==103);
    auto cut=visual_camera::between({{0,0,6},{0,0,0}},{{100,0,6},{100,0,0}},.5f);
    assert(cut.eye.x==100);
    auto orbit=visual_camera::between({{0,0,6},{0,0,0}},{{6,0,0},{0,0,0}},.5f);
    assert(std::fabs(dot(orbit.eye,orbit.eye)-36)<1e-4f);
    visual_camera::uninstall(); assert(!visual_camera::pending);
    originalDestroy=reinterpret_cast<Destroy>(&fakeDestroy);
    assert(destroyHook(object,nullptr,0)==object && !history.contains(object));
    puts("Current-time visual callbacks: draw-scoped animation, native geometry placement, guarded restoration, no extra simulation calls, camera and quaternion tests passed.");
}
