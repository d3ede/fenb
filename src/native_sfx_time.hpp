#pragma once
#include "native_binding.hpp"
#include "hybrid_time.hpp"

// CSfxModel::Process advances particle motion, keyframes and expiry by one
// native tick. Some callers live in Render, others in the continuous world.
// Share a per-model budget across those callers and every pass of one frame.
namespace native_sfx_time {
template<class T> T &field(void *p,size_t at) { return *reinterpret_cast<T *>(static_cast<char *>(p)+at); }
using Process=BOOL(__thiscall *)(void *);
using Destroy=void *(__thiscall *)(void *,unsigned);
inline Process original=nullptr;
inline Destroy originalDestroy=nullptr;
inline native_binding::Set patches;
inline bool installed=false;
inline uint64_t serial=0;
inline DWORD thread=0;
inline unsigned budget=0;
struct Clock { uint64_t serial=0;unsigned used=0;bool ended=false;WORD endFrame=0;void *asset=nullptr; };
inline std::unordered_map<void *,Clock> clocks;
inline std::mutex mutex;
inline void reset() { std::lock_guard lock(mutex);clocks.clear();budget=0;thread=0;serial=0; }
inline void beginFrame(const hybrid_time::Frame &frame) {
    std::lock_guard lock(mutex);
    if(clocks.size()>65536)clocks.clear();
    serial=frame.serial;thread=frame.thread;budget=std::min(frame.legacy,16u);
}
inline bool active() { return installed && hybrid_time::frame.active && hybrid_time::frame.thread==GetCurrentThreadId() &&
    hybrid_time::frame.serial==serial && thread==GetCurrentThreadId(); }
inline BOOL __fastcall processHook(void *model,void *) {
    if(!active())return original(model);
    unsigned count=hybrid_time::frame.stage==hybrid_time::Stage::Legacy ? 1u :
        hybrid_time::frame.stage==hybrid_time::Stage::World ? hybrid_time::frame.legacy : budget;
    count=std::min(count,budget);
    for(unsigned i=0;i<count;++i) {
        {
            std::lock_guard lock(mutex);auto &clock=clocks[model];
            if(clock.serial!=serial) { clock.serial=serial;clock.used=0;clock.ended=false; }
            auto asset=field<void *>(model,0xe8);
            if(clock.asset!=asset || field<WORD>(model,0x150)!=clock.endFrame)clock.ended=false;
            clock.asset=asset;
            if(clock.used>=budget || clock.ended)return FALSE;
            ++clock.used;
        }
        const BOOL ended=original(model);
        if(ended) {
            std::lock_guard lock(mutex);
            if(auto found=clocks.find(model);found!=clocks.end()) {
                found->second.ended=true;found->second.endFrame=field<WORD>(model,0x150);
            }
            return ended; // Native caller owns looping, model replacement and deletion.
        }
    }
    return FALSE;
}
inline void *__fastcall destroyHook(void *model,void *,unsigned flags) {
    { std::lock_guard lock(mutex);clocks.erase(model); }
    return originalDestroy(model,flags);
}
inline bool install() {
    if(installed)return true;
    native_binding::Image scan(native_patch::image());
    if(!scan.bytes(0x352380,"55 8B EC 83 E4 F8 83 EC 6C 53 8B D9 33 D2 56") ||
       !scan.bytes(0x34f230,"55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1") ||
       !scan.bytes(0x837fbc,"?? ?? ?? ?? ?? ?? ?? ?? 2E 3F 41 56 43 53 66 78 4D 6F 64 65 6C 40 40 00"))return false;
    auto table=reinterpret_cast<void **>(scan.address(0x7b73f4));
    auto locator=table ? field<void *>(table-1,0) : nullptr;
    if(!table || table[0]!=scan.address(0x34f230) || !locator || field<void *>(locator,12)!=scan.address(0x837fbc))return false;
    original=reinterpret_cast<Process>(scan.address(0x352380));originalDestroy=reinterpret_cast<Destroy>(table[0]);
    patches.clear();
    for(DWORD site:native_binding::catalog::sfxProcessCalls) {
        if(!scan.calls(site,0x352380)) { patches.clear();return false; }
        unsigned char call[5]={0xe8};const uint32_t offset=0x352380u-site-5u;std::memcpy(call+1,&offset,4);
        if(!patches.call(scan.address(site),call,reinterpret_cast<void *>(&processHook))) { patches.clear();return false; }
    }
    if(!patches.pointer(table,table[0],reinterpret_cast<void *>(&destroyHook)) || !patches.apply()) { patches.clear();return false; }
    reset();installed=true;Log("Native SFX timing: shared legacy budgets installed for world, attachment and render-driven effects.");return true;
}
inline void uninstall() { installed=false;patches.clear();reset();original=nullptr;originalDestroy=nullptr; }
}
