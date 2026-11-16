#pragma once
#include "native_binding.hpp"
#include "hybrid_time.hpp"

// The cloud layer uses a texture transform whose two offsets are incremented
// inside CSky::Render. Keep that transform native, but advance its persistent
// offsets once per elapsed frame rather than once per render pass.
namespace native_layer_time {
inline native_binding::Set patches;
inline float *offsets[2]{},*increment=nullptr;
inline double phases[2]{};
inline float written[2]{};
inline uint64_t serial=0;
inline bool installed=false,started=false;
inline void reset() { started=false;serial=0; }
inline void beginFrame(const hybrid_time::Frame &frame) {
    if(!installed || !frame.active || frame.thread!=GetCurrentThreadId() ||
       (started && serial==frame.serial) || !std::isfinite(frame.ticks) || frame.ticks<0 || frame.ticks>16)return;
    const double step=double(*increment)*frame.ticks;
    for(size_t i=0;i<2;++i) {
        if(!std::isfinite(*offsets[i]))continue;
        if(!started || *offsets[i]!=written[i])phases[i]=*offsets[i];
        phases[i]+=step;written[i]=*offsets[i]=float(phases[i]);
    }
    serial=frame.serial;started=true;
}
inline bool install() {
    if(installed)return true;
    native_binding::Image scan(native_patch::image());
    if(!scan.bytes(0x38975a,"F3 0F 10 0D ?? ?? ?? ?? 8D 45 A0 F3 0F 10 15 ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ??") ||
       !scan.bytes(0x3897ef,"F3 0F 11 0D ?? ?? ?? ?? F3 0F 11 15 ?? ?? ?? ?? FF 90 B0 00 00 00"))return false;
    offsets[0]=reinterpret_cast<float *>(scan.address(0x9abbe8));
    offsets[1]=reinterpret_cast<float *>(scan.address(0x9abbec));
    increment=reinterpret_cast<float *>(scan.address(0x7b9c7c));
    if(!offsets[0] || !offsets[1] || !increment || *increment!=.0002f)return false;
    patches.clear();
    const unsigned char nop[8]={0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90};
    for(size_t i=0;i<2;++i) {
        unsigned char store[8]={0xf3,0x0f,0x11,static_cast<unsigned char>(i ? 0x15 : 0x0d)};
        const uint32_t address=uint32_t(reinterpret_cast<uintptr_t>(offsets[i]));std::memcpy(store+4,&address,4);
        if(!patches.add(scan.address(i ? 0x3897f7 : 0x3897ef),store,nop,8)) { patches.clear();return false; }
    }
    if(!patches.apply()) { patches.clear();return false; }
    reset();installed=true;Log("Native cloud-layer timing: elapsed-time texture drift installed; repeated render passes do not advance offsets.");return true;
}
inline void uninstall() { installed=false;patches.clear();reset();offsets[0]=offsets[1]=increment=nullptr; }
}
