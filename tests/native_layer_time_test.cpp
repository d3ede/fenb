#include "native_layer_time.hpp"
#include <cassert>
#include <cstdio>
void Log(const char *) {}
int main() {
    float offsets[6],increment[]={.0002f,.0002f,.001f,.001f,.0015f,.0015f};
    for(int fps:{30,60,144,240,500,1000}) {
        for(size_t i=0;i<6;++i) {
            offsets[i]=i%2?-.5f:.25f;
            native_layer_time::offsets[i]=offsets+i;native_layer_time::increments[i]=increment+i;
        }
        native_layer_time::installed=true;native_layer_time::reset();
        hybrid_time::Clock clock;
        for(int frame=0;frame<=2*fps;++frame) {
            auto time=clock.advance(int64_t(frame)*1000000/fps,1000000);time.thread=GetCurrentThreadId();
            for(int pass=0;pass<4;++pass)native_layer_time::beginFrame(time);
        }
        for(size_t i=0;i<6;++i)assert(std::abs(offsets[i]-((i%2?-.5:.25)+126.0*increment[i]))<1e-7);
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
