#include "settings.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            printf("FAIL %d: %s\n", __LINE__, #x);                                                           \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
int main() {
    {
        Settings section;
        section.enabled = false;
        section.unlockFps = true;
        section.color = false;
        section.bloom = false;
        section.waterOpacity = .9f;
        section.waterRefraction = .9f;
        section.waterTexture = .9f;
        section.exposure = 1;
        section.bloomScale = 2;
        section.resetBloom();
        CHECK(section.bloomScale == 4 && section.exposure == 1 && section.waterOpacity == .9f &&
              !section.bloom);
        section.resetWater();
        CHECK(section.waterOpacity == Settings{}.waterOpacity && section.exposure == 1);
        CHECK(section.waterRefraction == Settings{}.waterRefraction &&
              section.waterTexture == Settings{}.waterTexture);
        section.resetColor();
        CHECK(section.exposure == 0 && !section.color && !section.enabled && section.unlockFps);
    }
    Settings s;
    {
        Settings particles;
        particles.sfx = false;
        particles.sfxIntensity = 8;
        particles.sfxWake = -1;
        particles.sfxGlow = std::numeric_limits<float>::quiet_NaN();
        particles.sanitize();
        CHECK(particles.sfxIntensity == 2 && particles.sfxWake == 0 &&
              particles.sfxGlow == Settings{}.sfxGlow);
        particles.language = Language::Russian;
        particles.resetSceneSection(11);
        CHECK(!particles.sfx && particles.sfxIntensity == Settings{}.sfxIntensity &&
              particles.sfxWake == 0 && particles.language == Language::Russian);
        particles.resetWaterSurface();
        CHECK(particles.sfxWake == Settings{}.sfxWake && !particles.sfx);
        particles.skyDetail=2;particles.skyStars=std::numeric_limits<float>::quiet_NaN();
        particles.sanitize();
        CHECK(particles.skyDetail==1 && particles.skyStars==Settings{}.skyStars);
        particles.resetSceneSection(2);
        CHECK(particles.skyDetail==Settings{}.skyDetail && particles.language==Language::Russian);
        CHECK(particles.save(L"build/sfx-settings.ini"));
        Settings restored;
        CHECK(restored.load(L"build/sfx-settings.ini") && restored == particles);
        DeleteFileW(L"build/sfx-settings.ini");
    }
    {
        Settings environment;
        CHECK(!environment.timeOverride && !environment.weatherOverride && environment.freezeTime);
        environment.timeOverride = true;
        environment.weatherOverride = true;
        environment.freezeTime = false;
        environment.timeMinutes = 1110;
        environment.weatherType = 2;
        environment.weatherIntensity = 37;
        environment.language = Language::Russian;
        CHECK(environment.save(L"build/environment-settings.ini"));
        Settings restored;
        CHECK(restored.load(L"build/environment-settings.ini") && restored == environment);
        restored.timeMinutes = 9999;
        restored.weatherType = -10;
        restored.weatherIntensity = std::numeric_limits<float>::quiet_NaN();
        restored.sanitize();
        CHECK(restored.timeMinutes == 1439 && restored.weatherType == 0 && restored.weatherIntensity == 100);
        CHECK(restored.language == Language::Russian);
        FILE *legacy = _wfopen(L"build/environment-legacy.ini", L"wb");
        CHECK(legacy);
        fputs("language=ru\n[Color]\nexposure=0.5\n", legacy);
        fclose(legacy);
        CHECK(restored.load(L"build/environment-legacy.ini"));
        CHECK(!restored.timeOverride && !restored.weatherOverride && restored.exposure == .5f &&
              restored.language == Language::Russian);
    }
    CHECK(s.language == Language::English);
    for (int i = 1; i < static_cast<int>(UiText::Count); ++i) {
        auto text = static_cast<UiText>(i);
        auto en = Translate(Language::English, text), ru = Translate(Language::Russian, text);
        CHECK(*en && *ru);
        CHECK(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, en, -1, nullptr, 0) > 0);
        CHECK(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ru, -1, nullptr, 0) > 0);
    }
    s.language = Language::Russian;
    CHECK(!s.unlockFps);
    s.unlockFps = true;
    s.bloomScale = 3;
    s.exposure = std::numeric_limits<float>::infinity();
    s.gamma = 0;
    s.threshold = 9;
    s.sanitize();
    CHECK(s.unlockFps);
    CHECK(s.bloomScale == 4 && s.exposure == 0 && s.gamma == .5f && s.threshold == 1);
    auto path = std::wstring(L"build\\settings-roundtrip.ini");
    s.preset(2);
    CHECK(s.language == Language::Russian);
    s.enabled = false;
    s.shadows = -.25f;
    CHECK(s.unlockFps);
    s.water = false;
    s.waterWaves = .075f;
    s.waterGlints = 1.4f;
    s.waterOpacity = .3f;
    s.waterRefraction = .72f;
    s.waterTexture = .18f;
    s.waterDepth = .8f;
    s.waterShore = 1.1f;
    s.waterSSR = .9f;
    s.waterSSRQuality = 1;
    s.preciseWaterDepth = false;
    s.msaaSamples = 4;
    s.waterDepth = .8f;
    s.waterShore = 1.1f;
    s.waterSSR = .9f;
    s.waterSSRQuality = 1;
    s.waterCaustics = .65f;
    s.waterDispersion = .3f;
    s.waterCausticScale = 2;
    s.ssao = true;
    s.sunRays = true;
    s.sky = true;
    s.fog = true;
    s.dof = true;
    s.adaptation = true;
    s.localLighting = true;
    s.aoStrength = .8f;
    s.fogDensity = .008f;
    s.focusDistance = 48;
    s.maxEV = .7f;
    s.fxaa = true;
    s.smaa = true;
    s.fxaaStrength = .8f;
    s.nightFoliage = .55f;
    CHECK(s.save(path));
    Settings loaded;
    CHECK(loaded.load(path));
    CHECK(loaded.language == Language::Russian);
    CHECK(loaded.unlockFps);
    CHECK(!loaded.preciseWaterDepth);
    CHECK(loaded.msaaSamples == 4 && loaded.fxaa && loaded.smaa && loaded.fxaaStrength == .8f &&
          loaded.nightFoliage == .55f);
    CHECK(loaded.ssao && loaded.sunRays && loaded.sky && loaded.fog && loaded.dof && loaded.adaptation &&
          loaded.localLighting);
    CHECK(loaded.aoStrength == .8f && loaded.fogDensity == .008f && loaded.focusDistance == 48 &&
          loaded.maxEV == .7f);
    loaded.resetSceneSection(0);
    CHECK(loaded.ssao && loaded.aoStrength == Settings{}.aoStrength && loaded.fog);
    CHECK(loaded.waterCaustics == .65f && loaded.waterDispersion == .3f && loaded.waterCausticScale == 2);
    CHECK(!loaded.enabled && loaded.temperature == s.temperature && loaded.shadows == s.shadows);
    CHECK(!loaded.water && loaded.waterWaves == s.waterWaves && loaded.waterGlints == s.waterGlints &&
          loaded.waterOpacity == s.waterOpacity);
    CHECK(loaded.waterRefraction == s.waterRefraction && loaded.waterTexture == s.waterTexture);
    CHECK(loaded.waterDepth == s.waterDepth && loaded.waterShore == s.waterShore &&
          loaded.waterSSR == s.waterSSR && loaded.waterSSRQuality == 1);
    loaded.resetWaterSurface();
    CHECK(loaded.waterSSR == .9f && loaded.waterDepth == .8f);
    loaded.resetWaterScene();
    CHECK(loaded.waterSSR == Settings{}.waterSSR && loaded.waterDepth == Settings{}.waterDepth);
    CHECK(loaded.waterCaustics == 0 && loaded.waterDispersion == Settings{}.waterDispersion &&
          loaded.waterCausticScale == 4);
    FILE *file = _wfopen(path.c_str(), L"wb");
    CHECK(file);
    fputs("unlock_fps=1\nexposure=nan\ngamma=0\nbloom_scale=1e35\nsaturation=999\nunknown=3\nwater_length="
          "0\nwater_waves=99\nwater_opacity=nan\n",
          file);
    fclose(file);
    CHECK(loaded.load(path));
    CHECK(loaded.language == Language::English); // Existing configs without a language use English.
    CHECK(loaded.unlockFps);                     // Explicit opt-in uses the simulation-aware unlock.
    CHECK(loaded.waterRefraction == Settings{}.waterRefraction &&
          loaded.waterTexture == Settings{}.waterTexture);
    CHECK(loaded.exposure == 0 && loaded.gamma == .5f && loaded.bloomScale == 4 && loaded.saturation == 2);
    CHECK(loaded.waterLength == 1 && loaded.waterWaves == .15f && loaded.waterOpacity == .55f);
    CHECK(!loaded.save(L"build\\missing-directory\\config.ini"));
    Settings before = loaded;
    CHECK(!loaded.load(L"build\\missing-file.ini"));
    CHECK(loaded.saturation == before.saturation);
    s.unlockFps = true;
    CHECK(s.save(path));
    CHECK(loaded.load(path) && loaded.unlockFps);
    file = _wfopen(path.c_str(), L"rb");
    CHECK(file);
    bool safeUnlock = false;
    char line[256];
    while (fgets(line, sizeof(line), file))
        if (!strcmp(line, "unlock_fps=1\n"))
            safeUnlock = true;
    fclose(file);
    CHECK(safeUnlock);
    s.unlockFps = true;
    s.preset(1);
    CHECK(s.unlockFps);
    CHECK(!s.preciseWaterDepth);
    CHECK(s.msaaSamples == 4);
    CHECK(s.language == Language::Russian);
    file = _wfopen(path.c_str(), L"wb");
    CHECK(file);
    fputs("[General]\nlanguage=unsupported\nexposure=0.5\n", file);
    fclose(file);
    CHECK(loaded.load(path));
    CHECK(loaded.language == Language::English && loaded.exposure == .5f);
    loaded.language = static_cast<Language>(42);
    loaded.sanitize();
    CHECK(loaded.language == Language::English);
    CHECK(loaded.save(path));
    s.language = Language::Russian;
    CHECK(s.load(path) && s.language == Language::English);
    Settings shipped;
    CHECK(shipped.load(L"config/FENB.ini"));
    Settings builtIn;
    builtIn.preset(0);
    shipped.preserveControls(builtIn);
    CHECK(shipped == builtIn && builtIn.matchingPreset() == 0);
    CHECK(!strcmp(Translate(Language::English, BuiltInProfileNames[0]), "Default"));
    CHECK(!strcmp(Translate(Language::Russian, BuiltInProfileNames[0]), "Основной"));
    for (int index = 0; index < BuiltInPresetCount; ++index) {
        Settings controls;
        controls.enabled = false;
        controls.customCursor = false;
        controls.interfaceMotion = false;
        controls.unlockFps = true;
        controls.msaaSamples = 8;
        controls.language = Language::Russian;
        controls.preciseWaterDepth = false;
        controls.timeOverride = controls.weatherOverride = true;
        controls.timeMinutes = 1193;
        controls.freezeTime = false;
        controls.weatherType = 2;
        controls.weatherIntensity = 38;
        auto profile = controls;
        profile.preset(index);
        CHECK(profile.matchingPreset() == index);
        auto sanitized = profile;
        sanitized.sanitize();
        CHECK(sanitized == profile);
        auto retained = Settings{};
        retained.preserveControls(profile);
        CHECK(retained == controls);
        CHECK(profile.save(path));
        auto roundTrip = profile;
        CHECK(roundTrip.load(path));
        CHECK(roundTrip == profile);
        for (int other = 0; other < index; ++other) {
            auto different = controls;
            different.preset(other);
            CHECK(different != profile);
        }
        profile.exposure += .01f;
        CHECK(profile.matchingPreset() == -1);
    }
    DeleteFileW(path.c_str());
    puts("PASS: config round-trip, sanitization, corrupt values, failed IO.");
}
