#pragma once
#include "settings.hpp"

namespace puddle_detail {
// Session-local rain history; no per-pixel history, readbacks or configuration writes.
struct Accumulation {
    float water = 0;
    ULONGLONG tick = 0;
    bool sampled = false;
    void update(const Settings &s, float rain, ULONGLONG now, bool active = true) {
        const float dt = sampled && now >= tick ? float(std::min<ULONGLONG>(now-tick, 1000))*.001f : 0;
        tick = now; sampled = true;
        if (!s.puddles) { water = 0; return; }
        if (!active || !s.enabled) return;
        rain = std::clamp(rain, 0.f, 1.f);
        const float rate = rain>0 ? rain/std::max(1.f,s.puddleFillTime) : -1/std::max(1.f,s.puddleDryTime);
        water = std::clamp(water + dt*rate, 0.f, 1.f);
    }
};
}
