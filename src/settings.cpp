#include "settings.hpp"
#include "generated/presets.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

void Settings::resetColor() {
    const Settings d;
    exposure = d.exposure;
    contrast = d.contrast;
    saturation = d.saturation;
    temperature = d.temperature;
    tint = d.tint;
    gamma = d.gamma;
    shadows = d.shadows;
    highlights = d.highlights;
}
void Settings::resetBloom() {
    const Settings d;
    bloomStrength = d.bloomStrength;
    threshold = d.threshold;
    knee = d.knee;
    radius = d.radius;
    bloomScale = d.bloomScale;
}
void Settings::resetWater() {
    resetWaterSurface();
    resetWaterScene();
}
void Settings::resetWaterSurface() {
    const Settings d;
    sfxWake = d.sfxWake;
    waterWaves = d.waterWaves;
    waterLength = d.waterLength;
    waterSpeed = d.waterSpeed;
    waterRipples = d.waterRipples;
    waterRippleScale = d.waterRippleScale;
    waterOpacity = d.waterOpacity;
    waterSky = d.waterSky;
    waterGlints = d.waterGlints;
    waterGloss = d.waterGloss;
    waterTint = d.waterTint;
    waterRefraction = d.waterRefraction;
    waterTexture = d.waterTexture;
}
void Settings::resetWaterScene() {
    const Settings d;
    waterDepth = d.waterDepth;
    waterShore = d.waterShore;
    waterSSR = d.waterSSR;
    waterSSRQuality = d.waterSSRQuality;
    waterReflectionMethod = d.waterReflectionMethod;
    waterReflectionResolution = d.waterReflectionResolution;
    waterRaySteps = d.waterRaySteps;
    waterRefinementSteps = d.waterRefinementSteps;
    waterTraceDistance = d.waterTraceDistance;
    waterTraceBias = d.waterTraceBias;
    waterTraceThickness = d.waterTraceThickness;
    waterReflectionConfidence = d.waterReflectionConfidence;
    waterSoftFallback = d.waterSoftFallback;
    waterCaustics = d.waterCaustics;
    waterDispersion = d.waterDispersion;
    waterCausticScale = d.waterCausticScale;
}
void Settings::resetWaterReflections() {
    const Settings d;
    waterSSR=d.waterSSR; waterSSRQuality=d.waterSSRQuality; waterSky=d.waterSky;
    waterReflectionMethod=d.waterReflectionMethod; waterReflectionResolution=d.waterReflectionResolution;
    waterRaySteps=d.waterRaySteps; waterRefinementSteps=d.waterRefinementSteps;
    waterTraceDistance=d.waterTraceDistance; waterTraceBias=d.waterTraceBias;
    waterTraceThickness=d.waterTraceThickness; waterReflectionConfidence=d.waterReflectionConfidence;
    waterSoftFallback=d.waterSoftFallback;
}

void Settings::sanitize() {
    auto range = [](float &v, float lo, float hi, float fallback) {
        v = std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
    };
    range(bloomStrength, 0, 2, .22f);
    range(timeMinutes, 0, 1439, 720);
    timeMinutes = std::round(timeMinutes);
    range(weatherIntensity, 0, 100, 100);
    range(shopSignDistance,5,100,40);
    range(playerDrawDistance,0,1000,0);
    range(monsterDrawDistance,0,1000,0);
    range(petDrawDistance,0,1000,0);
    weatherType = std::clamp(weatherType, 0, 2);
    range(threshold, 0, 1, .72f);
    range(knee, .001f, .5f, .15f);
    range(radius, .5f, 4, 1.6f);
    bloomScale = bloomScale == 2 ? 2 : 4;
    panelPage = std::clamp(panelPage, 0, 3);
    language = static_cast<Language>(LanguageIndex(language));
    range(exposure, -2, 2, 0);
    range(contrast, .5f, 1.5f, 1);
    range(saturation, 0, 2, 1);
    range(temperature, -1, 1, 0);
    range(tint, -1, 1, 0);
    range(gamma, .5f, 2, 1);
    range(shadows, -.5f, .5f, 0);
    range(highlights, -.5f, .5f, 0);
    range(waterWaves, 0, .15f, .035f);
    range(waterLength, 1, 15, 5);
    range(waterSpeed, 0, 3, .7f);
    range(waterRipples, 0, 1, .35f);
    range(waterRippleScale, .25f, 6, 2);
    range(waterOpacity, .15f, 1, .55f);
    range(waterSky, 0, 1, .25f);
    range(waterGlints, 0, 2, .65f);
    range(waterGloss, 0, 1, .65f);
    range(waterTint, 0, 1, .25f);
    range(waterRefraction, 0, 1, .35f);
    range(waterTexture, 0, 1, .05f);
    range(waterDepth, 0, 2, 1);
    range(waterShore, .05f, 3, .6f);
    range(waterSSR, 0, 1, .65f);
    waterSSRQuality = waterSSRQuality == 1 ? 1 : 0;
    waterReflectionMethod = std::clamp(waterReflectionMethod, -1, 2);
    if (waterReflectionResolution != 50 && waterReflectionResolution != 75) waterReflectionResolution = 100;
    waterRaySteps = waterRaySteps ? std::clamp(waterRaySteps, 8, 64) : 0;
    waterRefinementSteps = std::clamp(waterRefinementSteps, 4, 16);
    range(waterTraceDistance, 16, 512, 512);
    range(waterTraceBias, .01f, .5f, .06f);
    range(waterTraceThickness, .5f, 3, 1);
    range(waterReflectionConfidence, 0, .75f, 0);
    range(waterCaustics, 0, 2, 0);
    range(waterDispersion, 0, 1, .15f);
    waterCausticScale = waterCausticScale == 2 ? 2 : 4;
    if (msaaSamples != -1 && msaaSamples != 2 && msaaSamples != 4 && msaaSamples != 8)
        msaaSamples = 0;
#define SCENE_BOOL(g, m, k, d)
#define SCENE_FLOAT(g, m, k, d, lo, hi) range(m, lo, hi, d);
#include "scene_settings.inc"
#undef SCENE_BOOL
#undef SCENE_FLOAT
    grassQuality=std::round(grassQuality);
}
void Settings::resetSceneSection(int group) {
    if(group==11)sfxAuraCustomColor=false;
#define SCENE_BOOL(g, m, k, d)
#define SCENE_FLOAT(g, m, k, d, lo, hi)                                                                      \
    if (group == g)                                                                                          \
        m = d;
#include "scene_settings.inc"
#undef SCENE_BOOL
#undef SCENE_FLOAT
}

bool Settings::load(const std::wstring &path) {
    FILE *file = _wfopen(path.c_str(), L"rb");
    if (!file)
        return false;
    Settings next;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char key[80], value[80];
        if (sscanf(line, " %79[^= \t] = %79s", key, value) != 2)
            continue;
        if (!strcmp(key, "language")) {
            next.language = Language::English;
            for (std::size_t i = 0; i < LanguageCount; ++i)
                if (!strcmp(value, LanguageCodes[i])) next.language = static_cast<Language>(i);
            continue;
        }
        char *end = nullptr;
        float number = strtof(value, &end);
        if (end == value || *end || !std::isfinite(number))
            continue;
#define READ_BOOL(k, m)                                                                                      \
    if (strcmp(key, k) == 0)                                                                                 \
        next.m = number != 0;
#define READ_FLOAT(k, m)                                                                                     \
    if (strcmp(key, k) == 0)                                                                                 \
        next.m = number;
        READ_BOOL("enabled", enabled)
        READ_BOOL("unlock_fps", unlockFps)
        READ_BOOL("background_activity", backgroundActivity)
        READ_BOOL("limit_shop_signs", limitShopSigns)
        READ_BOOL("hide_decorative_effects", hideDecorativeEffects)
        READ_BOOL("hide_skill_effects", hideSkillEffects)
        READ_BOOL("hide_pet_effects", hidePetEffects)
        READ_BOOL("render_players", renderPlayers)
        READ_BOOL("render_monsters", renderMonsters)
        READ_BOOL("render_pets", renderPets)
        READ_FLOAT("player_draw_distance", playerDrawDistance)
        READ_FLOAT("monster_draw_distance", monsterDrawDistance)
        READ_FLOAT("pet_draw_distance", petDrawDistance)
        READ_FLOAT("shop_sign_distance", shopSignDistance)
        READ_BOOL("custom_cursor", customCursor)
        READ_BOOL("interface_motion", interfaceMotion)
        READ_BOOL("precise_water_depth", preciseWaterDepth)
        READ_BOOL("time_override", timeOverride)
        READ_BOOL("freeze_time", freezeTime)
        READ_BOOL("weather_override", weatherOverride)
        READ_FLOAT("time_minutes", timeMinutes)
        READ_FLOAT("weather_intensity", weatherIntensity)
        if (!strcmp(key, "weather_type"))
            next.weatherType = int(std::clamp(number, 0.f, 2.f));
        if (!strcmp(key, "msaa_samples"))
            next.msaaSamples = (number == -1 || number == 2 || number == 4 || number == 8) ? int(number) : 0;
        READ_BOOL("bloom", bloom)
        READ_BOOL("color", color)
        READ_BOOL("open_on_start", openOnStart)
        READ_FLOAT("bloom_strength", bloomStrength) READ_FLOAT("threshold", threshold)
            READ_FLOAT("knee", knee) READ_FLOAT("radius", radius) if (strcmp(key, "bloom_scale") == 0)
                next.bloomScale = number == 2 ? 2 : 4;
        if (strcmp(key, "panel_page") == 0)
            next.panelPage = int(std::clamp(number, 0.f, 3.f));
        READ_FLOAT("exposure", exposure)
        READ_FLOAT("contrast", contrast)
        READ_FLOAT("saturation", saturation)
        READ_FLOAT("temperature", temperature)
        READ_FLOAT("tint", tint) READ_FLOAT("gamma", gamma) READ_FLOAT("shadows", shadows) READ_FLOAT(
            "highlights", highlights) READ_BOOL("water", water) READ_FLOAT("water_waves", waterWaves)
            READ_FLOAT("water_length", waterLength) READ_FLOAT("water_speed", waterSpeed)
                READ_FLOAT("water_ripples", waterRipples) READ_FLOAT("water_ripple_scale", waterRippleScale)
                    READ_FLOAT("water_opacity", waterOpacity) READ_FLOAT("water_sky", waterSky) READ_FLOAT(
                        "water_glints", waterGlints) READ_FLOAT("water_gloss", waterGloss)
                        READ_FLOAT("water_tint", waterTint) READ_FLOAT("water_refraction", waterRefraction)
                            READ_FLOAT("water_texture", waterTexture) READ_FLOAT("water_depth", waterDepth)
                                READ_FLOAT("water_shore", waterShore) READ_FLOAT("water_ssr", waterSSR)
                                    READ_FLOAT("water_caustics", waterCaustics) READ_FLOAT(
                                        "water_dispersion",
                                        waterDispersion) if (strcmp(key, "water_caustic_scale") == 0)
                                        next.waterCausticScale = number == 2 ? 2 : 4;
        if (strcmp(key, "water_ssr_quality") == 0)
            next.waterSSRQuality = number == 1 ? 1 : 0;
        if (!strcmp(key,"water_reflection_method")) next.waterReflectionMethod=int(std::clamp(number,-1.f,2.f));
        if (!strcmp(key,"water_reflection_resolution")) next.waterReflectionResolution=int(std::clamp(number,0.f,100.f));
        if (!strcmp(key,"water_ray_steps")) next.waterRaySteps=int(std::clamp(number,0.f,64.f));
        if (!strcmp(key,"water_refinement_steps")) next.waterRefinementSteps=int(std::clamp(number,4.f,16.f));
        READ_FLOAT("water_trace_distance", waterTraceDistance)
        READ_FLOAT("water_trace_bias", waterTraceBias)
        READ_FLOAT("water_trace_thickness", waterTraceThickness)
        READ_FLOAT("water_reflection_confidence", waterReflectionConfidence)
        READ_BOOL("water_soft_fallback", waterSoftFallback)
#undef READ_BOOL
#undef READ_FLOAT
#define SCENE_BOOL(g, m, k, d)                                                                               \
    if (!strcmp(key, k))                                                                                     \
        next.m = number != 0;
#define SCENE_FLOAT(g, m, k, d, lo, hi)                                                                      \
    if (!strcmp(key, k))                                                                                     \
        next.m = number;
#include "scene_settings.inc"
#undef SCENE_BOOL
#undef SCENE_FLOAT
    }
    bool ok = !ferror(file);
    fclose(file);
    if (ok) {
        next.sanitize();
        *this = next;
    }
    return ok;
}

bool Settings::save(const std::wstring &path) const {
    auto temp = path + L".tmp";
    FILE *file = _wfopen(temp.c_str(), L"wb");
    if (!file)
        return false;
    int written = fprintf(
        file,
        "; FENB / Shift + Enter opens the "
        "panel\n[General]\nenabled=%d\nopen_on_start=%d\npanel_page=%d\nlanguage=%s\nunlock_fps=%d\nbackground_activity=%d\nprecise_"
        "water_depth=%"
        "d\n"
        "\n[Bloom]\nbloom=%d\nbloom_strength=%.6f\nthreshold=%.6f\nknee=%.6f\nradius=%.6f\nbloom_scale=%d\n"
        "\n[Color]\ncolor=%d\nexposure=%.6f\ncontrast=%.6f\nsaturation=%.6f\ntemperature=%.6f\ntint=%."
        "6f\ngamma=%.6f\nshadows=%.6f\nhighlights=%.6f\n"
        "\n[WaterFX]\nwater=%d\nwater_waves=%.6f\nwater_length=%.6f\nwater_speed=%.6f\nwater_ripples=%."
        "6f\nwater_ripple_scale=%.6f\nwater_opacity=%.6f\nwater_sky=%.6f\nwater_glints=%.6f\nwater_gloss=%."
        "6f\nwater_tint=%.6f\nwater_refraction=%.6f\nwater_texture=%.6f\nwater_depth=%.6f\n"
        "water_shore=%.6f\nwater_ssr=%.6f\nwater_ssr_quality=%d\nwater_caustics=%.6f\nwater_dispersion=%."
        "6f\nwater_caustic_scale=%d\n",
        enabled, openOnStart, panelPage, LanguageCodes[LanguageIndex(language)], unlockFps, backgroundActivity,
        preciseWaterDepth, bloom, bloomStrength, threshold, knee, radius, bloomScale, color, exposure,
        contrast, saturation, temperature, tint, gamma, shadows, highlights, water, waterWaves, waterLength,
        waterSpeed, waterRipples, waterRippleScale, waterOpacity, waterSky, waterGlints, waterGloss,
        waterTint, waterRefraction, waterTexture, waterDepth, waterShore, waterSSR, waterSSRQuality,
        waterCaustics, waterDispersion, waterCausticScale);
    bool ok = written > 0 && !ferror(file);
    if (fprintf(file,"water_reflection_method=%d\nwater_reflection_resolution=%d\nwater_ray_steps=%d\n"
                     "water_refinement_steps=%d\nwater_trace_distance=%.6f\nwater_trace_bias=%.6f\n"
                     "water_trace_thickness=%.6f\nwater_reflection_confidence=%.6f\nwater_soft_fallback=%d\n",
                waterReflectionMethod,waterReflectionResolution,waterRaySteps,waterRefinementSteps,
                waterTraceDistance,waterTraceBias,waterTraceThickness,waterReflectionConfidence,waterSoftFallback)<0)
        ok=false;
    if(fprintf(file,"\n[Optimization]\nlimit_shop_signs=%d\nshop_sign_distance=%.6f\n",limitShopSigns,shopSignDistance)<0)ok=false;
    if(fprintf(file,"hide_decorative_effects=%d\nhide_skill_effects=%d\nhide_pet_effects=%d\n",hideDecorativeEffects,hideSkillEffects,hidePetEffects)<0)ok=false;
    if(fprintf(file,"render_players=%d\nrender_monsters=%d\nrender_pets=%d\n"
        "player_draw_distance=%.6f\nmonster_draw_distance=%.6f\npet_draw_distance=%.6f\n",
        renderPlayers,renderMonsters,renderPets,playerDrawDistance,monsterDrawDistance,petDrawDistance)<0)ok=false;
    if (fprintf(file, "\n[Interface]\ncustom_cursor=%d\ninterface_motion=%d\n",
                customCursor, interfaceMotion) < 0)
        ok = false;
    if (fprintf(file, "\n[Environment]\ntime_override=%d\ntime_minutes=%.0f\nfreeze_time=%d\n"
                      "weather_override=%d\nweather_type=%d\nweather_intensity=%.6f\n",
                timeOverride, timeMinutes, freezeTime, weatherOverride, weatherType, weatherIntensity) < 0)
        ok = false;
    if (fprintf(file, "\n[SceneFX]\nmsaa_samples=%d\n", msaaSamples) < 0)
        ok = false;
#define SCENE_BOOL(g, m, k, d)                                                                               \
    if (fprintf(file, k "=%d\n", int(m)) < 0)                                                                \
        ok = false;
#define SCENE_FLOAT(g, m, k, d, lo, hi)                                                                      \
    if (fprintf(file, k "=%.6f\n", m) < 0)                                                                   \
        ok = false;
#include "scene_settings.inc"
#undef SCENE_BOOL
#undef SCENE_FLOAT
    if (fclose(file) != 0)
        ok = false;
    if (!ok) {
        DeleteFileW(temp.c_str());
        return false;
    }
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp.c_str());
        return false;
    }
    return true;
}

void Settings::preserveControls(const Settings &source) {
#define KEEP(member) member = source.member;
    KEEP(enabled) KEEP(customCursor) KEEP(interfaceMotion) KEEP(trees)
    KEEP(limitShopSigns) KEEP(shopSignDistance)
    KEEP(hideDecorativeEffects) KEEP(hideSkillEffects) KEEP(hidePetEffects)
    KEEP(renderPlayers) KEEP(renderMonsters) KEEP(renderPets)
    KEEP(playerDrawDistance) KEEP(monsterDrawDistance) KEEP(petDrawDistance)
    KEEP(openOnStart) KEEP(unlockFps) KEEP(backgroundActivity) KEEP(preciseWaterDepth) KEEP(msaaSamples) KEEP(panelPage) KEEP(language)
        KEEP(timeOverride) KEEP(timeMinutes) KEEP(freezeTime) KEEP(weatherOverride) KEEP(weatherType)
            KEEP(weatherIntensity)
#undef KEEP
}
void Settings::preset(int index) {
    const Settings controls = *this;
    *this = Settings{};
    ApplyBuiltInVisuals(*this, index);
    preserveControls(controls);
}
int Settings::matchingPreset() const {
    for (int i = 0; i < BuiltInPresetCount; ++i) {
        Settings candidate = *this;
        candidate.preset(i);
        if (candidate == *this)
            return i;
    }
    return -1;
}
