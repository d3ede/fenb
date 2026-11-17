#include "optimization.cpp"
#include <cstdio>
void Log(const char *) {}
int wmain(int argc,wchar_t **argv) {
    if(argc!=2)return 2;
    HMODULE image=LoadLibraryExW(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    if(!image)return 3;
    native_patch::imageOverride=image;
    auto binding=optimization_detail::discover();
    if(!binding){puts("FAIL: shop-sign ABI discovery");FreeLibrary(image);return 4;}
    unsigned char before[5];std::memcpy(before,binding.call,5);
    native_patch::Set patches;
    const bool installed=patches.call(binding.call,before,reinterpret_cast<void *>(&optimization_detail::renderHook))&&patches.apply();
    patches.clear();
    const bool restored=!std::memcmp(before,binding.call,5);
    client_detail::Image scan(image);
    const DWORD loop=scan.find("8B 7E 78 83 BF B8 02 00 00 02 0F 84 ?? ?? ?? ?? 8D 85 28 F7 FF FF");
    bool rejected=false;
    if(loop) {
        auto *ownerOffset=reinterpret_cast<unsigned char *>(image)+loop+2;
        DWORD protection=0,ignored=0;
        if(VirtualProtect(ownerOffset,1,PAGE_EXECUTE_READWRITE,&protection)) {
            const auto saved=*ownerOffset;*ownerOffset^=4;
            rejected=!optimization_detail::discover();*ownerOffset=saved;
            VirtualProtect(ownerOffset,1,protection,&ignored);
        }
    }
    const bool acceptedAgain=bool(optimization_detail::discover());
    const auto effects=optimization_detail::discoverEffects();
    if(!effects){puts("FAIL: native effect-filter ABI discovery");return 5;}
    std::array<std::array<unsigned char,5>,2> calls{};
    bool effectsInstalled=true;
    for(size_t i=0;i<2;++i){std::memcpy(calls[i].data(),effects.calls[i],5);effectsInstalled&=patches.call(effects.calls[i],calls[i].data(),reinterpret_cast<void *>(&optimization_detail::effectsHook));}
    effectsInstalled=effectsInstalled&&patches.apply();patches.clear();
    bool effectsRestored=true;
    for(size_t i=0;i<2;++i)effectsRestored&=!std::memcmp(calls[i].data(),effects.calls[i],5);
    const int savedDecorative=*effects.decorative,savedSkills=*effects.skills;
    Settings settings;settings.hideDecorativeEffects=settings.hideSkillEffects=true;
    {optimization_detail::EffectsScope scope(effects,settings);effectsRestored&=*effects.decorative==1&&*effects.skills==1;}
    effectsRestored&=*effects.decorative==savedDecorative&&*effects.skills==savedSkills;
    bool effectsRejected=false;
    const DWORD flags=scan.find("83 3D ?? ?? ?? ?? 00 74 09 83 BE E8 05 00 00 00 74 12 83 3D ?? ?? ?? ?? 00 74 49 83 BE E8 05 00 00 00 74 40");
    if(flags){auto *offset=reinterpret_cast<unsigned char *>(image)+flags+11;DWORD protection=0,ignored=0;if(VirtualProtect(offset,1,PAGE_EXECUTE_READWRITE,&protection)){const auto saved=*offset;*offset^=4;effectsRejected=!optimization_detail::discoverEffects();*offset=saved;VirtualProtect(offset,1,protection,&ignored);}}
    printf("Native effects ABI: install=%d restored=%d changed-skill-field-rejected=%d. No game callback executed.\n",effectsInstalled,effectsRestored,effectsRejected);
    native_patch::imageOverride=nullptr;FreeLibrary(image);
    printf("Shop-sign ABI: install=%d restored=%d changed-layout-rejected=%d restored-layout-accepted=%d. No game callback executed.\n",installed,restored,rejected,acceptedAgain);
    return installed&&restored&&rejected&&acceptedAgain&&effectsInstalled&&effectsRestored&&effectsRejected?0:1;
}
