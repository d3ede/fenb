#include "settings.hpp"
#include <cstdio>
#include <limits>
#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main() {
    Settings s;
    s.bloomScale=3; s.exposure=std::numeric_limits<float>::infinity(); s.gamma=0; s.threshold=9;
    s.sanitize();
    CHECK(s.bloomScale==4 && s.exposure==0 && s.gamma==.5f && s.threshold==1);
    auto path=std::wstring(L"build\\settings-roundtrip.ini");
    s.preset(2); s.enabled=false; s.shadows=-.25f;
    s.water=false;s.waterWaves=.075f;s.waterGlints=1.4f;s.waterOpacity=.3f;
    CHECK(s.save(path));
    Settings loaded; CHECK(loaded.load(path));
    CHECK(!loaded.enabled && loaded.temperature==s.temperature && loaded.shadows==s.shadows);
    CHECK(!loaded.water&&loaded.waterWaves==s.waterWaves&&loaded.waterGlints==s.waterGlints&&loaded.waterOpacity==s.waterOpacity);
    FILE* file=_wfopen(path.c_str(),L"wb"); CHECK(file);
    fputs("exposure=nan\ngamma=0\nbloom_scale=1e35\nsaturation=999\nunknown=3\nwater_length=0\nwater_waves=99\nwater_opacity=nan\n",file); fclose(file);
    CHECK(loaded.load(path));
    CHECK(loaded.exposure==0 && loaded.gamma==.5f && loaded.bloomScale==4 && loaded.saturation==2);
    CHECK(loaded.waterLength==1&&loaded.waterWaves==.15f&&loaded.waterOpacity==.55f);
    CHECK(!loaded.save(L"build\\missing-directory\\config.ini"));
    Settings before=loaded; CHECK(!loaded.load(L"build\\missing-file.ini"));
    CHECK(loaded.saturation==before.saturation);
    DeleteFileW(path.c_str());
    puts("PASS: config round-trip, sanitization, corrupt values, failed IO.");
}
