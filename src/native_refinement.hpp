#pragma once
#include "native_binding.hpp"
#include <unordered_set>

// A render-only scope identifies one native particle model. It never advances
// simulation or edits an emitter, actor, lifetime, texture or game file.
namespace native_refinement {
struct Context {
    void *model = nullptr, *consumer = nullptr;
    void (*finish)(void *) = nullptr;
    float origin[3]{};
    bool hasOrigin = false;
};
inline thread_local Context *current = nullptr;
struct Scope {
    Context context;
    Context *previous = current;
    explicit Scope(void *model,const float *origin=nullptr) : context{model} {
        if(origin){std::memcpy(context.origin,origin,sizeof(context.origin));context.hasOrigin=true;}
        current = &context;
    }
    ~Scope() {
        if (context.finish) context.finish(context.consumer);
        current = previous;
    }
};
using Render = BOOL(__thiscall *)(void *, void *, void *);
inline Render original = nullptr;
inline native_patch::Set patches;
inline std::unordered_set<const void *> owners;
inline std::mutex mutex;
inline HMODULE attempted = nullptr;
inline bool available = false, positionAvailable = false;
inline BOOL __fastcall renderHook(void *model, void *, void *device, void *matrix) {
    // The validated native render passes this world-space emitter position to
    // its particle elements. Copy it; never retain the model's field address.
    float origin[3]{};
    if(positionAvailable)std::memcpy(origin,static_cast<char *>(model)+0x12c,sizeof(origin));
    Scope scope(model,positionAvailable?origin:nullptr);
    return original(model, device, matrix);
}
inline bool install() {
    const auto module = native_patch::image();
    if (attempted == module && !available) return false;
    attempted = module; available = positionAvailable = false;
    native_binding::Image binding(module);
    auto **table = reinterpret_cast<void **>(binding.address(0x7b73f4));
    auto *type = binding.address(0x837fbc);
    client_detail::Image image(module);
    if (!table || !type || image.typeTable(DWORD(type - reinterpret_cast<unsigned char *>(module))) !=
            DWORD(reinterpret_cast<unsigned char *>(table) - reinterpret_cast<unsigned char *>(module))) return false;
    const auto method = image.relative(reinterpret_cast<uintptr_t>(table[19]));
    // CSfxModel::Render(device, matrix), including the accessed asset field.
    if (!method || !image.bytes(method,"55 8B EC 83 E4 F8 51 53 56 8B F1 57 83 BE E8 00 00 00 00")) return false;
    // Rebuilt hosts can move the method. Enable the head anchor only if this
    // implementation also reads all three coordinates at the known field.
    std::array<unsigned char,0x500> body{};
    constexpr unsigned char xy[]={0xf3,0x0f,0x7e,0x86,0x2c,0x01,0x00,0x00};
    constexpr unsigned char z[]={0x8b,0x86,0x34,0x01,0x00,0x00};
    if(image.read(method,body.data(),body.size())){
        const auto at=std::search(body.begin(),body.end(),std::begin(xy),std::end(xy));
        if(at!=body.end())positionAvailable=std::search(at,std::min(body.end(),at+64),std::begin(z),std::end(z))!=std::min(body.end(),at+64);
    }
    original = reinterpret_cast<Render>(table[19]);
    patches.clear();
    if (!patches.pointer(table + 19, table[19], reinterpret_cast<void *>(&renderHook)) || !patches.apply()) {
        patches.clear(); original = nullptr; positionAvailable=false; return false;
    }
    available = true; return true;
}
inline void request(const void *owner, bool enabled) {
    std::lock_guard lock(mutex);
    if (enabled) {
        if (owners.contains(owner)) return;
        if (!owners.empty() || install()) owners.insert(owner);
    } else {
        owners.erase(owner);
        if (owners.empty()) { patches.clear(); original = nullptr; positionAvailable=false; }
    }
}
}
