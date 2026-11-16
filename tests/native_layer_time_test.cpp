#include "native_layer_time.hpp"
#include <cassert>
#include <cstdio>
void Log(const char *) {}
int main() {
    float offsets[2],increment=.0002f;
    for(int fps:{30,60,144,240,500,1000}) {
        offsets[0]=.25f;offsets[1]=-.5f;
        native_layer_time::offsets[0]=offsets;native_layer_time::offsets[1]=offsets+1;
        native_layer_time::increment=&increment;native_layer_time::installed=true;native_layer_time::reset();
        hybrid_time::Clock clock;
        for(int frame=0;frame<=2*fps;++frame) {
            auto time=clock.advance(int64_t(frame)*1000000/fps,1000000);time.thread=GetCurrentThreadId();
            for(int pass=0;pass<4;++pass)native_layer_time::beginFrame(time);
        }
        assert(std::abs(offsets[0]-(.25+126.0*increment))<1e-7);
        assert(std::abs(offsets[1]-(-.5+126.0*increment))<1e-7);
        hybrid_time::Frame time{.ticks=1,.serial=100000,.thread=GetCurrentThreadId(),.active=false};
        const float before=offsets[0];native_layer_time::beginFrame(time);assert(offsets[0]==before);
        time.active=true;time.thread=GetCurrentThreadId()+1;native_layer_time::beginFrame(time);assert(offsets[0]==before);
        time.thread=GetCurrentThreadId();time.ticks=NAN;native_layer_time::beginFrame(time);assert(offsets[0]==before);
        time.ticks=1;offsets[0]=.1f;native_layer_time::beginFrame(time);assert(std::abs(offsets[0]-.1002f)<1e-7);
        native_layer_time::uninstall();assert(offsets[0]>.1f);
    }
    assert(!native_layer_time::install());
    puts("Native cloud-layer timing: elapsed-time drift at 30..1000 FPS, repeated passes, phase continuity, reset and inactive/thread guards passed.");
}
