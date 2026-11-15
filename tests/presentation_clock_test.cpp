#include "presentation_clock.hpp"
#include "fixed_step.hpp"
#include <cassert>
#include <cstdio>

int main() {
    constexpr int64_t frequency=1000000000;
    for(int fps:{30,60,144,240,500,1000}) {
        presentation_clock::Timeline clock;
        FixedStep simulation;
        double position=0,last=-1;
        unsigned updates=0;
        for(int i=0;i<=fps*10;++i) {
            int64_t now=int64_t(double(i)*frequency/fps);
            unsigned steps=simulation.advance(now,frequency);
            updates+=steps;position+=steps*.016*5;
            const auto &frame=clock.sample(now,frequency,simulation.fraction(frequency));
            double visible=position+frame.fraction*.016*5;
            // An analytical 5 units/s path must remain continuous across native tick boundaries.
            double reference=.016*5+double(now)/frequency*5;
            assert(std::fabs(visible-reference)<1e-5);
            assert(visible>=last);last=visible;
        }
        assert(updates==626);
    }
    presentation_clock::Timeline clock;
    auto first=clock.sample(0,frequency,0);assert(first.discontinuity && first.deltaSeconds==0);
    auto smooth=clock.sample(4000000,frequency,.25f);assert(!smooth.discontinuity && smooth.fraction==.25f);
    assert(std::fabs(smooth.deltaSeconds-.004)<1e-12);
    auto stall=clock.sample(400000000,frequency,.7f);assert(stall.discontinuity && stall.fraction==0 && stall.epoch>smooth.epoch);
    auto backwards=clock.sample(100,frequency,.4f);assert(backwards.discontinuity && backwards.fraction==0);
    auto invalid=clock.sample(200,0,.4f);assert(invalid.discontinuity && invalid.fraction==0);
    clock.sample(300,frequency,0);
    auto nan=clock.sample(400,frequency,std::nanf(""));assert(nan.fraction==0);
    clock.reset();assert(clock.value.discontinuity && clock.value.deltaSeconds==0 && clock.value.fraction==0);
    FixedStep catchup;catchup.advance(0,frequency);
    assert(catchup.advance(200000000,frequency)==8 && catchup.discontinuous());
    auto dropped=clock.sample(200000000,frequency,.5f,catchup.discontinuous());assert(dropped.discontinuity && dropped.fraction==0);
    puts("Presentation clock: continuous analytical motion across 30/60/144/240/500/1000 FPS; reset/stall/backwards/invalid input passed.");
}
