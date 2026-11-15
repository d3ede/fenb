#pragma once
#include <algorithm>
#include <cstdint>

// Preserve Neuz's original 16 ms simulation cadence independently of render FPS.
class FixedStep {
  public:
    bool discontinuous() const { return discontinuity_; }
    float fraction(int64_t frequency) const {
        return frequency > 0 ? float(double(credit_) / (double(frequency) * 16.0)) : 1.f;
    }
    void reset() {
        started_ = false;
        credit_ = 0;
        discontinuity_=true;
    }
    unsigned advance(int64_t now, int64_t frequency) {
        discontinuity_=false;
        if (frequency <= 0 || frequency > 10000000000LL) {
            discontinuity_=true;
            return 0;
        }
        if (!started_ || now < last_ || now - last_ > frequency / 4) {
            started_ = true;
            last_ = now;
            credit_ = 0;
            discontinuity_=true;
            return 1; // Resume without replaying a loading screen or a long pause.
        }
        credit_ += (now - last_) * 1000;
        last_ = now;
        const auto step = frequency * 16;
        auto due = credit_ / step;
        credit_ %= step;
        discontinuity_=due>8;
        return unsigned(std::min<int64_t>(due, 8));
    }

  private:
    int64_t last_ = 0, credit_ = 0;
    bool started_ = false;
    bool discontinuity_=true;
};
