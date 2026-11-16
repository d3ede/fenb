#pragma once
#include "native_binding.hpp"
#include "hybrid_time.hpp"

// CSky and the terrain cloud plane use separate texture-transform offsets.
// Advance all six by elapsed time, rather than by the number of render passes.
namespace native_layer_time {
inline native_binding::Set patches;
inline float *offsets[6]{},*increments[6]{};
inline double phases[6]{};
inline float written[6]{};
inline uint64_t serial=0;
inline bool installed=false,started=false;
inline void reset() { started=false;serial=0; }
inline void beginFrame(const hybrid_time::Frame &frame) {
    if(!installed || !frame.active || frame.thread!=GetCurrentThreadId() ||
       (started && serial==frame.serial) || !std::isfinite(frame.ticks) || frame.ticks<0 || frame.ticks>16)return;
    for(size_t i=0;i<std::size(offsets);++i) {
        if(!std::isfinite(*offsets[i]))continue;
        if(!started || *offsets[i]!=written[i])phases[i]=*offsets[i];
        phases[i]+=double(*increments[i])*frame.ticks;written[i]=*offsets[i]=float(phases[i]);
    }
    serial=frame.serial;started=true;
}
inline bool install() {
    if(installed)return true;
    native_binding::Image scan(native_patch::image());
    if(!scan.bytes(0x38975a,"F3 0F 10 0D ?? ?? ?? ?? 8D 45 A0 F3 0F 10 15 ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ??") ||
       !scan.bytes(0x3897ef,"F3 0F 11 0D ?? ?? ?? ?? F3 0F 11 15 ?? ?? ?? ?? FF 90 B0 00 00 00") ||
       !scan.bytes(0x2f8d3c,"F3 0F 10 05 ?? ?? ?? ?? F3 0F 10 0D ?? ?? ?? ?? F3 0F 58 C1 F3 0F 11 05 ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ?? F3 0F 58 C1"))return false;
    offsets[0]=reinterpret_cast<float *>(scan.address(0x9abbe8));
    offsets[1]=reinterpret_cast<float *>(scan.address(0x9abbec));
    offsets[2]=reinterpret_cast<float *>(scan.address(0x84215c));offsets[3]=reinterpret_cast<float *>(scan.address(0x842158));
    offsets[4]=reinterpret_cast<float *>(scan.address(0x842164));offsets[5]=reinterpret_cast<float *>(scan.address(0x842160));
    increments[0]=increments[1]=reinterpret_cast<float *>(scan.address(0x7b9c7c));
    increments[2]=increments[3]=reinterpret_cast<float *>(scan.address(0x7b9c80));
    increments[4]=increments[5]=reinterpret_cast<float *>(scan.address(0x7b9c84));
    for(size_t i=0;i<std::size(offsets);++i)if(!offsets[i] || !increments[i])return false;
    if(*increments[0]!=.0002f || *increments[2]!=.001f || *increments[4]!=.0015f)return false;
    patches.clear();
    const unsigned char nop[8]={0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90};
    const DWORD stores[]={0x3897ef,0x3897f7,0x2f8d50,0x2f8d6c,0x2f8d80,0x2f8d94};
    for(size_t i=0;i<std::size(offsets);++i) {
        unsigned char store[8]={0xf3,0x0f,0x11,static_cast<unsigned char>(i==0?0x0d:i==1?0x15:0x05)};
        const uint32_t address=uint32_t(reinterpret_cast<uintptr_t>(offsets[i]));std::memcpy(store+4,&address,4);
        if(!patches.add(scan.address(stores[i]),store,nop,8)) { patches.clear();return false; }
    }
    if(!patches.apply()) { patches.clear();return false; }
    reset();installed=true;Log("Native cloud-layer timing: elapsed-time drift installed for sky and both terrain-cloud layers; render passes do not advance offsets.");return true;
}
inline void uninstall() { installed=false;patches.clear();reset();std::fill(std::begin(offsets),std::end(offsets),nullptr);std::fill(std::begin(increments),std::end(increments),nullptr); }
}
