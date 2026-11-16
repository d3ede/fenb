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
        Settings opt;
        CHECK(!opt.limitShopSigns&&opt.shopSignDistance==40);
        opt.limitShopSigns=true;opt.shopSignDistance=25;opt.language=Language::Portuguese;
        for(int i=0;i<BuiltInPresetCount;++i){opt.preset(i);CHECK(opt.limitShopSigns&&opt.shopSignDistance==25&&opt.language==Language::Portuguese);}
        CHECK(opt.save(L"build/optimization-settings.ini"));
        Settings restored;CHECK(restored.load(L"build/optimization-settings.ini"));
        CHECK(restored==opt);
        opt.shopSignDistance=std::numeric_limits<float>::quiet_NaN();opt.sanitize();CHECK(opt.shopSignDistance==40);
        opt.shopSignDistance=200;opt.sanitize();CHECK(opt.shopSignDistance==100);
        opt.shopSignDistance=-5;opt.sanitize();CHECK(opt.shopSignDistance==5);
        DeleteFileW(L"build/optimization-settings.ini");
    }
    {
        Settings sharp;
        CHECK(!sharp.sharpening && sharp.sharpeningStrength == .45f);
        sharp.bloom = sharp.color = false;
        CHECK(!sharp.postEnabled());
        sharp.sharpening = true;
        CHECK(sharp.postEnabled());
        sharp.sharpeningStrength = 0;
        CHECK(!sharp.postEnabled());
        sharp.sharpeningStrength = 2;
        sharp.sanitize();
        CHECK(sharp.sharpeningStrength == 1);
        sharp.sharpeningStrength = -1;
        sharp.sanitize();
        CHECK(sharp.sharpeningStrength == 0);
        sharp.sharpeningStrength = std::numeric_limits<float>::quiet_NaN();
        sharp.sanitize();
        CHECK(sharp.sharpeningStrength == .45f);
        sharp.sharpeningStrength = .8f;
        sharp.language = Language::Russian;
        CHECK(sharp.save(L"build/sharpening-settings.ini"));
        Settings restored;
        CHECK(restored.load(L"build/sharpening-settings.ini") && restored == sharp);
        restored.resetSceneSection(14);
        CHECK(restored.sharpening && restored.sharpeningStrength == .45f &&
              restored.language == Language::Russian && !restored.color);
        FILE *legacy = _wfopen(L"build/sharpening-settings.ini", L"wb");
        CHECK(legacy);
        fputs("[Color]\nexposure=0.5\n", legacy);
        fclose(legacy);
        CHECK(restored.load(L"build/sharpening-settings.ini"));
        CHECK(!restored.sharpening && restored.sharpeningStrength == .45f && restored.exposure == .5f);
        DeleteFileW(L"build/sharpening-settings.ini");
    }
    {
        Settings particles;
        particles.sfx = false;
        particles.sfxIntensity = 8;
        particles.sfxAura = 4;
        particles.sfxWake = -1;
        particles.sfxGlow = std::numeric_limits<float>::quiet_NaN();
        particles.sanitize();
        CHECK(particles.sfxIntensity == 2 && particles.sfxAura == 2 && particles.sfxWake == 0 &&
              particles.sfxGlow == Settings{}.sfxGlow);
        particles.language = Language::Russian;
        particles.resetSceneSection(11);
        CHECK(!particles.sfx && particles.sfxIntensity == Settings{}.sfxIntensity &&
              particles.sfxAura == Settings{}.sfxAura &&
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
        auto en = Translate(Language::English, text);
        for (std::size_t language = 0; language < LanguageCount; ++language) {
            auto translated = Translate(static_cast<Language>(language), text);
#ifndef FENB_DIAGNOSTICS
            if(text==UiText::CaptureFrames || text==UiText::CapturePending){
                CHECK(!*translated);continue;
            }
#endif
            CHECK(*translated);
            CHECK(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, translated, -1, nullptr, 0) > 0);
            // Localized printf substitutions must retain their types and ordering.
            const char *original = en, *localized = translated;
            while ((original = strchr(original, '%'))) {
                localized = strchr(localized, '%');
                CHECK(localized && original[1] == localized[1]);
                ++original; ++localized;
            }
            CHECK(!strchr(localized, '%'));
        }
    }
    s.language = Language::Russian;
    CHECK(!s.unlockFps);
    CHECK(!s.backgroundActivity);
    s.unlockFps = true;
    s.backgroundActivity = true;
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
    s.waterReflectionMethod=2;s.waterReflectionResolution=75;s.waterRaySteps=56;
    s.waterRefinementSteps=10;s.waterTraceDistance=320;s.waterTraceBias=.08f;
    s.waterTraceThickness=1.2f;s.waterReflectionConfidence=.15f;s.waterSoftFallback=false;
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
    s.surfaceReflections = true;
    s.surfaceStrength = .7f;
    s.surfaceRoughness = .8f;
    s.surfaceDistance = 65;
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
    CHECK(loaded.backgroundActivity);
    CHECK(!loaded.preciseWaterDepth);
    CHECK(loaded.msaaSamples == 4 && loaded.fxaa && loaded.smaa && loaded.fxaaStrength == .8f &&
          loaded.nightFoliage == .55f);
    CHECK(loaded.ssao && loaded.sunRays && loaded.sky && loaded.fog && loaded.dof && loaded.adaptation &&
          loaded.localLighting);
    CHECK(loaded.surfaceReflections && loaded.surfaceStrength==.7f && loaded.surfaceRoughness==.8f &&
          loaded.surfaceDistance==65);
    loaded.surfaceStrength=2; loaded.surfaceRoughness=-1; loaded.surfaceDistance=1000;
    loaded.sanitize();
    CHECK(loaded.surfaceStrength==1 && loaded.surfaceRoughness==0 && loaded.surfaceDistance==100);
    loaded.resetSceneSection(13);
    CHECK(loaded.surfaceReflections && loaded.surfaceStrength==Settings{}.surfaceStrength &&
          loaded.surfaceRoughness==Settings{}.surfaceRoughness && loaded.surfaceDistance==Settings{}.surfaceDistance);
    CHECK(loaded.aoStrength == .8f && loaded.fogDensity == .008f && loaded.focusDistance == 48 &&
          loaded.maxEV == .7f);
    loaded.resetSceneSection(0);
    CHECK(loaded.ssao && loaded.aoStrength == Settings{}.aoStrength && loaded.fog);
    CHECK(loaded.waterCaustics == .65f && loaded.waterDispersion == .3f && loaded.waterCausticScale == 2);
    CHECK(!loaded.enabled && loaded.temperature == s.temperature && loaded.shadows == s.shadows);
    CHECK(!loaded.water && loaded.waterWaves == s.waterWaves && loaded.waterGlints == s.waterGlints &&
          loaded.waterOpacity == s.waterOpacity);
    CHECK(loaded.waterRefraction == s.waterRefraction && loaded.waterTexture == s.waterTexture);
    CHECK(loaded.waterReflectionMethod==2 && loaded.waterReflectionResolution==75 && loaded.waterRaySteps==56 &&
          loaded.waterRefinementSteps==10 && loaded.waterTraceDistance==320 && loaded.waterTraceBias==.08f &&
          loaded.waterTraceThickness==1.2f && loaded.waterReflectionConfidence==.15f && !loaded.waterSoftFallback);
    loaded.resetWaterReflections();
    CHECK(loaded.waterReflectionMethod==Settings{}.waterReflectionMethod && loaded.waterRaySteps==0 &&
          loaded.waterReflectionResolution==100 && loaded.waterSoftFallback && loaded.waterDepth==.8f);
    loaded.waterSSR=s.waterSSR;loaded.waterSSRQuality=s.waterSSRQuality;
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
    CHECK(loaded.waterReflectionMethod==-1 && loaded.waterRaySteps==0 && loaded.waterReflectionResolution==100);
    loaded.waterReflectionMethod=99;loaded.waterReflectionResolution=60;loaded.waterRaySteps=999;
    loaded.waterRefinementSteps=-10;loaded.waterTraceDistance=-1;loaded.waterTraceBias=5;
    loaded.waterTraceThickness=0;loaded.waterReflectionConfidence=1;
    loaded.sanitize();
    CHECK(loaded.waterReflectionMethod==2 && loaded.waterReflectionResolution==100 && loaded.waterRaySteps==64 &&
          loaded.waterRefinementSteps==4 && loaded.waterTraceDistance==16 && loaded.waterTraceBias==.5f &&
          loaded.waterTraceThickness==.5f && loaded.waterReflectionConfidence==.75f);
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
    for (std::size_t i = 0; i < LanguageCount; ++i) {
        Settings localized;
        localized.language = static_cast<Language>(i);
        localized.sanitize();
        CHECK(localized.language == static_cast<Language>(i));
        CHECK(localized.save(path));
        CHECK(loaded.load(path) && loaded.language == localized.language);
        for (int preset = 0; preset < BuiltInPresetCount; ++preset) {
            loaded.preset(preset);
            CHECK(loaded.language == localized.language);
        }
        loaded.resetSceneSection(0); loaded.resetWater();
        CHECK(loaded.language == localized.language);
    }
    CHECK(!strcmp(Translate(static_cast<Language>(-1), UiText::Profile), "Profile"));
    Settings shipped;
    CHECK(shipped.load(L"config/FENB.ini"));
    Settings builtIn;
    builtIn.preset(0);
    shipped.preserveControls(builtIn);
    CHECK(shipped == builtIn && builtIn.matchingPreset() == 0);
    CHECK(!strcmp(Translate(Language::English, BuiltInProfileNames[0]), "Default"));
    CHECK(!strcmp(Translate(Language::Russian, BuiltInProfileNames[0]), "Основной"));
    CHECK(BuiltInProfileOrder[0]==0 && BuiltInProfileNames[BuiltInProfileOrder[1]]==UiText::HighEndProfile &&
          BuiltInProfileNames[BuiltInProfileOrder[2]]==UiText::LowEndProfile &&
          BuiltInProfileNames[BuiltInProfileOrder[3]]==UiText::PotatoProfile &&
          BuiltInProfileNames[BuiltInProfileOrder[4]]==UiText::BlueWaterProfile &&
          BuiltInProfileNames[BuiltInProfileOrder[5]]==UiText::CyanWaterProfile);
    unsigned ordered=0;for(int index:BuiltInProfileOrder)ordered|=1u<<index;
    CHECK(ordered==(1u<<BuiltInPresetCount)-1);
    Settings blueWater;blueWater.preset(7);
    CHECK(blueWater.water && blueWater.waterWaves==.08f && blueWater.waterOpacity==1.f &&
          blueWater.waterRefraction==1.f && blueWater.waterSSRQuality==1 && blueWater.waterReflectionMethod==2);
    auto waterOnly=blueWater;
    waterOnly.resetWater();auto defaultWater=builtIn;defaultWater.resetWater();
    CHECK(waterOnly==defaultWater);
    Settings cyanWater;cyanWater.preset(8);cyanWater.resetWater();
    CHECK(cyanWater==defaultWater);
    Settings potato;potato.preset(6);
    CHECK(potato.adaptation==builtIn.adaptation && potato.lightingBalance==builtIn.lightingBalance &&
          potato.adaptationKey==builtIn.adaptationKey && potato.lightingBalanceStrength==builtIn.lightingBalanceStrength);
    CHECK(potato.exposure==builtIn.exposure && potato.gamma==builtIn.gamma && potato.contrast==builtIn.contrast &&
          potato.saturation==builtIn.saturation && potato.temperature==builtIn.temperature && potato.tint==builtIn.tint &&
          potato.shadows==builtIn.shadows && potato.highlights==builtIn.highlights && potato.bloomStrength==builtIn.bloomStrength);
    Settings highEnd;highEnd.preset(1);
    CHECK(highEnd.ssao && highEnd.sunRays && highEnd.sky && highEnd.fog && highEnd.dof &&
          highEnd.adaptation && highEnd.localLighting && highEnd.lightingBalance && highEnd.smaa);
    CHECK(highEnd.sceneQuality && highEnd.sfx && highEnd.surfaceReflections &&
          highEnd.water && highEnd.waterReflectionMethod==2 && highEnd.waterRaySteps==48 &&
          highEnd.waterReflectionResolution==100 && highEnd.waterRefinementSteps==12 && highEnd.waterCaustics>0);
    CHECK(highEnd.exposure==builtIn.exposure && highEnd.temperature==builtIn.temperature &&
          highEnd.waterWaves==builtIn.waterWaves && highEnd.waterTint==builtIn.waterTint);
    for (int index : {2,3,4}) {
        Settings tone;tone.preset(index);
        CHECK(tone.exposure>=0 && tone.exposure<=.1f && tone.saturation>=.94f && tone.saturation<=1.04f &&
              tone.contrast>=.97f && tone.contrast<=1.05f && std::abs(tone.temperature)<=.25f &&
              tone.highlights<=0 && tone.shadows>=0);
        tone.resetColor();auto defaultWithoutColor=builtIn;defaultWithoutColor.resetColor();
        CHECK(tone==defaultWithoutColor); // Color looks retain Default water, effects and quality.
    }
    for (int index = 0; index < BuiltInPresetCount; ++index) {
        Settings controls;
        controls.enabled = false;
        controls.customCursor = false;
        controls.interfaceMotion = false;
        controls.unlockFps = true;
        controls.backgroundActivity = true;
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
