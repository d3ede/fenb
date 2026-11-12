#include "settings.hpp"
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
}

void Settings::sanitize() {
    auto range = [](float &v, float lo, float hi, float fallback) {
        v = std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
    };
    range(bloomStrength, 0, 2, .22f);
    range(threshold, 0, 1, .72f);
    range(knee, .001f, .5f, .15f);
    range(radius, .5f, 4, 1.6f);
    bloomScale = bloomScale == 2 ? 2 : 4;
    panelPage = std::clamp(panelPage, 0, 3);
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
        READ_BOOL("bloom", bloom)
        READ_BOOL("color", color) READ_BOOL("open_on_start", openOnStart)
            READ_FLOAT("bloom_strength", bloomStrength) READ_FLOAT("threshold", threshold)
                READ_FLOAT("knee", knee) READ_FLOAT("radius", radius) if (strcmp(key, "bloom_scale") == 0)
                    next.bloomScale = number == 2 ? 2 : 4;
        if (strcmp(key, "panel_page") == 0)
            next.panelPage = int(std::clamp(number, 0.f, 3.f));
        READ_FLOAT("exposure", exposure)
        READ_FLOAT("contrast", contrast)
        READ_FLOAT("saturation", saturation) READ_FLOAT("temperature", temperature) READ_FLOAT("tint", tint)
            READ_FLOAT("gamma", gamma) READ_FLOAT("shadows", shadows) READ_FLOAT("highlights", highlights)
                READ_BOOL("water", water) READ_FLOAT("water_waves", waterWaves)
                    READ_FLOAT("water_length", waterLength) READ_FLOAT("water_speed", waterSpeed)
                        READ_FLOAT("water_ripples", waterRipples)
                            READ_FLOAT("water_ripple_scale", waterRippleScale)
                                READ_FLOAT("water_opacity", waterOpacity) READ_FLOAT("water_sky", waterSky)
                                    READ_FLOAT("water_glints", waterGlints)
                                        READ_FLOAT("water_gloss", waterGloss)
                                            READ_FLOAT("water_tint", waterTint)
        READ_FLOAT("water_refraction", waterRefraction)
        READ_FLOAT("water_texture", waterTexture)
        READ_FLOAT("water_depth", waterDepth)
        READ_FLOAT("water_shore", waterShore)
        READ_FLOAT("water_ssr", waterSSR)
        if (strcmp(key, "water_ssr_quality") == 0) next.waterSSRQuality = number == 1 ? 1 : 0;
#undef READ_BOOL
#undef READ_FLOAT
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
        "panel\n[General]\nenabled=%d\nopen_on_start=%d\npanel_page=%d\nunlock_fps=%d\n"
        "\n[Bloom]\nbloom=%d\nbloom_strength=%.6f\nthreshold=%.6f\nknee=%.6f\nradius=%.6f\nbloom_scale=%d\n"
        "\n[Color]\ncolor=%d\nexposure=%.6f\ncontrast=%.6f\nsaturation=%.6f\ntemperature=%.6f\ntint=%."
        "6f\ngamma=%.6f\nshadows=%.6f\nhighlights=%.6f\n"
        "\n[WaterFX]\nwater=%d\nwater_waves=%.6f\nwater_length=%.6f\nwater_speed=%.6f\nwater_ripples=%."
        "6f\nwater_ripple_scale=%.6f\nwater_opacity=%.6f\nwater_sky=%.6f\nwater_glints=%.6f\nwater_gloss=%."
        "6f\nwater_tint=%.6f\nwater_refraction=%.6f\nwater_texture=%.6f\nwater_depth=%.6f\n"
        "water_shore=%.6f\nwater_ssr=%.6f\nwater_ssr_quality=%d\n",
        enabled, openOnStart, panelPage, unlockFps, bloom, bloomStrength, threshold, knee, radius, bloomScale,
        color, exposure, contrast, saturation, temperature, tint, gamma, shadows, highlights, water,
        waterWaves, waterLength, waterSpeed, waterRipples, waterRippleScale, waterOpacity, waterSky,
        waterGlints, waterGloss, waterTint, waterRefraction, waterTexture, waterDepth, waterShore, waterSSR, waterSSRQuality);
    bool ok = written > 0 && !ferror(file);
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

void Settings::preset(int index) {
    bool open = openOnStart;
    bool unlock = unlockFps;
    int page = panelPage;
    *this = Settings{};
    openOnStart = open;
    unlockFps = unlock;
    panelPage = page;
    if (index == 1) {
        bloomStrength = .32f;
        temperature = .12f;
        saturation = .94f;
        contrast = 1.04f;
    }
    if (index == 2) {
        bloomStrength = .12f;
        temperature = -.10f;
        saturation = .88f;
        contrast = 1.08f;
    }
    if (index == 3) {
        bloomStrength = .38f;
        threshold = .64f;
        radius = 2.4f;
        saturation = 1.10f;
        shadows = .05f;
    }
}
