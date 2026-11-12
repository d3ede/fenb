#pragma once
#include "common.hpp"

struct Settings {
    bool enabled = true, bloom = true, color = true;
    float bloomStrength = .22f, threshold = .72f, knee = .15f, radius = 1.6f;
    int bloomScale = 4;
    float exposure = 0.f, contrast = 1.f, saturation = 1.f;
    float temperature = 0.f, tint = 0.f, gamma = 1.f;
    float shadows = 0.f, highlights = 0.f;
    bool water = true;
    float waterWaves = .035f, waterLength = 5.f, waterSpeed = .7f;
    float waterRipples = .35f, waterRippleScale = 2.f;
    float waterOpacity = .55f, waterSky = .25f, waterGlints = .65f, waterGloss = .65f;
    float waterTint = .25f;
    float waterRefraction = .35f, waterTexture = .05f;
    bool openOnStart = false;
    bool unlockFps = false; // Opt-in rendering unlock with a fixed 16 ms simulation step.
    int panelPage = 0;
    bool operator==(const Settings &) const = default;
    void sanitize();
    bool load(const std::wstring &path);
    bool save(const std::wstring &path) const;
    void preset(int index);
    void resetColor();
    void resetBloom();
    void resetWater();
};
