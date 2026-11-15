#include "visual_mover.hpp"
#include <cassert>
#include <cstdio>

void Log(const char *) {}
namespace {
using namespace visual_animation;
using visual_transform::Vec;
float seenFrame=0,seenBlend=0;
int __fastcall drawModel(void *p,void *,void *,const float *) {
    seenFrame=field<float>(p,0xa0);seenBlend=field<float>(p,0xe8);
    field<unsigned char *>(p,0x118)[0]=0xa5;return 1;
}
void __fastcall advanceModel(void *p,void *,void *,float speed,int) {
    field<float>(p,0xa0)+=speed;field<float>(p,0xe8)+=.05f;
}
struct Fixture {
    alignas(16) unsigned char model[512]{},motion[128]{},otherMotion[128]{},bones[128]{},otherBones[192]{};
    std::array<float,16> matrix=visual_transform::compose({{},{1,1,1},{}});
    Fixture() {
        visual_mover::history.clear();visual_animation::history.clear();
        enabled=true;ownerThread=GetCurrentThreadId();tick=0;renderPhase=.5f;
        originalRender=reinterpret_cast<Render>(&drawModel);originalMove=reinterpret_cast<Move>(&advanceModel);
        field<void *>(model,0x108)=motion;field<unsigned char *>(model,0x118)=bones;
        field<int>(motion,0x54)=2;field<int>(otherMotion,0x54)=2;
        field<int>(model,0xa8)=20;field<int>(model,0xb4)=4;field<int>(model,0xe4)=1;
        field<float>(model,0xa0)=5;field<float>(model,0xe8)=.1f;
        beginTick();moveHook(model,nullptr,nullptr,1,0);
    }
    void draw() { renderHook(model,nullptr,nullptr,matrix.data()); }
    ~Fixture() { finishFrame();visual_animation::history.clear();visual_mover::history.clear(); }
};
void testModelRestore() {
    {
        Fixture f;f.draw();assert(seenFrame==6.5f);
        field<float>(f.model,0xe8)=.7f;finishFrame();
        assert(field<float>(f.model,0xa0)==6 && field<float>(f.model,0xe8)==.7f);
    }
    {
        Fixture f;f.draw();field<float>(f.model,0xa0)=0;field<float>(f.model,0xe8)=0;finishFrame();
        assert(field<float>(f.model,0xa0)==0 && field<float>(f.model,0xe8)==0);
    }
    {
        Fixture f;f.draw();field<void *>(f.model,0x108)=f.otherMotion;
        field<float>(f.model,0xa0)=2;finishFrame();assert(field<float>(f.model,0xa0)==2);
    }
    for(bool countChange : {false,true}) {
        Fixture f;f.draw();
        if(countChange) field<int>(f.motion,0x54)=3;
        else field<unsigned char *>(f.model,0x118)=f.otherBones;
        finishFrame();
        assert(field<float>(f.model,0xa0)==6);
        assert(f.bones[0]==0xa5 && f.otherBones[0]==0); // no copy into an obsolete/new buffer
    }
    {
        Fixture f;field<float>(f.model,0xe8)=.8f;f.draw();
        assert(seenFrame==6.5f && seenBlend==.8f);finishFrame();
        assert(field<float>(f.model,0xa0)==6 && field<float>(f.model,0xe8)==.8f);
    }
    {
        Fixture f;f.draw();field<int>(f.model,0x94)=4;f.draw();
        assert(seenFrame==6 && field<int>(f.model,0x94)==4);
    }
}
void __fastcall drawMover(void *,void *,void *) {}
void setupMover(unsigned char *actor,void *model,void *world) {
    using namespace visual_mover;
    field<void *>(actor,0x178)=model;field<void *>(actor,0x16c)=world;
    auto identity=visual_transform::compose({{},{1,1,1},{}});
    std::memcpy(actor+0xa4,identity.data(),64);
    field<Vec>(actor,0x160)={2,0,0};
    auto &h=visual_mover::history[actor];h.current=read(actor);h.previous=h.current;
    h.previous.position={1,0,0};h.previous.angles={350,0,0};h.current.angles={10,0,0};
    field<Vec>(actor,0x18)=h.current.angles;
    h.local=true;h.updated=tick;h.epoch=presentation_clock::frame.epoch;
    active=true;visual_mover::originalRender=reinterpret_cast<visual_mover::Render>(&drawMover);
}
void testMoverRestore() {
    {
        Fixture f;alignas(16) unsigned char actor[1024]{};void *world=reinterpret_cast<void *>(123);
        setupMover(actor,f.model,world);auto logical=visual_mover::read(actor);
        visual_mover::renderHook(actor,nullptr,nullptr);
        assert(field<Vec>(actor,0x160).x==2.5f);
        field<Vec>(actor,0x160)={25,1,2};visual_mover::finishFrame();
        assert(field<Vec>(actor,0x160).x==25);
        assert(std::memcmp(actor+0xa4,logical.rotation.data(),64)==0 && (field<DWORD>(actor,4)&8));
    }
    {
        Fixture f;alignas(16) unsigned char actor[1024]{};
        setupMover(actor,f.model,reinterpret_cast<void *>(123));visual_mover::renderHook(actor,nullptr,nullptr);
        field<void *>(actor,0x178)=f.otherMotion;
        field<float>(actor,0xa4)=.25f;visual_mover::finishFrame();
        assert(field<Vec>(actor,0x160).x==2 && field<float>(actor,0xa4)==.25f);
    }
    {
        Fixture f;alignas(16) unsigned char actor[1024]{};
        setupMover(actor,f.model,reinterpret_cast<void *>(123));visual_mover::renderHook(actor,nullptr,nullptr);
        field<void *>(actor,0x16c)=reinterpret_cast<void *>(456);field<Vec>(actor,0x160)={50,0,0};
        visual_mover::finishFrame();assert(field<Vec>(actor,0x160).x==50);
    }
    {
        Fixture f;alignas(16) unsigned char actor[1024]{};
        setupMover(actor,f.model,reinterpret_cast<void *>(123));auto logical=visual_mover::read(actor);
        visual_mover::renderHook(actor,nullptr,nullptr);field<Vec>(actor,0x160)={30,0,0};
        auto corrected=visual_mover::presentation(actor);
        assert(corrected.position.x==30 && !visual_mover::history[actor].pending);
        assert(std::memcmp(actor+0xa4,logical.rotation.data(),64)==0);
    }
    visual_mover::active=false;
}
}
int main() {
    testModelRestore();testMoverRestore();
    puts("Visual restoration: independent native mutations, clip restarts, bone lifetime and actor corrections passed.");
}
