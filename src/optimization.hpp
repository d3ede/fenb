#pragma once
#include "settings.hpp"

class Optimization {
  public:
    ~Optimization();
    bool supported();
    bool effectsSupported();
    bool petEffectsSupported();
    void update(const Settings &);
};
