#pragma once
#include "settings.hpp"

class Optimization {
  public:
    ~Optimization();
    bool supported();
    bool effectsSupported();
    bool petEffectsSupported();
    bool actorVisibilitySupported();
    void update(const Settings &);
};
