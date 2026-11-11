#include "settings.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

void Settings::sanitize() {
    auto range = [](float& v, float lo, float hi, float fallback) {
        v = std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
    };
    range(bloomStrength, 0, 2, .22f); range(threshold, 0, 1, .72f);
    range(knee, .001f, .5f, .15f); range(radius, .5f, 4, 1.6f);
    bloomScale = bloomScale == 2 ? 2 : 4;
    panelPage = std::clamp(panelPage,0,3);
    range(exposure, -2, 2, 0); range(contrast, .5f, 1.5f, 1);
    range(saturation, 0, 2, 1); range(temperature, -1, 1, 0);
    range(tint, -1, 1, 0); range(gamma, .5f, 2, 1);
    range(shadows, -.5f, .5f, 0); range(highlights, -.5f, .5f, 0);
}

bool Settings::load(const std::wstring& path) {
    FILE* file = _wfopen(path.c_str(), L"rb");
    if (!file) return false;
    Settings next;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char key[80], value[80];
        if (sscanf(line, " %79[^= \t] = %79s", key, value) != 2) continue;
        char* end = nullptr;
        float number = strtof(value, &end);
        if (end == value || *end || !std::isfinite(number)) continue;
#define READ_BOOL(k, m) if (strcmp(key, k) == 0) next.m = number != 0;
#define READ_FLOAT(k, m) if (strcmp(key, k) == 0) next.m = number;
        READ_BOOL("enabled", enabled) READ_BOOL("bloom", bloom) READ_BOOL("color", color)
        READ_BOOL("open_on_start", openOnStart)
        READ_FLOAT("bloom_strength", bloomStrength) READ_FLOAT("threshold", threshold)
        READ_FLOAT("knee", knee) READ_FLOAT("radius", radius)
        if (strcmp(key, "bloom_scale") == 0) next.bloomScale = number == 2 ? 2 : 4;
        if (strcmp(key, "panel_page") == 0) next.panelPage = int(std::clamp(number,0.f,3.f));
        READ_FLOAT("exposure", exposure) READ_FLOAT("contrast", contrast)
        READ_FLOAT("saturation", saturation) READ_FLOAT("temperature", temperature)
        READ_FLOAT("tint", tint) READ_FLOAT("gamma", gamma)
        READ_FLOAT("shadows", shadows) READ_FLOAT("highlights", highlights)
#undef READ_BOOL
#undef READ_FLOAT
    }
    bool ok = !ferror(file); fclose(file);
    if (ok) { next.sanitize(); *this = next; }
    return ok;
}

bool Settings::save(const std::wstring& path) const {
    auto temp = path + L".tmp";
    FILE* file = _wfopen(temp.c_str(), L"wb");
    if (!file) return false;
    int written = fprintf(file,
        "; FENB / Shift + Enter opens the panel\n[General]\nenabled=%d\nopen_on_start=%d\npanel_page=%d\n"
        "\n[Bloom]\nbloom=%d\nbloom_strength=%.6f\nthreshold=%.6f\nknee=%.6f\nradius=%.6f\nbloom_scale=%d\n"
        "\n[Color]\ncolor=%d\nexposure=%.6f\ncontrast=%.6f\nsaturation=%.6f\ntemperature=%.6f\ntint=%.6f\ngamma=%.6f\nshadows=%.6f\nhighlights=%.6f\n",
        enabled, openOnStart, panelPage, bloom, bloomStrength, threshold, knee, radius, bloomScale,
        color, exposure, contrast, saturation, temperature, tint, gamma, shadows, highlights);
    bool ok = written > 0 && !ferror(file);
    if (fclose(file) != 0) ok = false;
    if (!ok) { DeleteFileW(temp.c_str()); return false; }
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp.c_str()); return false;
    }
    return true;
}

void Settings::preset(int index) {
    bool open = openOnStart; int page = panelPage;
    *this = Settings{}; openOnStart = open; panelPage = page;
    if (index == 1) { bloomStrength = .32f; temperature = .12f; saturation = .94f; contrast = 1.04f; }
    if (index == 2) { bloomStrength = .12f; temperature = -.10f; saturation = .88f; contrast = 1.08f; }
    if (index == 3) { bloomStrength = .38f; threshold = .64f; radius = 2.4f; saturation = 1.10f; shadows = .05f; }
}

