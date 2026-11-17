#pragma once
#include "settings.hpp"

class Optimization {
  public:
    ~Optimization();
    bool supported();
    bool effectsSupported();
    void update(const Settings &);
};
