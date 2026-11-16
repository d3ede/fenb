#include "native_animation.hpp"
#include <cassert>
#include <bit>
#include <cstdio>
#include <vector>

void Log(const char *) {}
namespace {
using namespace native_animation;
std::vector<int> sounds;
int quakes=0,trailCalls=0,fallbackCalls=0;
int boneCalls=0,destroyCalls=0;
std::array<uint32_t,8> boneArguments{};
void sampleBones(void *motion,float frame,void *output,void *oldMotion,int next,int anchor,int blending,float blend) {
    ++boneCalls;boneArguments={uint32_t(uintptr_t(motion)),std::bit_cast<uint32_t>(frame),uint32_t(uintptr_t(output)),
        uint32_t(uintptr_t(oldMotion)),uint32_t(next),uint32_t(anchor),uint32_t(blending),std::bit_cast<uint32_t>(blend)};
    *static_cast<float *>(output)=frame;
}
void *__fastcall destroy(void *p,void *,unsigned flags) { assert(flags==1);++destroyCalls;return p; }
#if FENB_NATIVE_X86_ASM
__declspec(naked) void samplerEntry() {
    __asm {
        mov dword ptr [boneArguments],ecx
        movss dword ptr [boneArguments+4],xmm3
        mov eax,[esp+4]
        mov dword ptr [boneArguments+8],eax
        mov eax,[esp+8]
        mov dword ptr [boneArguments+12],eax
        mov eax,[esp+12]
        mov dword ptr [boneArguments+16],eax
        mov eax,[esp+16]
        mov dword ptr [boneArguments+20],eax
        mov eax,[esp+20]
        mov dword ptr [boneArguments+24],eax
        mov eax,[esp+24]
        mov dword ptr [boneArguments+28],eax
        ret 24
    }
}
#endif
void sound(int id,void *) { sounds.push_back(id); }
void quake(bool) { ++quakes; }
void __fastcall trail(void *,void *) { ++trailCalls; }
void __fastcall fallback(void *,void *,void *,float,int) { ++fallbackCalls; }
struct Model {
    alignas(16) unsigned char data[512]{},motion[128]{},object[4096]{},element[16]{},sword[16]{};
    Attribute attr[20]{};
    Model() {
        enabled=true;
        hybrid_time::frame={};hybrid_time::frame.active=true;
        hybrid_time::frame.thread=GetCurrentThreadId();hybrid_time::frame.stage=hybrid_time::Stage::World;
        playSound=&sound;setQuake=&quake;trailProcess=reinterpret_cast<TrailProcess>(&trail);
        originalMove=reinterpret_cast<Move>(&fallback);
        field<void *>(data,0x108)=motion;field<Attribute *>(motion,4)=attr;
        field<int>(data,0xa8)=20;field<float>(data,0xac)=.5f;field<int>(data,0xb4)=4;
        field<float>(data,0xa4)=-1;
        for(int i=0;i<20;++i) attr[i]={0,100+i,float(i)};
        sounds.clear();quakes=trailCalls=fallbackCalls=0;
    }
    void step(float ticks,unsigned legacy=0,float speed=1,int silent=0) {
        hybrid_time::frame.ticks=ticks;hybrid_time::frame.legacy=legacy;++hybrid_time::frame.serial;
        moveHook(data,nullptr,nullptr,speed,silent);
    }
};
void testCrossRate() {
    for(int rate : {60,120,240,1000}) {
        Model m;field<int>(m.data,0xa8)=1000;
        // No attribute storage is accessed because the larger clip is test-only.
        field<Attribute *>(m.motion,4)=nullptr;
        for(int i=0;i<rate;++i) m.step(62.5f/float(rate));
        assert(std::fabs(field<float>(m.data,0xa0)-31.25f)<.001f);
    }
    Model m;field<int>(m.data,0x98)=1;
    m.step(.25f);assert(std::fabs(field<float>(m.data,0xa0)-.025f)<.000001f);
    field<int>(m.data,0xe4)=1;m.step(.5f,0,2);
    assert(std::fabs(field<float>(m.data,0xe8)-.025f)<.000001f);
    for(float speed:{.5f,1.f,2.f,4.f}) {
        Model transition;field<int>(transition.data,0xe4)=1;
        transition.step(1,1,speed);
        assert(field<float>(transition.data,0xe8)==.05f);
    }
}
void testNativeCompletion() {
    // Expected phases/end flags from the inspected native phase routine at one
    // native tick. Cover slow clips, attack-speed scaling and non-boolean slow.
    struct Case { float start,per,speed;int slow,loop;float phase;int end; };
    const Case cases[]={
        {18.5f,.5f,1,0,1,19,0}, {19,.5f,1,0,1,19.5f,0},
        {19.25f,.5f,1,0,1,19,1}, {17,1,2,0,1,19,1},
        {19.75f,.5f,1,0,4,.25f,1}, {19.5f,.5f,1,1,1,19.6f,0},
        {18.5f,.5f,2,2,1,19,1}, {18,.5f,2,0,1,19,0}
    };
    for(const auto &c:cases) {
        Model m;field<float>(m.data,0xa0)=c.start;field<float>(m.data,0xac)=c.per;
        field<int>(m.data,0x98)=c.slow;field<int>(m.data,0xb4)=c.loop;
        advancePhase(m.data,c.speed,1);
        assert(std::fabs(field<float>(m.data,0xa0)-c.phase)<.00001f && field<int>(m.data,0x9c)==c.end);
    }
    // Render partitioning must not shrink completion lookahead to a render
    // interval. Slower/faster attacks retain their native speed-based threshold.
    for(float speed:{.5f,1.f,1.655077f,2.f,4.f})for(int fps:{30,60,144,240,500,1000}) {
        Model m;field<int>(m.data,0xb4)=1;
        const double rate=.5*speed,threshold=20-rate,dt=62.5/double(fps);
        unsigned count=0;
        while(!field<int>(m.data,0x9c) && count<10000) { advancePhase(m.data,speed,float(dt));++count; }
        const double advanced=double(count)*dt*rate;
        assert(advanced>threshold-.001 && advanced<=threshold+dt*rate+.001);
        assert(field<float>(m.data,0xa0)==19);
    }
}
void testBaseModelPhases() {
    for(int fps:{30,60,144,240,500,1000}) {
        Model m;field<int>(m.data,0xa8)=1000;
        field<Attribute *>(m.motion,4)=nullptr;
        field<WORD>(m.data,0x150)=77;
        for(int i=0;i<fps;++i) {
            hybrid_time::frame.ticks=62.5f/float(fps);
            baseMoveHook(m.data,nullptr,nullptr,1,0);
        }
        assert(std::fabs(field<float>(m.data,0xa0)-31.25f)<.001f);
        assert(field<WORD>(m.data,0x150)==77 && sounds.empty() && quakes==0);
    }
    Model m;originalBaseMove=reinterpret_cast<Move>(&fallback);
    hybrid_time::frame.stage=hybrid_time::Stage::Legacy;
    baseMoveHook(m.data,nullptr,nullptr,1,0);assert(fallbackCalls==1);
}
void testIntervals() {
    Model m;m.attr[1].flags=3;m.attr[2].flags=7;m.attr[3].flags=2;
    field<float>(m.data,0xa4)=.5f;field<float>(m.data,0xa0)=3.5f;
    assert((hitHook(m.data,nullptr)&1)!=0);
    assert(firstCrossed(m.data,3.5f,2)==&m.attr[1]);
    assert(quakeHook(m.data,nullptr)&4);
    m.step(.5f);assert((sounds==std::vector<int>{101,102,103}) && quakes==1);
    m.step(.5f);assert(sounds.size()==3 && quakes==1);
    // Loop crossing includes late keys and frame zero, in chronological order.
    m.attr[19].flags=2;m.attr[0].flags=3;
    field<float>(m.data,0xa0)=18.5f;m.step(4);
    assert(field<float>(m.data,0xa0)==.5f && field<float>(m.data,0xa4)==-1.5f);
    assert(hitHook(m.data,nullptr)&1);
    const size_t before=sounds.size();m.step(.25f);
    assert(sounds.size()==before+2 && sounds[before]==119 && sounds[before+1]==100);
    // A fractional key is emitted only when crossed, not at every render in its bucket.
    m.attr[1]={3,201,1.25f};field<float>(m.data,0xa4)=1.1f;field<float>(m.data,0xa0)=1.3f;
    assert(hitHook(m.data,nullptr)&1);m.step(.1f);const size_t count=sounds.size();
    m.step(.1f);assert(sounds.size()==count);
    // Paused phase retires the preceding interval without replaying sound on resume.
    field<float>(m.data,0xa4)=1;field<float>(m.data,0xa0)=1.3f;field<int>(m.data,0x94)=3;
    m.step(.5f);assert(field<float>(m.data,0xa4)==1.3f);
    field<int>(m.data,0x94)=0;m.step(.5f);assert(sounds.size()==count);
}
void testCountersAndLegacy() {
    Model m;field<int>(m.data,0x94)=5;
    int before=capturePause(m.data);field<int>(m.data,0x94)=4;
    hybrid_time::frame.legacy=0;settlePause(m.data,before);assert(capturePause(m.data)==5);
    before=capturePause(m.data);field<int>(m.data,0x94)=4;
    hybrid_time::frame.legacy=3;settlePause(m.data,before);assert(capturePause(m.data)==2);
    before=capturePause(m.data);field<int>(m.data,0x94)=4;
    settlePause(m.data,before);assert(capturePause(m.data)==4); // a new hit stop wins
    field<int>(m.data,0x94)=0;field<void *>(m.data,0x10c)=m.sword;
    field<int>(m.sword,4)=100;
    m.step(.25f,0);assert(field<int>(m.sword,8)==0 && trailCalls==0);
    m.step(.25f,2);assert(field<int>(m.sword,8)==16 && trailCalls==1);
    hybrid_time::frame.stage=hybrid_time::Stage::Legacy;m.step(4,4);
    assert(fallbackCalls==1 && field<int>(m.sword,8)==16);
}
void testStaticModelAndClips() {
    Model m;field<void *>(m.data,0x108)=nullptr;
    field<void *>(m.data,0xf4)=m.element;field<void *>(m.data,0xf8)=m.element+16;
    field<void *>(m.element,4)=m.object;field<Attribute *>(m.object,0x48)=m.attr;
    field<int>(m.object,0xc1c)=20;
    m.attr[2]={7,302,2.2f};field<float>(m.data,0xa4)=1.5f;field<float>(m.data,0xa0)=2.5f;
    assert(hitHook(m.data,nullptr)&1);m.step(.5f);
    assert(sounds==std::vector<int>{302} && quakes==1);
    field<int>(m.data,0xb4)=0;field<float>(m.data,0xa0)=18.9f;m.step(4);
    assert(field<float>(m.data,0xa0)==19 && field<int>(m.data,0x9c)==1);
    field<float>(m.data,0xa4)=-1;field<float>(m.data,0xa0)=0;m.attr[0].flags=2;
    const size_t before=sounds.size();m.step(.25f,0,1,1);assert(sounds.size()==before);
    field<float>(m.data,0xa4)=-1;field<float>(m.data,0xa0)=0;m.step(.25f);
    assert(sounds.size()==before+1 && sounds.back()==100);
    assert(!flagsCrossed(m.data,std::numeric_limits<float>::quiet_NaN(),1));
}
void testBoneRefresh() {
    Model m;alignas(16) unsigned char skeleton[512]{},oldMotion[128]{};
    float output[32]{},replacement[32]{};void *modelTable[1]{};
    table=modelTable;field<void **>(m.data,0)=table;boneSampler=&sampleBones;boneCalls=destroyCalls=0;reset();
    field<void *>(m.data,0x100)=skeleton;field<void *>(m.data,0x118)=output;
    field<int>(skeleton,0x44)=2;field<int>(m.motion,8)=20;field<int>(m.motion,0x54)=2;
    field<void *>(m.motion,0x64)=output;field<void *>(m.motion,0x68)=output;
    field<float>(m.data,0xa0)=3.25f;field<float>(m.data,0xa4)=2.75f;
    assert(refreshBones(m.data) && boneCalls==1 && output[0]==3.25f && boneArguments[4]==4);
    assert(refreshBones(m.data) && boneCalls==1); // multiple attachments share one sample
    assert(field<float>(m.data,0xa0)==3.25f && field<float>(m.data,0xa4)==2.75f && sounds.empty() && !quakes);
    field<float>(m.data,0xa0)=3.5f;assert(refreshBones(m.data) && boneCalls==2);
    ++hybrid_time::frame.serial;assert(refreshBones(m.data) && boneCalls==3);
    field<void *>(m.data,0x118)=replacement;assert(refreshBones(m.data) && boneCalls==4 && replacement[0]==3.5f);
    field<void *>(m.data,0x104)=oldMotion;field<int>(oldMotion,8)=20;
    field<int>(oldMotion,0x54)=2;field<void *>(oldMotion,0x64)=output;
    field<int>(m.data,0xe4)=1;field<float>(m.data,0xe8)=.25f;field<int>(m.data,0x114)=7;
    assert(refreshBones(m.data) && boneCalls==5 && boneArguments[5]==7 && boneArguments[6]==1);
    field<int>(m.data,0x114)=20;assert(!refreshBones(m.data) && boneCalls==5);
    field<int>(m.data,0x114)=7;field<float>(m.data,0xa0)=19.5f;
    assert(refreshBones(m.data) && boneArguments[4]==0);
    field<int>(m.data,0xb4)=0;assert(refreshBones(m.data) && boneArguments[4]==19);
    field<int>(m.motion,0x54)=3;assert(!refreshBones(m.data));
    field<int>(m.motion,0x54)=2;hybrid_time::frame.stage=hybrid_time::Stage::Legacy;assert(!refreshBones(m.data));
    hybrid_time::frame.stage=hybrid_time::Stage::World;
    originalDestroy=reinterpret_cast<Destroy>(&destroy);assert(destroyHook(m.data,nullptr,1)==m.data);
    assert(destroyCalls==1 && boneCache.empty());
    field<void *>(m.data,0x108)=nullptr;assert(!refreshBones(m.data));
#if FENB_NATIVE_X86_ASM
    boneEntry=reinterpret_cast<void *>(&samplerEntry);uint32_t before=0,after=0;
    __asm mov before,esp
    nativeBones(m.motion,4.75f,output,oldMotion,5,7,1,.625f);
    __asm mov after,esp
    assert(before==after);
    const std::array<uint32_t,8> expected={uint32_t(uintptr_t(m.motion)),std::bit_cast<uint32_t>(4.75f),
        uint32_t(uintptr_t(output)),uint32_t(uintptr_t(oldMotion)),5,7,1,std::bit_cast<uint32_t>(.625f)};
    assert(boneArguments==expected);
#endif
    table=nullptr;boneSampler=nullptr;boneEntry=nullptr;reset();
}
}
int main() {
    testCrossRate();testNativeCompletion();testBaseModelPhases();testIntervals();testCountersAndLegacy();testStaticModelAndClips();testBoneRefresh();
    native_animation::enabled=false;
    assert(!native_animation::install()); // unsupported test EXE must fail closed
    puts("Native animation: float phases, interval events, legacy counters and cached event-free bone sampler ABI passed.");
}
