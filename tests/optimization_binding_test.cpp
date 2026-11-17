#include "optimization.cpp"
#include "native_hybrid.hpp"
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
    if(!effects.owners()){puts("FAIL: pet effect-owner ABI discovery");return 6;}
    const auto actors=optimization_detail::discoverActors(effects);
    if(!actors){puts("FAIL: actor render-list ABI discovery");return 7;}
    std::array<std::array<unsigned char,5>,2> calls{};
    bool effectsInstalled=true;
    for(size_t i=0;i<2;++i){std::memcpy(calls[i].data(),effects.calls[i],5);effectsInstalled&=patches.call(effects.calls[i],calls[i].data(),reinterpret_cast<void *>(&optimization_detail::effectsHook));}
    unsigned char sfxCall[5];std::memcpy(sfxCall,effects.sfxCall,5);
    const unsigned char virtualCall[5]={0x8b,0x06,0xff,0x50,0x24};
    effectsInstalled=effectsInstalled&&!std::memcmp(sfxCall,virtualCall,5)&&
        patches.call(effects.sfxCall,virtualCall,reinterpret_cast<void *>(&optimization_detail::sfxRenderHook));
    effectsInstalled=effectsInstalled&&patches.apply();patches.clear();
    unsigned char sortCall[5];std::memcpy(sortCall,actors.sortCall,5);
    const bool actorInstalled=patches.call(actors.sortCall,sortCall,reinterpret_cast<void *>(&optimization_detail::actorSortHook))&&patches.apply();
    patches.clear();
    const bool actorRestored=!std::memcmp(sortCall,actors.sortCall,5);
    bool effectsRestored=true;
    for(size_t i=0;i<2;++i)effectsRestored&=!std::memcmp(calls[i].data(),effects.calls[i],5);
    effectsRestored&=!std::memcmp(sfxCall,effects.sfxCall,5);
    const int savedDecorative=*effects.decorative,savedSkills=*effects.skills;
    Settings settings;settings.hideDecorativeEffects=settings.hideSkillEffects=true;
    {optimization_detail::EffectsScope scope(effects,settings);effectsRestored&=*effects.decorative==1&&*effects.skills==1;}
    effectsRestored&=*effects.decorative==savedDecorative&&*effects.skills==savedSkills;
    bool effectsRejected=false;
    const DWORD flags=scan.find("83 3D ?? ?? ?? ?? 00 74 09 83 BE E8 05 00 00 00 74 12 83 3D ?? ?? ?? ?? 00 74 49 83 BE E8 05 00 00 00 74 40");
    if(flags){auto *offset=reinterpret_cast<unsigned char *>(image)+flags+11;DWORD protection=0,ignored=0;if(VirtualProtect(offset,1,PAGE_EXECUTE_READWRITE,&protection)){const auto saved=*offset;*offset^=4;effectsRejected=!optimization_detail::discoverEffects();*offset=saved;VirtualProtect(offset,1,protection,&ignored);}}
    bool stackRejected=false;
    auto *stackSize=reinterpret_cast<unsigned char *>(effects.render)+39;
    DWORD protection=0,ignored=0;
    if(VirtualProtect(stackSize,1,PAGE_EXECUTE_READWRITE,&protection)) {
        const auto saved=*stackSize;*stackSize=0x58;
        stackRejected=!optimization_detail::discoverEffects();*stackSize=saved;
        VirtualProtect(stackSize,1,protection,&ignored);
    }
    bool ownerGuardRejected=false;
    const DWORD pet=scan.find("83 BE 1C 03 00 00 00 75 60 83 BE 8C 01 00 00 05 75 57 8D 45 F8 B9 ?? ?? ?? ?? 50 FF B6 40 03 00 00 E8");
    if(pet) {
        auto *aiField=reinterpret_cast<unsigned char *>(image)+pet+11;
        if(VirtualProtect(aiField,1,PAGE_EXECUTE_READWRITE,&protection)) {
            const auto saved=*aiField;*aiField^=4;
            const auto changed=optimization_detail::discoverEffects();
            ownerGuardRejected=bool(changed)&&!changed.owners();*aiField=saved;
            VirtualProtect(aiField,1,protection,&ignored);
        }
    }
    bool actorGuardRejected=false;
    const DWORD peace=scan.find("33 C0 83 B9 E8 06 00 00 01 0F 94 C0 C3 CC CC CC");
    if(peace) {
        auto *offset=reinterpret_cast<unsigned char *>(image)+peace+4;
        if(VirtualProtect(offset,1,PAGE_EXECUTE_READWRITE,&protection)) {
            const auto saved=*offset;*offset^=4;
            actorGuardRejected=!optimization_detail::discoverActors(effects);*offset=saved;
            VirtualProtect(offset,1,protection,&ignored);
        }
    }
    printf("Actor render list: installed=%d restored=%d changed-NPC-field-rejected=%d. No game callback executed.\n",actorInstalled,actorRestored,actorGuardRejected);
    printf("Pet SFX dispatch: installed=%d restored=%d changed-owner-layout-rejected=%d. No game callback executed.\n",effectsInstalled,effectsRestored,ownerGuardRejected);
    const bool hybrid=native_hybrid::install();
    const bool background=hybrid&&native_background::install();
    const auto alongside=optimization_detail::discoverEffects();
    bool coexist=hybrid&&background&&alongside.owners()&&bool(optimization_detail::discoverActors(alongside));
    if(coexist) {
        Optimization option;Settings pets;pets.hidePetEffects=true;option.update(pets);
        coexist=optimization_detail::state().effectsInstalled&&native_hybrid::installed&&native_background::installed;
        pets.hidePetEffects=false;option.update(pets);
        coexist=coexist&&!optimization_detail::state().effectsInstalled&&native_hybrid::installed&&native_background::installed;
        pets.renderPlayers=pets.renderMonsters=pets.renderPets=false;option.update(pets);
        coexist=coexist&&optimization_detail::state().effectsInstalled&&optimization_detail::state().installed&&native_hybrid::installed&&native_background::installed;
        pets=Settings{};option.update(pets);
        coexist=coexist&&!optimization_detail::state().effectsInstalled&&!optimization_detail::state().installed&&
            !std::memcmp(sortCall,actors.sortCall,5)&&native_hybrid::installed&&native_background::installed;
    }
    native_background::uninstall();native_hybrid::uninstall();
    printf("Pet filters with native FPS/background hooks: installed and removed independently=%d.\n",coexist);
    printf("Native effects ABI: install=%d restored=%d changed-skill-field-rejected=%d unknown-stack-rejected=%d. No game callback executed.\n",effectsInstalled,effectsRestored,effectsRejected,stackRejected);
    native_patch::imageOverride=nullptr;FreeLibrary(image);
    printf("Shop-sign ABI: install=%d restored=%d changed-layout-rejected=%d restored-layout-accepted=%d. No game callback executed.\n",installed,restored,rejected,acceptedAgain);
    return installed&&restored&&rejected&&acceptedAgain&&effectsInstalled&&effectsRestored&&effectsRejected&&stackRejected&&ownerGuardRejected&&coexist&&actorInstalled&&actorRestored&&actorGuardRejected?0:1;
}
