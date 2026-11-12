#pragma once
#include <algorithm>
#include <cstdint>

// Preserve Neuz's original 16 ms simulation cadence independently of render FPS.
class FixedStep {
  public:
    void reset() {
        started_ = false;
        credit_ = 0;
    }
    unsigned advance(int64_t now, int64_t frequency) {
        if (frequency <= 0 || frequency > 10000000000LL)
            return 0;
        if (!started_ || now < last_ || now - last_ > frequency / 4) {
            started_ = true;
            last_ = now;
            credit_ = 0;
            return 1; // Resume without replaying a loading screen or a long pause.
        }
        credit_ += (now - last_) * 1000;
        last_ = now;
        const auto step = frequency * 16;
        auto due = credit_ / step;
        credit_ %= step;
        return unsigned(std::min<int64_t>(due, 8));
    }

  private:
    int64_t last_ = 0, credit_ = 0;
    bool started_ = false;
};
