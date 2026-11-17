#pragma once
#include "common.hpp"
#include "localization.hpp"
inline constexpr UiText BuiltInProfileNames[] = {UiText::DefaultProfile, UiText::HighEndProfile,
                                                 UiText::WarmTonesProfile, UiText::CoolTonesProfile,
                                                 UiText::SoftColorsProfile, UiText::LowEndProfile,
                                                 UiText::PotatoProfile, UiText::BlueWaterProfile,
                                                 UiText::CyanWaterProfile};
inline constexpr int BuiltInPresetCount = sizeof(BuiltInProfileNames) / sizeof(BuiltInProfileNames[0]);
// Display order is independent of persistent preset identities.
inline constexpr int BuiltInProfileOrder[] = {0, 1, 5, 6, 7, 8, 2, 3, 4};

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
    int waterReflectionMethod = -1, waterReflectionResolution = 100, waterRaySteps = 0;
    int waterRefinementSteps = 12;
    float waterTraceDistance = 512.f, waterTraceBias = .06f, waterTraceThickness = 1.f;
    float waterReflectionConfidence = 0.f;
    bool waterSoftFallback = true;
    float waterCaustics = 0.f, waterDispersion = .15f;
    int waterCausticScale = 4;
#define SCENE_BOOL(g, m, k, d) bool m = d;
#define SCENE_FLOAT(g, m, k, d, lo, hi) float m = d;
#include "scene_settings.inc"
#undef SCENE_BOOL
#undef SCENE_FLOAT
    bool postEnabled() const {
        return bloom || color || ssao || sunRays || fog || dof || adaptation || localLighting || fxaa ||
               smaa || lightingBalance || surfaceReflections || (sharpening && sharpeningStrength > 0);
    }
    void resetSceneSection(int group);
    bool openOnStart = false;
    bool preciseWaterDepth = true;
    int msaaSamples = 0;    // 0 retains the native choice, -1 disables, otherwise 2/4/8.
    bool unlockFps = false; // Opt-in continuous native updates with a guarded legacy fallback.
    bool backgroundActivity = false; // Independent opt-in to keep the world running while inactive.
    bool limitShopSigns = false;
    bool hideDecorativeEffects = false, hideSkillEffects = false, hidePetEffects = false;
    bool renderPlayers = true, renderMonsters = true, renderPets = true;
    float playerDrawDistance = 0, monsterDrawDistance = 0, petDrawDistance = 0;
    float shopSignDistance = 40;
    bool timeOverride = false, freezeTime = true, weatherOverride = false;
    float timeMinutes = 720, weatherIntensity = 100;
    int weatherType = 0; // 0 clear, 1 rain, 2 snow; independent of server weather.
    int panelPage = 0;
    Language language = Language::English;
    bool customCursor = true, interfaceMotion = true;
    bool operator==(const Settings &) const = default;
    void sanitize();
    bool load(const std::wstring &path);
    bool save(const std::wstring &path) const;
    void preset(int index);
    int matchingPreset() const;
    void preserveControls(const Settings &);
    void resetColor();
    void resetBloom();
    void resetWater();
    void resetWaterSurface();
    void resetWaterScene();
    void resetWaterReflections();
};
