#pragma once
#include "settings.hpp"

class Optimization {
  public:
    ~Optimization();
    bool supported();
    void update(const Settings &);
};
