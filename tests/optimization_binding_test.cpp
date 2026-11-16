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
    native_patch::imageOverride=nullptr;FreeLibrary(image);
    printf("Shop-sign ABI: install=%d restored=%d changed-layout-rejected=%d restored-layout-accepted=%d. No game callback executed.\n",installed,restored,rejected,acceptedAgain);
    return installed&&restored&&rejected&&acceptedAgain?0:1;
}
