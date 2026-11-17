#pragma once
#include "settings.hpp"

class Optimization {
  public:
    ~Optimization();
    bool supported();
    bool effectsSupported();
    bool petEffectsSupported();
    bool actorVisibilitySupported();
    float actorDistanceMaximum();
    void update(const Settings &);
};
