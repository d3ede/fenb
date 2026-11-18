#pragma once
#include "common.hpp"

// Injected only into the standalone showcase build, after Windows declarations.
// Freeze shader animation phases across off/on renders, including rain rings.
inline ULONGLONG FeatureShowcaseTick() { return 15000; }
#define GetTickCount64 FeatureShowcaseTick
