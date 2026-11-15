#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace presentation_clock {
struct Frame {
    double deltaSeconds=0;
    float fraction=0;
    uint64_t serial=0,epoch=0;
    bool discontinuity=true;
};
class Timeline {
  public:
    Frame value;
    void reset() { started_=false; ++value.epoch; value.deltaSeconds=0; value.fraction=0; value.discontinuity=true; }
    const Frame &sample(int64_t now,int64_t frequency,float residual,bool dropped=false) {
        ++value.serial;
        const bool valid=frequency>0 && frequency<=10000000000LL;
        const bool jump=dropped || !valid || !started_ || frequency!=frequency_ || now<last_ || now-last_>frequency/4;
        value.discontinuity=jump;
        if(jump) { ++value.epoch; value.deltaSeconds=0; value.fraction=0; }
        else {
            value.deltaSeconds=std::clamp(double(now-last_)/double(frequency),0.0,.05);
            value.fraction=std::isfinite(residual) ? std::clamp(residual,0.f,1.f) : 0;
        }
        last_=now;frequency_=frequency;started_=valid;
        return value;
    }
  private:
    int64_t last_=0,frequency_=0;
    bool started_=false;
};
inline Timeline timeline;
inline const Frame &frame=timeline.value;
inline void reset() { timeline.reset(); }
inline void sample(int64_t now,int64_t frequency,float residual,bool dropped=false) { timeline.sample(now,frequency,residual,dropped); }
}
