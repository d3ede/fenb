#include "visual_animation.hpp"
#include "fixed_step.hpp"
#include <cassert>
#include <cstdio>
#include <functional>

void Log(const char *) {}
namespace {
using namespace visual_animation;
using visual_transform::Vec;
float seenFrame=0,seenBlend=0;
std::function<void(void *,const float *)> duringDraw;
int __fastcall drawModel(void *p,void *,void *,const float *world) {
    seenFrame=field<float>(p,0xa0);seenBlend=field<float>(p,0xe8);
    field<unsigned char *>(p,0x118)[0]=0xa5;
    if(duringDraw) duringDraw(p,world);
    return 1;
}
void __fastcall advanceModel(void *p,void *,void *,float speed,int) {
    field<float>(p,0xa0)=std::fmod(field<float>(p,0xa0)+speed,float(field<int>(p,0xa8)));
    field<float>(p,0xe8)+=.05f;
}
void *__fastcall deleteModel(void *p,void *,unsigned) { return p; }
struct Fixture {
    alignas(16) unsigned char model[512]{},motion[128]{},otherMotion[128]{},bones[128]{},otherBones[192]{};
    std::array<float,16> matrix=visual_transform::compose({{},{1,1,1},{}});
    Fixture() {
        history.clear();duringDraw={};
        enabled=true;ownerThread=GetCurrentThreadId();tick=0;renderPhase=.5f;
        originalRender=reinterpret_cast<Render>(&drawModel);originalMove=reinterpret_cast<Move>(&advanceModel);
        originalDestroy=reinterpret_cast<Destroy>(&deleteModel);
        field<void *>(model,0x108)=motion;field<unsigned char *>(model,0x118)=bones;
        field<int>(motion,0x54)=2;field<int>(otherMotion,0x54)=2;
        field<int>(model,0xa8)=20;field<int>(model,0xb4)=4;field<int>(model,0xe4)=1;
        field<float>(model,0xa0)=5;field<float>(model,0xe8)=.1f;
        beginTick();moveHook(model,nullptr,nullptr,1,0);
    }
    void draw() { assert(renderHook(model,nullptr,nullptr,matrix.data())==1); }
    ~Fixture() { duringDraw={};finishFrame();history.clear(); }
};
void testModelRestore() {
    {
        Fixture f;f.draw();assert(seenFrame==6.5f);
        assert(field<float>(f.model,0xa0)==6 && f.bones[0]==0 && !history[f.model].pending);
        f.draw();assert(seenFrame==6.5f && f.bones[0]==0);
        field<float>(f.model,0xa0)=0;finishFrame();assert(field<float>(f.model,0xa0)==0);
    }
    {
        Fixture f;duringDraw=[](void *p,const float *) { field<float>(p,0xe8)=.7f; };f.draw();
        assert(field<float>(f.model,0xa0)==6 && field<float>(f.model,0xe8)==.7f && f.bones[0]==0xa5);
    }
    {
        Fixture f;duringDraw=[](void *p,const float *) { field<float>(p,0xa0)=0;field<float>(p,0xe8)=0; };f.draw();
        assert(field<float>(f.model,0xa0)==0 && field<float>(f.model,0xe8)==0);
    }
    {
        Fixture f;duringDraw=[&](void *p,const float *) { field<void *>(p,0x108)=f.otherMotion;field<float>(p,0xa0)=2; };f.draw();
        assert(field<float>(f.model,0xa0)==2 && field<void *>(f.model,0x108)==f.otherMotion);
    }
    for(bool countChange : {false,true}) {
        Fixture f;duringDraw=[&](void *p,const float *) {
            if(countChange) field<int>(f.motion,0x54)=3;
            else field<unsigned char *>(p,0x118)=f.otherBones;
        };f.draw();
        assert(field<float>(f.model,0xa0)==6);
        assert(f.bones[0]==0xa5 && f.otherBones[0]==0); // Do not write obsolete/new bone storage.
    }
    {
        Fixture f;field<float>(f.model,0xe8)=.8f;f.draw();
        assert(seenFrame==6.5f && seenBlend==.8f);
        assert(field<float>(f.model,0xa0)==6 && field<float>(f.model,0xe8)==.8f);
    }
    {
        Fixture f;duringDraw=[](void *p,const float *) { field<int>(p,0x94)=4; };f.draw();
        duringDraw={};f.draw();assert(seenFrame==6 && field<int>(f.model,0x94)==4);
    }
    {
        Fixture f;bool nested=false;
        duringDraw=[&](void *p,const float *world) {
            assert(field<float>(p,0xa0)==6.5f);
            if(!nested) { nested=true;renderHook(p,nullptr,nullptr,world);assert(field<float>(p,0xa0)==6.5f); }
        };f.draw();assert(nested && field<float>(f.model,0xa0)==6 && f.bones[0]==0);
    }
    {
        Fixture f;duringDraw=[](void *,const float *) {
            for(uintptr_t i=1;i<512;++i) history[reinterpret_cast<void *>(i)];
        };f.draw();assert(field<float>(f.model,0xa0)==6 && f.bones[0]==0);
    }
    {
        Fixture f;duringDraw=[](void *p,const float *) { assert(destroyHook(p,nullptr,0)==p); };f.draw();
        assert(!history.contains(f.model)); // Outer draw must not touch a model destroyed in its callback.
    }
}
void testCombatPlacement() {
    // Both the local player and a remote mover use native world placement throughout
    // pre-draw shadows, geometry, picking/range checks and post-draw skill spawning.
    for(bool local : {true,false}) for(int fps : {30,60,144,240,500}) {
        Fixture f;alignas(16) std::array<unsigned char,1024> actor{};
        field<void *>(actor.data(),0x178)=f.model;
        field<Vec>(actor.data(),0x160)={local?10.f:11.5f,2,30};
        field<Vec>(actor.data(),0x18)={350,15,0};
        f.matrix[12]=field<Vec>(actor.data(),0x160).x;f.matrix[13]=2;f.matrix[14]=30;
        FixedStep scheduler;
        for(int frame=0;frame<fps*2;++frame) {
            const unsigned steps=scheduler.advance(int64_t(frame)*1000000/fps,1000000);
            for(unsigned step=0;step<steps;++step) {
                beginTick();moveHook(f.model,nullptr,nullptr,1,0);
                field<Vec>(actor.data(),0x160).x+=.25f;
                field<Vec>(actor.data(),0x18).x+=1.f;
            }
            const auto originalActor=actor;
            const Vec shadow=field<Vec>(actor.data(),0x160);
            f.matrix[12]=shadow.x;
            const float nativeFrame=field<float>(f.model,0xa0);
            duringDraw=[&](void *,const float *world) {
                assert(actor==originalActor);
                assert(std::equal(f.matrix.begin(),f.matrix.end(),world));
                assert(world[12]==shadow.x && world[13]==shadow.y && world[14]==shadow.z);
            };
            renderPhase=scheduler.fraction(1000000);
            f.draw();f.draw(); // Main geometry and an attachment pass at identical native placement.
            const Vec skillOrigin=field<Vec>(actor.data(),0x160);
            assert(skillOrigin.x==shadow.x && skillOrigin.y==shadow.y && skillOrigin.z==shadow.z);
            assert(actor==originalActor && f.bones[0]==0 && field<float>(f.model,0xa0)==nativeFrame);
            finishFrame();assert(actor==originalActor);
            duringDraw={};
        }
    }
}
}
int main() {
    testModelRestore();testCombatPlacement();
    puts("Visual callbacks: native world placement, immediate pose restoration, render-time mutations, nested draws and model lifetime passed.");
}
