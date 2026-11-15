#include "native_sfx_follow.hpp"
#include <cassert>
#include <cstdio>

void Log(const char *) {}
namespace {
using namespace native_sfx_follow;
void *source=nullptr,*destination=nullptr,*ownerModel=nullptr;
int handCalls=0,forceCalls=0,positionCalls=0,boneRefreshCalls=0;
bool freshBoneResult=true;
bool freshBones(void *p) { assert(p==ownerModel);++boneRefreshCalls;return freshBoneResult; }
BOOL __fastcall lookup(void *,void *,uint32_t id,void **out) { *out=id==11 ? source : id==22 ? destination : nullptr;return *out!=nullptr; }
void __fastcall move(void *p,void *,const Vec *v) { field<Vec>(p,0x160)=*v;field<DWORD>(p,4)|=8;++positionCalls; }
void turn(void *p,float value) { field<float>(p,0x18)=value; }
void __fastcall handPoint(void *p,void *,Vec *out,int part,const float *world) {
    assert(p==ownerModel && (part==9 || part==10));++handCalls;*out={world[12]+float(part),world[13],world[14]};
}
void __fastcall forcePoint(void *p,void *,Vec *out,int index,int part,const float *world) {
    assert(p==ownerModel && index==0 && part==10);++forceCalls;*out={world[12]+1,world[13]+2,world[14]+3};
}
Matrix identity(float x=0,float y=0,float z=0) { return {1,0,0,0,0,1,0,0,0,0,1,0,x,y,z,1}; }
struct Fixture {
    alignas(16) unsigned char effect[2048]{},primary[512]{},actor[1024]{},target[1024]{},model[512]{},bones[512]{};
    Matrix bone[2]={identity(1,2,3),identity(2,3,4)};
    void *vtable[1]{};
    Fixture(Kind kind) {
        installed=true;hybrid_time::frame={};hybrid_time::frame.active=true;hybrid_time::frame.legacy=0;
        hybrid_time::frame.thread=GetCurrentThreadId();hybrid_time::frame.stage=hybrid_time::Stage::World;
        resolve=reinterpret_cast<Resolve>(&lookup);setPos=reinterpret_cast<SetPos>(&move);setAngle=&turn;
        hand=reinterpret_cast<Hand>(&handPoint);force=reinterpret_cast<Force>(&forcePoint);ctrlMap=reinterpret_cast<void *>(1);
        source=actor;destination=target;ownerModel=model;handCalls=forceCalls=positionCalls=0;
        bindings.clear();bindings.emplace(vtable,kind);field<void **>(effect,0)=vtable;
        field<void *>(effect,0x5f4)=primary;field<uint32_t>(effect,0x5f8)=11;field<uint32_t>(effect,0x5fc)=22;
        field<int>(effect,0x5f0)=99;field<WORD>(primary,0x150)=77;
        field<Vec>(actor,0x160)={10,20,30};field<float>(actor,0x18)=40;field<DWORD>(actor,0x170)=5;
        field<Vec>(target,0x160)={100,200,300};field<float>(target,0x18)=80;field<DWORD>(target,0x170)=5;
        field<void *>(actor,0x178)=model;field<void *>(model,0x100)=bones;field<void *>(model,0x118)=bone;
        field<int>(bones,0x44)=2;field<int>(bones,0x150)=0;field<int>(bones,0x154)=1;
        field<int>(bones,0x1c4)=0;field<Vec>(bones,0x16c)={7,8,9};
        field<int>(bones,0x1c8)=0;field<Vec>(bones,0x178)={4,5,6};
        field<int>(bones,0x1dc)=0;field<Vec>(bones,0x1b4)={2,3,4};
        const auto world=identity(10,20,30);std::memcpy(actor+0xe4,world.data(),64);
    }
    void run() { assert(refresh(effect));assert(field<int>(effect,0x5f0)==99 && field<WORD>(primary,0x150)==77); }
};
void vec(Vec actual,Vec expected) { assert(std::fabs(actual.x-expected.x)<.0001f && std::fabs(actual.y-expected.y)<.0001f && std::fabs(actual.z-expected.z)<.0001f); }
void testSimpleFollow() {
    Fixture f(Kind::Destination);f.run();vec(field<Vec>(f.effect,0x160),{100,200,300});
    field<DWORD>(f.target,4)=1;field<Vec>(f.target,0x160)={900,900,900};f.run();vec(field<Vec>(f.effect,0x160),{100,200,300});
    field<uint32_t>(f.effect,0x5fc)=UINT32_MAX;positionCalls=0;f.run();assert(!positionCalls);
    bindings[f.vtable]=Kind::Source;f.run();vec(field<Vec>(f.effect,0x160),{10,20,30});
    bindings[f.vtable]=Kind::Stretch;f.run();vec(field<Vec>(f.effect,0x160),{10,20.1f,30});assert(field<float>(f.effect,0x18)==-40);
    bindings[f.vtable]=Kind::NoOp;unsigned char before[2048];std::memcpy(before,f.effect,sizeof(before));f.run();
    assert(std::memcmp(before,f.effect,sizeof(before))==0);
    bindings.clear();assert(!refresh(f.effect));assert(std::memcmp(before,f.effect,sizeof(before))==0);
}
void testBonesAndHands() {
    Fixture f(Kind::Charge);f.run();vec(field<Vec>(f.effect,0x160),{15,27,39});assert(field<float>(f.effect,0x18)==-40);
    bindings[f.vtable]=Kind::Cannon;f.run();vec(field<Vec>(f.effect,0x160),{18,30,42});
    bindings[f.vtable]=Kind::Mushmoot;field<int>(f.effect,0x684)=7;f.run();vec(field<Vec>(f.effect,0x160),{13,25,37});
    positionCalls=0;field<int>(f.effect,0x684)=99;f.run();assert(!positionCalls);
    bindings[f.vtable]=Kind::Parts;field<int>(f.effect,0x688)=0;f.run();vec(field<Vec>(f.effect,0x160),{20,20,30});
    field<int>(f.effect,0x688)=1;f.run();vec(field<Vec>(f.effect,0x160),{19,20,30});assert(handCalls==2);
    bindings[f.vtable]=Kind::AllowParts;field<int>(f.effect,0x684)=1;f.run();assert(handCalls==3);
    bindings[f.vtable]=Kind::Collect;f.run();vec(field<Vec>(f.effect,0x160),{11,22,33});assert(forceCalls==1);
    field<int>(f.bones,0x150)=10;f.run();assert(forceCalls==1); // invalid bone indices never reach native helpers
    // Animated attachment reads require a fresh authoritative pose, never the previous Render's bones.
    field<int>(f.bones,0x150)=0;field<void *>(f.model,0x108)=reinterpret_cast<void *>(1);
    boneRefresh=&freshBones;boneRefreshCalls=0;freshBoneResult=true;
    f.run();assert(boneRefreshCalls==1 && forceCalls==2);
    bindings[f.vtable]=Kind::Cannon;f.run();assert(boneRefreshCalls==2);
    freshBoneResult=false;positionCalls=0;f.run();assert(boneRefreshCalls==3 && positionCalls==0);
    bindings[f.vtable]=Kind::Parts;f.run();assert(handCalls==3); // failed refresh cannot use stale hand matrices
    boneRefresh=&native_animation::refreshBones;
}
void testEmbeddedEffects() {
    Fixture f(Kind::SecondaryPlusOne);f.run();vec(field<Vec>(f.effect,0x444),{100,201,300});
    assert(positionCalls==0); // endpoint refresh cannot move the projectile itself
    bindings[f.vtable]=Kind::SecondaryScaled;f.run();vec(field<Vec>(f.effect,0x444),{100,200.3f,300});vec(field<Vec>(f.effect,0x45c),{2,2,2});
    bindings[f.vtable]=Kind::Wave;field<float>(f.effect,0x6a4)=3;f.run();vec(field<Vec>(f.effect,0x45c),{3,3,3});
    bindings[f.vtable]=Kind::PrimaryYaw;f.run();assert(field<float>(f.primary,0x13c)==140);
    bindings[f.vtable]=Kind::HotAir;f.run();vec(field<Vec>(f.primary,0x12c),{10,20,30});
    vec(field<Vec>(f.primary,0x138),{0,-40,0});vec(field<Vec>(f.effect,0x444),{100,200,300});
    field<DWORD>(f.actor,4)=1;field<DWORD>(f.target,4)=1;
    unsigned char before[2048];std::memcpy(before,f.effect,sizeof(before));f.run();assert(std::memcmp(before,f.effect,sizeof(before))==0);
}
void testNativeSkills() {
    Fixture f(Kind::Brandish);f.run();vec(field<Vec>(f.effect,0x160),{100,201,300});assert(field<float>(f.effect,0x18)==100);
    bindings[f.vtable]=Kind::OverCutter;f.run();assert(field<float>(f.effect,0x18)==140);
    bindings[f.vtable]=Kind::CleanHit;field<Vec>(f.effect,0x600)={4,5,6};f.run();vec(field<Vec>(f.effect,0x160),{4,5,6});
    hybrid_time::frame.stage=hybrid_time::Stage::Legacy;assert(!refresh(f.effect));
}
}
int main() {
    testSimpleFollow();testBonesAndHands();testEmbeddedEffects();testNativeSkills();
    native_sfx_follow::uninstall();assert(!native_sfx_follow::install());
    puts("Native SFX follow: explicit classes, source/destination/bone poses and impact endpoints passed without advancing effects.");
}
