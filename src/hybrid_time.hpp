#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Native client time domain: .016 seconds is one Homesickness simulation step.
// Continuous work uses ticks. Counters and legacy services consume legacy steps.
namespace hybrid_time {
enum class Stage { Idle, Legacy, World, Input, Camera };
struct Frame {
    float ticks=1;
    unsigned legacy=1;
    uint64_t serial=0;
    uint32_t thread=0;
    bool active=false;
    Stage stage=Stage::Idle;
    float fraction=0;
};
inline Frame frame;
class Scope {
    Frame saved_;
  public:
    explicit Scope(Stage stage):saved_(frame) { frame.stage=stage; }
    Scope(Stage stage,float ticks,unsigned legacy):saved_(frame) { frame.stage=stage;frame.ticks=ticks;frame.legacy=legacy; }
    ~Scope() { frame=saved_; }
    Scope(const Scope&)=delete;Scope&operator=(const Scope&)=delete;
};
class Clock {
  public:
    void reset() { started_=false; credit_=0; }
    Frame advance(int64_t now,int64_t frequency) {
        Frame result;result.serial=++serial_;result.active=true;
        if(frequency<=0 || frequency>10000000000LL) { result.ticks=0;result.legacy=0;return result; }
        if(!started_ || now<last_ || frequency!=frequency_ || now-last_>frequency/4) {
            started_=true;last_=now;frequency_=frequency;credit_=0;
            return result;
        }
        const int64_t elapsed=now-last_;
        last_=now;
        result.ticks=float(double(elapsed)*1000.0/(double(frequency)*16.0));
        credit_+=elapsed*1000;
        const int64_t step=frequency*16;
        result.legacy=unsigned(credit_/step);credit_%=step;
        result.fraction=float(double(credit_)/double(step));
        return result;
    }
  private:
    int64_t last_=0,frequency_=0,credit_=0;
    uint64_t serial_=0;
    bool started_=false;
};
}
