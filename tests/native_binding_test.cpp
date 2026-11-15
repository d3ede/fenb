#include "native_animation.hpp"
#include "native_mover.hpp"
#include "native_mover_legacy.hpp"
#include "native_mover_services.hpp"
#include "native_world.hpp"
#include "native_camera.hpp"
#include "native_app.hpp"
#include "native_hybrid.hpp"
#include "visual_camera.hpp"
#if __has_include("native_sfx_follow.hpp")
#include "native_sfx_follow.hpp"
#define FENB_BINDING_SFX 1
#endif
#include <cstdio>
#include <vector>

void Log(const char *message) { std::printf("%s\n",message); }
namespace {
struct Region { unsigned char *address;DWORD protection;std::vector<unsigned char> bytes; };
std::vector<Region> snapshot(HMODULE image) {
    auto *base=reinterpret_cast<unsigned char *>(image);
    auto *dos=reinterpret_cast<IMAGE_DOS_HEADER *>(base);
    auto *nt=reinterpret_cast<IMAGE_NT_HEADERS32 *>(base+dos->e_lfanew);
    std::vector<Region> result;
    auto *cursor=base,*end=base+nt->OptionalHeader.SizeOfImage;
    while(cursor<end) {
        MEMORY_BASIC_INFORMATION info{};
        if(!VirtualQuery(cursor,&info,sizeof(info))) return {};
        auto *next=std::min(static_cast<unsigned char *>(info.BaseAddress)+info.RegionSize,end);
        if(next<=cursor) return {};
        if(info.AllocationBase==image && info.State==MEM_COMMIT && !(info.Protect&(PAGE_NOACCESS|PAGE_GUARD))) {
            Region region{cursor,info.Protect,{}};region.bytes.resize(size_t(next-cursor));SIZE_T count=0;
            if(!ReadProcessMemory(GetCurrentProcess(),cursor,region.bytes.data(),region.bytes.size(),&count) || count!=region.bytes.size()) return {};
            result.push_back(std::move(region));
        }
        cursor=next;
    }
    return result;
}
bool identical(const std::vector<Region> &before) {
    bool ok=true;
    for(auto &region:before) {
        std::vector<unsigned char> bytes(region.bytes.size());SIZE_T count=0;
        if(!ReadProcessMemory(GetCurrentProcess(),region.address,bytes.data(),bytes.size(),&count) || count!=bytes.size() || bytes!=region.bytes) {
            size_t offset=0;while(offset<bytes.size() && bytes[offset]==region.bytes[offset]) ++offset;
            std::fprintf(stderr,"Mapped PE changed after uninstall at %p +0x%zx\n",region.address,offset);ok=false;
        }
        // VirtualProtect may split a region around a patch. Every subregion must
        // have its original protection after the reverse transaction finishes.
        auto *cursor=region.address,*end=cursor+region.bytes.size();
        while(cursor<end) {
            MEMORY_BASIC_INFORMATION info{};
            if(!VirtualQuery(cursor,&info,sizeof(info)) || info.Protect!=region.protection) {
                std::fprintf(stderr,"Mapped PE page protection changed at %p: expected 0x%lx, actual 0x%lx\n",cursor,region.protection,info.Protect);ok=false;break;
            }
            auto *next=std::min(static_cast<unsigned char *>(info.BaseAddress)+info.RegionSize,end);
            if(next<=cursor) { ok=false;break; }cursor=next;
        }
    }
    return ok;
}
struct Adapter { const char *name;bool (*install)();void (*uninstall)(); };
bool presentationRoundTrip(HMODULE image) {
    client_detail::Image initial(image);
    const auto cameraBinding=presentation_detail::camera(initial);
    const auto moverBinding=presentation_detail::mover(initial);
    if(!cameraBinding || !moverBinding || cameraBinding.player!=moverBinding.player)return false;
    auto **moverTable=reinterpret_cast<void **>(reinterpret_cast<unsigned char *>(image)+moverBinding.table);
    std::array<void *,10> nativeMover{};std::copy_n(moverTable,10,nativeMover.begin());
    for(int cycle=0;cycle<2;++cycle) {
        visual_animation::install(image);
        const bool animation=visual_animation::enabled;
        const bool camera=visual_camera::install(image);
        const bool moverUnchanged=std::equal(nativeMover.begin(),nativeMover.end(),moverTable);
        std::printf("Presentation cycle %d: animation=%d camera=%d nativeMoverUnchanged=%d\n",cycle+1,animation,camera,moverUnchanged);
        visual_camera::uninstall();visual_animation::uninstall();
        if(!animation || !camera || !moverUnchanged)return false;
    }
    // An ABI mismatch must still reject the image without installing a subset.
    client_detail::Image scan(image);const auto binding=presentation_detail::animation(scan);
    if(!binding)return false;
    auto *table=reinterpret_cast<void **>(reinterpret_cast<unsigned char *>(image)+binding.table);
    auto *entry=static_cast<unsigned char *>(table[0]);
    DWORD protection=0,ignored=0;
    if(!VirtualProtect(entry,1,PAGE_EXECUTE_READWRITE,&protection))return false;
    const auto original=*entry;*entry=0xcc;
    visual_animation::install(image);const bool rejected=!visual_animation::enabled;
    visual_animation::uninstall();*entry=original;
    if(!VirtualProtect(entry,1,protection,&ignored))return false;
    return rejected;
}
}
int wmain(int argc,wchar_t **argv) {
    const bool presentationOnly=argc==3 && !wcscmp(argv[2],L"--presentation");
    if(argc!=2 && !presentationOnly) { std::fprintf(stderr,"Usage: native-binding-test.exe <Homesickness PE copy> [--presentation]\n");return 2; }
    auto image=LoadLibraryExW(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    if(!image) { std::fprintf(stderr,"Could not map supplied PE: Windows error %lu\n",GetLastError());return 3; }
    native_patch::imageOverride=image;
    std::printf("Mapped native PE at %p\n",static_cast<void *>(image));
    // Fault in the mapped image before recording page protections. Relocated
    // writable image pages can become private during their first access.
    if(snapshot(image).empty()) { std::fprintf(stderr,"Could not materialize mapped PE\n");native_patch::imageOverride=nullptr;FreeLibrary(image);return 4; }
    const auto before=snapshot(image);
    if(before.empty()) { std::fprintf(stderr,"Could not snapshot mapped PE\n");native_patch::imageOverride=nullptr;FreeLibrary(image);return 4; }
    if(presentationOnly) {
        const bool ok=presentationRoundTrip(image) && identical(before);
        native_patch::imageOverride=nullptr;FreeLibrary(image);
        if(ok)std::puts("PASS: presentation adapters relocated, ABI rejection retained, every mapped byte and protection restored; no game callback executed.");
        return ok?0:1;
    }
    auto *imageBytes=reinterpret_cast<unsigned char *>(image);
    unsigned char previewCompare[7],previewNullBranch[2];
    std::memcpy(previewCompare,imageBytes+0xdabd5,sizeof(previewCompare));
    std::memcpy(previewNullBranch,imageBytes+0xdabc2,sizeof(previewNullBranch));
    const auto previewFlowPreserved=[&]() {
        return std::memcmp(previewCompare,imageBytes+0xdabd5,sizeof(previewCompare))==0 &&
            std::memcmp(previewNullBranch,imageBytes+0xdabc2,sizeof(previewNullBranch))==0;
    };
    Adapter adapters[]={
        {"animation",&native_animation::install,&native_animation::uninstall},
        {"mover",&native_mover::install,&native_mover::uninstall},
        {"mover legacy",&native_mover_legacy::install,&native_mover_legacy::uninstall},
        {"mover services",&native_mover_services::install,&native_mover_services::uninstall},
        {"world",&native_world::install,&native_world::uninstall},
        {"camera",&native_camera::install,&native_camera::uninstall},
        {"application",&native_app::install,&native_app::uninstall},
#ifdef FENB_BINDING_SFX
        {"SFX followers",&native_sfx_follow::install,&native_sfx_follow::uninstall},
#endif
    };
    bool ok=true;
    for(int cycle=0;cycle<2 && ok;++cycle) {
        size_t installed=0;
        for(auto &adapter:adapters) {
            const bool success=adapter.install();std::printf("Cycle %d: %s %s\n",cycle+1,adapter.name,success ? "installed" : "REJECTED");
            if(!success) { adapter.uninstall();ok=false;break; }++installed;
            if(!previewFlowPreserved()) { std::fprintf(stderr,"Native preview branch/comparison was overwritten\n");ok=false;break; }
        }
        while(installed) adapters[--installed].uninstall();
        if(!identical(before)) ok=false;
    }
    if(ok) {
        ok=native_hybrid::install();std::printf("Central native hybrid install: %s\n",ok ? "accepted" : "REJECTED");
        ok=ok && previewFlowPreserved();
        native_hybrid::uninstall();if(!identical(before)) ok=false;
    }
    if(ok)ok=presentationRoundTrip(image) && identical(before);
    native_patch::imageOverride=nullptr;FreeLibrary(image);
    if(!ok) return 1;
    std::puts("All native bindings installed twice on an entrypoint-free PE mapping and restored every mapped byte and page protection. No native game callback was executed.");
    return 0;
}
