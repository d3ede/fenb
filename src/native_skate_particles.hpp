#pragma once
#include "client_layout.hpp"
#include "native_patch.hpp"
#include <array>
#include <unordered_set>

namespace native_skate_particles {
using Render=HRESULT(__thiscall *)(void *,IDirect3DDevice9 *);
struct Batch {
    BYTE *groups=nullptr;
    std::array<BYTE *,2> calls{};
    Render render=nullptr;
};
inline Batch native;
inline thread_local bool active=false;
inline native_patch::Set patch;
inline std::unordered_set<const void *> owners;
inline std::mutex mutex;
inline HMODULE attempted=nullptr;

inline bool boardBatch(const BYTE *groups,const void *group) {
    constexpr unsigned stride=0x3c;
    const auto offset=reinterpret_cast<uintptr_t>(group)-reinterpret_cast<uintptr_t>(groups);
    // Four flight-particle palettes each occupy three consecutive sizes.
    return offset>=13*stride&&offset<25*stride&&offset%stride==0;
}
inline HRESULT __fastcall renderHook(void *group,void *,IDirect3DDevice9 *device) {
    const bool previous=active;
    active=boardBatch(native.groups,group);
    const auto result=native.render(group,device);
    active=previous;
    return result;
}
inline Batch discover(HMODULE module) {
    client_detail::Image image(module);
    const DWORD normal=image.find("55 8B EC 51 56 57 8B 7D 08 33 F6 0F 1F 44 00 00 57 8D 8E ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C6 3C 81 FE 80 07 00 00 75 E9");
    const DWORD alternate=image.find("8B 7F 6C 33 F6 0F 1F 00 57 8D 8E ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C6 3C 81 FE 80 07 00 00 75 E9 83 3D");
    if(!normal||!alternate)return {};
    const DWORD loop=normal+16,otherLoop=alternate+8;
    const DWORD groups=image.relative(image.value(loop+3));
    const DWORD render=loop+12+image.value(loop+8);
    if(!groups||image.relative(image.value(otherLoop+3))!=groups||otherLoop+12+image.value(otherLoop+8)!=render||
       !image.section(groups,32*0x3c,IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE,IMAGE_SCN_MEM_EXECUTE)||
       !image.bytes(render,"55 8B EC 81 EC 80 00 00 00 A1 ?? ?? ?? ?? 33 C5 89 45 F8 53 8B 5D 08 56 57 8B F9 83 7F 2C 00"))
        return {};
    auto *base=reinterpret_cast<BYTE *>(module);
    return {base+groups,{base+loop+7,base+otherLoop+7},reinterpret_cast<Render>(base+render)};
}
inline void request(const void *owner,bool enable) {
    std::lock_guard lock(mutex);
    if(!enable) {
        if(owners.erase(owner)&&owners.empty())patch.clear();
        return;
    }
    if(owners.contains(owner))return;
    if(!owners.empty()) { owners.insert(owner); return; }
    HMODULE module=native_patch::image();
    if(module!=attempted) { attempted=module; native=discover(module); }
    if(!native.render)return;
    for(auto *call:native.calls) {
        BYTE original[5]{0xe8};
        const auto displacement=uint32_t(reinterpret_cast<uintptr_t>(native.render)-reinterpret_cast<uintptr_t>(call)-5);
        std::memcpy(original+1,&displacement,4);
        if(!patch.call(call,original,reinterpret_cast<void *>(&renderHook))) { patch.clear();return; }
    }
    if(patch.apply())owners.insert(owner);
    else patch.clear();
}
}
