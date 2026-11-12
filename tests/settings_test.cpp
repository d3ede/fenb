#include "settings.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main() {
    {
        Settings section;section.enabled=false;section.unlockFps=true;section.color=false;section.bloom=false;
        section.waterOpacity=.9f;section.waterRefraction=.9f;section.waterTexture=.9f;section.exposure=1;section.bloomScale=2;
        section.resetBloom();CHECK(section.bloomScale==4&&section.exposure==1&&section.waterOpacity==.9f&&!section.bloom);
        section.resetWater();CHECK(section.waterOpacity==Settings{}.waterOpacity&&section.exposure==1);
        CHECK(section.waterRefraction==Settings{}.waterRefraction&&section.waterTexture==Settings{}.waterTexture);
        section.resetColor();CHECK(section.exposure==0&&!section.color&&!section.enabled&&section.unlockFps);
    }
    Settings s;
    CHECK(!s.unlockFps);
    s.unlockFps=true;
    s.bloomScale=3; s.exposure=std::numeric_limits<float>::infinity(); s.gamma=0; s.threshold=9;
    s.sanitize();
    CHECK(s.unlockFps);
    CHECK(s.bloomScale==4 && s.exposure==0 && s.gamma==.5f && s.threshold==1);
    auto path=std::wstring(L"build\\settings-roundtrip.ini");
    s.preset(2); s.enabled=false; s.shadows=-.25f;
    CHECK(s.unlockFps);
    s.water=false;s.waterWaves=.075f;s.waterGlints=1.4f;s.waterOpacity=.3f;
    s.waterRefraction=.72f;s.waterTexture=.18f;
    s.waterDepth=.8f;s.waterShore=1.1f;s.waterSSR=.9f;s.waterSSRQuality=1;
    CHECK(s.save(path));
    Settings loaded; CHECK(loaded.load(path));
    CHECK(loaded.unlockFps);
    CHECK(!loaded.enabled && loaded.temperature==s.temperature && loaded.shadows==s.shadows);
    CHECK(!loaded.water&&loaded.waterWaves==s.waterWaves&&loaded.waterGlints==s.waterGlints&&loaded.waterOpacity==s.waterOpacity);
    CHECK(loaded.waterRefraction==s.waterRefraction&&loaded.waterTexture==s.waterTexture);
    CHECK(loaded.waterDepth==s.waterDepth&&loaded.waterShore==s.waterShore&&loaded.waterSSR==s.waterSSR&&loaded.waterSSRQuality==1);
    loaded.resetWaterSurface();CHECK(loaded.waterSSR==.9f&&loaded.waterDepth==.8f);
    loaded.resetWaterScene();CHECK(loaded.waterSSR==Settings{}.waterSSR&&loaded.waterDepth==Settings{}.waterDepth);
    FILE* file=_wfopen(path.c_str(),L"wb"); CHECK(file);
    fputs("unlock_fps=1\nexposure=nan\ngamma=0\nbloom_scale=1e35\nsaturation=999\nunknown=3\nwater_length=0\nwater_waves=99\nwater_opacity=nan\n",file); fclose(file);
    CHECK(loaded.load(path));
    CHECK(loaded.unlockFps); // Explicit opt-in uses the simulation-aware unlock.
    CHECK(loaded.waterRefraction==Settings{}.waterRefraction&&loaded.waterTexture==Settings{}.waterTexture);
    CHECK(loaded.exposure==0 && loaded.gamma==.5f && loaded.bloomScale==4 && loaded.saturation==2);
    CHECK(loaded.waterLength==1&&loaded.waterWaves==.15f&&loaded.waterOpacity==.55f);
    CHECK(!loaded.save(L"build\\missing-directory\\config.ini"));
    Settings before=loaded; CHECK(!loaded.load(L"build\\missing-file.ini"));
    CHECK(loaded.saturation==before.saturation);
    s.unlockFps=true;CHECK(s.save(path));
    CHECK(loaded.load(path)&&loaded.unlockFps);
    file=_wfopen(path.c_str(),L"rb");CHECK(file);
    bool safeUnlock=false;char line[256];
    while(fgets(line,sizeof(line),file))if(!strcmp(line,"unlock_fps=1\n"))safeUnlock=true;
    fclose(file);CHECK(safeUnlock);
    s.unlockFps=true;s.preset(1);CHECK(s.unlockFps);
    DeleteFileW(path.c_str());
    puts("PASS: config round-trip, sanitization, corrupt values, failed IO.");
}
