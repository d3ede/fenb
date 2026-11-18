#pragma once
#include "client_layout.hpp"
#include "native_patch.hpp"
#include "skate_trail.hpp"
#include "draw_call.hpp"
#include <unordered_set>

// CTailEffectBelt is shared with skill trails. Only the two tail kinds with
// the board's 0.03 fade step are emitted by the flight-board path.
namespace native_skate {
using Render=HRESULT(__thiscall *)(void *,IDirect3DDevice9 *);
struct Context {
    const void *trail;
    bool attempted=false,replaced=false;
};
inline thread_local Context *current=nullptr;
inline Render original=nullptr;
inline native_patch::Set patch;
inline std::unordered_set<const void *> owners;
inline std::mutex mutex;
inline HMODULE attempted=nullptr;
inline void **slot=nullptr;

inline bool boardTrail(DWORD kind,DWORD fadeStep) {
    return (kind==1||kind==2)&&fadeStep==0x3cf5c28f;
}
inline HRESULT __fastcall renderHook(void *trail,void *,IDirect3DDevice9 *device) {
    DWORD fields[2]{};
    std::memcpy(fields,static_cast<const BYTE *>(trail)+4,sizeof(DWORD));
    std::memcpy(fields+1,static_cast<const BYTE *>(trail)+0xc,sizeof(DWORD));
    if(!boardTrail(fields[0],fields[1]))return original(trail,device);
    Context context{trail};
    auto *prior=current;
    current=&context;
    const HRESULT result=original(trail,device);
    current=prior;
    return result;
}
inline void **discover(HMODULE module) {
    client_detail::Image image(module);
    const DWORD name=image.string(".?AVCTailEffectBelt@@");
    const DWORD table=name>=8?image.typeTable(name-8):0;
    if(!table||!image.section(table,40,IMAGE_SCN_MEM_READ,IMAGE_SCN_MEM_WRITE|IMAGE_SCN_MEM_EXECUTE))return nullptr;
    const DWORD init=image.relative(image.value(table+4));
    const DWORD process=image.relative(image.value(table+20));
    const DWORD render=image.relative(image.value(table+36));
    // Validate initialization, node age/layout and the fixed-function draw ABI.
    if(!init||!process||!render||
       !image.bytes(init,"55 8B EC 56 8B F1 83 7E 14 00 75 04 8B 06 FF 10 8B 45 08 F3 0F 10 45 0C 89 46 04")||
       !image.bytes(process,"83 79 08 00 74 ?? 83 79 34 00 74 ?? 83 79 28 00 8D 51 28")||
       !image.bytes(render,"55 8B EC 83 E4 F8 81 EC 84 00 00 00 A1 ?? ?? ?? ?? 33 C4 89 84 24 80 00 00 00 53 8B 5D 08 56 57 8B F9 83 7F 08 00 0F 84 ?? ?? ?? ?? 83 7F 34 00 0F 84 ?? ?? ?? ?? 83 7F 10 02"))return nullptr;
    return reinterpret_cast<void **>(reinterpret_cast<BYTE *>(module)+table)+9;
}
inline void request(const void *owner,bool enable) {
    std::lock_guard lock(mutex);
    if(!enable) {
        if(owners.erase(owner)&&owners.empty()) { patch.clear(); original=nullptr; }
        return;
    }
    if(owners.contains(owner))return;
    if(!owners.empty()) { owners.insert(owner); return; }
    HMODULE module=native_patch::image();
    if(module!=attempted) { attempted=module; slot=discover(module); }
    if(!slot)return;
    original=reinterpret_cast<Render>(*slot);
    if(patch.pointer(slot,*slot,reinterpret_cast<void *>(&renderHook))&&patch.apply())owners.insert(owner);
    else { patch.clear(); original=nullptr; }
}
struct Node {
    skate_trail::Vec left,right;
    float colors[8],age;
    const Node *next;
};
static_assert(sizeof(Node)==64&&offsetof(Node,age)==0x38);
inline bool copyMemory(const void *from,void *to,size_t size) {
    SIZE_T read=0;
    return from&&ReadProcessMemory(GetCurrentProcess(),from,to,size,&read)&&read==size;
}
inline size_t points(const void *trail,std::span<skate_trail::Point> output) {
    if(!trail)return 0;
    DWORD fields[7]{};
    if(!copyMemory(static_cast<const BYTE *>(trail)+0x10,fields,sizeof(fields)))return 0;
    const size_t count=fields[0];
    const auto base=uintptr_t(fields[1]);
    auto address=uintptr_t(fields[6]);
    if(count<2||count>512||count>output.size()||!base||!address)return 0;
    std::array<Node,512> pool;
    if(!copyMemory(reinterpret_cast<const void *>(base),pool.data(),sizeof(pool)))return 0;
    for(size_t i=0;i<count;++i) {
        const uintptr_t offset=address-base;
        if(offset>=sizeof(pool)||offset%sizeof(Node))return 0;
        const auto &node=pool[offset/sizeof(Node)];
        const auto across=node.right-node.left;
        const float width=skate_trail::length(across);
        if(!skate_trail::finite(node.left)||!skate_trail::finite(node.right)||!std::isfinite(node.age)||
           node.age<0||node.age>2.01f||width<.005f||width>8)return 0;
        output[i]={(node.left+node.right)*.5f,across*(1/width),skate_trail::smooth(2-node.age)};
        address=reinterpret_cast<uintptr_t>(node.next);
    }
    return address?0:count;
}
}
