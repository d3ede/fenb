#include "native_hybrid.hpp"
#include <cassert>
#include <cstdio>
void Log(const char *) {}
namespace {
struct Model { alignas(16) unsigned char bytes[512]{};unsigned calls=0;float angle=0; };
unsigned endAt=0,destroyed=0;
BOOL __thiscall process(void *p) {
    auto &model=*static_cast<Model *>(p);++model.calls;model.angle+=3;
    return ++native_sfx_time::field<WORD>(p,0x150)>=endAt && endAt!=0;
}
void *__thiscall destroy(void *p,unsigned flags) { assert(flags==1);++destroyed;return p; }
void start(const hybrid_time::Frame &frame) {
    hybrid_time::frame=frame;hybrid_time::frame.thread=GetCurrentThreadId();
    native_sfx_time::beginFrame(hybrid_time::frame);
}
void testRates() {
    for(int fps:{30,60,144,240,500,1000})for(bool world:{false,true}) {
        native_sfx_time::reset();native_sfx_time::installed=true;native_sfx_time::original=&process;
        hybrid_time::Clock clock;Model model;endAt=0;
        for(int frame=0;frame<=fps*2;++frame) {
            auto time=clock.advance(int64_t(frame)*1000000/fps,1000000);start(time);
            if(world)native_hybrid::continuousSubsteps(time,[&] { native_sfx_time::processHook(&model,nullptr); });
            for(int pass=0;pass<4;++pass)assert(!native_sfx_time::processHook(&model,nullptr));
        }
        assert(model.calls==126 && model.angle==378 && native_sfx_time::field<WORD>(&model,0x150)==126);
    }
}
void testEndAndLifetime() {
    native_sfx_time::reset();native_sfx_time::installed=true;native_sfx_time::original=&process;
    native_sfx_time::originalDestroy=&destroy;endAt=2;Model model;
    hybrid_time::Frame frame{.ticks=3,.legacy=3,.serial=1,.thread=GetCurrentThreadId(),.active=true};start(frame);
    assert(native_sfx_time::processHook(&model,nullptr));assert(model.calls==2);
    assert(!native_sfx_time::processHook(&model,nullptr) && model.calls==2);
    native_sfx_time::field<WORD>(&model,0x150)=0; // Native looping caller owns this reset.
    assert(!native_sfx_time::processHook(&model,nullptr) && model.calls==3);
    ++frame.serial;frame.legacy=1;start(frame);
    assert(native_sfx_time::processHook(&model,nullptr) && model.calls==4);
    ++frame.serial;start(frame);
    assert(native_sfx_time::processHook(&model,nullptr) && model.calls==5);
    assert(native_sfx_time::destroyHook(&model,nullptr,1)==&model && destroyed==1 && native_sfx_time::clocks.empty());
    model={};++frame.serial;frame.legacy=0;start(frame);
    assert(!native_sfx_time::processHook(&model,nullptr) && !model.calls);
    ++frame.serial;frame.legacy=1;start(frame);
    assert(!native_sfx_time::processHook(&model,nullptr) && model.calls==1);
}
void testLegacyAndFallback() {
    native_sfx_time::reset();native_sfx_time::installed=true;native_sfx_time::original=&process;
    Model model;endAt=0;
    hybrid_time::Frame frame{.ticks=3,.legacy=3,.serial=1,.thread=GetCurrentThreadId(),.active=true};start(frame);
    {
        hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1,1);
        for(int i=0;i<3;++i)assert(!native_sfx_time::processHook(&model,nullptr));
    }
    for(int pass=0;pass<4;++pass)assert(!native_sfx_time::processHook(&model,nullptr));
    assert(model.calls==3);
    native_sfx_time::installed=false;native_sfx_time::processHook(&model,nullptr);assert(model.calls==4);
}
}
int main() {
    testRates();testEndAndLifetime();testLegacyAndFallback();native_sfx_time::uninstall();
    assert(!native_sfx_time::install());
    puts("Native SFX timing: 30..1000 FPS, shared world/render budgets, multipass, loop/expiry ownership, destruction and address reuse passed.");
}
