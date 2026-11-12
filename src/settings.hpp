#pragma once
#include "common.hpp"
#include "localization.hpp"

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
    float waterDepth = 1.f, waterShore = .6f, waterSSR = .65f;
    int waterSSRQuality = 0;
    float waterCaustics = 0.f, waterDispersion = .15f;
    int waterCausticScale = 4;
#define SCENE_BOOL(g, m, k, d) bool m = d;
#define SCENE_FLOAT(g, m, k, d, lo, hi) float m = d;
#include "scene_settings.inc"
#undef SCENE_BOOL
#undef SCENE_FLOAT
    bool postEnabled() const {
        return bloom || color || ssao || sunRays || fog || dof || adaptation || localLighting || fxaa ||
               smaa || lightingBalance;
    }
    void resetSceneSection(int group);
    bool openOnStart = false;
    bool preciseWaterDepth = true;
    int msaaSamples = 0;    // 0 retains the native choice, -1 disables, otherwise 2/4/8.
    bool unlockFps = false; // Opt-in rendering unlock with a fixed 16 ms simulation step.
    bool timeOverride = false, freezeTime = true, weatherOverride = false;
    float timeMinutes = 720, weatherIntensity = 100;
    int weatherType = 0; // 0 clear, 1 rain, 2 snow; independent of server weather.
    int panelPage = 0;
    Language language = Language::English;
    bool operator==(const Settings &) const = default;
    void sanitize();
    bool load(const std::wstring &path);
    bool save(const std::wstring &path) const;
    void preset(int index);
    void resetColor();
    void resetBloom();
    void resetWater();
    void resetWaterSurface();
    void resetWaterScene();
};
