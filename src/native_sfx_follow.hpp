#pragma once
#include "common.hpp"
#include "native_binding.hpp"
#include "hybrid_time.hpp"
#include "native_patch.hpp"
#include "native_animation.hpp"
#include <array>
#include <cstring>
#include <limits>

// Explicit equivalents of Shining's added no-legacy-tick virtual callbacks.
// They refresh native attachment coordinates only: no Process, lifetime, hit,
// sound, particle emission or animation calls occur in this component.
namespace native_sfx_follow {
struct Vec { float x=0,y=0,z=0; };
using Matrix=std::array<float,16>;
template<class T> T &field(void *p,size_t at) { return *reinterpret_cast<T *>(static_cast<char *>(p)+at); }
using Resolve=BOOL(__thiscall *)(void *,uint32_t,void **);
using SetPos=void(__thiscall *)(void *,const Vec *);
using SetAngle=void(*)(void *,float);
using Hand=void(__thiscall *)(void *,Vec *,int,const float *);
using Force=void(__thiscall *)(void *,Vec *,int,int,const float *);
using RefreshBones=bool(*)(void *);
enum class Kind { NoOp,Destination,Source,Stretch,Collect,Charge,Cannon,Mushmoot,Parts,AllowParts,
    Secondary,SecondaryMover,SecondaryPlusOne,SecondaryScaled,PrimaryYaw,HotAir,Wave,Brandish,CleanHit,OverCutter };
struct Record { const char *name;DWORD vtable,type,process;Kind kind; };
inline const Record records[]={
    {".?AVCSfxRotate@@",0x7b4a98,0x837568,0x343540,Kind::NoOp},
    {".?AVCSfx@@",0x7b75a8,0x83760c,0x357da0,Kind::Destination},
    {".?AVCSfxCollect@@",0x7b4bf8,0x8375f0,0x341180,Kind::Collect},
    {".?AVCSfxClockWorksCharge@@",0x7b4ba0,0x8375cc,0x341630,Kind::Charge},
    {".?AVCSfxClockWorksCannon@@",0x7b4b48,0x8375a8,0x3418b0,Kind::Cannon},
    {".?AVCSfxMushmootCharge@@",0x7b4af0,0x837584,0x3413a0,Kind::Mushmoot},
    {".?AVCSfxPartsLink@@",0x7b4990,0x837528,0x3449c0,Kind::Parts},
    {".?AVCSfxPartsLinkBlade@@",0x7b4938,0x8374e4,0x344c40,Kind::Parts},
    {".?AVCSfxPartsLinkJst@@",0x7b49e8,0x837508,0x344ca0,Kind::Parts},
    {".?AVCSfxAllowPartsLink@@",0x7b4a40,0x837544,0x344cc0,Kind::AllowParts},
    {".?AVCSfxReady@@",0x7b48e0,0x8374cc,0x344f20,Kind::Destination},
    {".?AVCSfxGenMoveMark@@",0x7b6d90,0x837f3c,0x338c90,Kind::NoOp},
    {".?AVCSfxShoot@@",0x7b5f78,0x837f24,0x343970,Kind::NoOp},
    {".?AVCSfxGenNormalDmg01@@",0x7b72b8,0x837e7c,0x337630,Kind::NoOp},
    {".?AVCSfxTroStretching01@@",0x7b7050,0x837e58,0x338060,Kind::Stretch},
    {".?AVCSfxTroStretching02@@",0x7b6f48,0x837e34,0x338300,Kind::Stretch},
    {".?AVCSfxTroBlitz@@",0x7b6fa0,0x837e18,0x338570,Kind::NoOp},
    {".?AVCSfxGenRestoration01@@",0x7b71b0,0x837d90,0x337a90,Kind::Source},
    {".?AVCSfxGenLogin@@",0x7b6e98,0x837d18,0x338740,Kind::Source},
    {".?AVCSfxGenWarp@@",0x7b6ef0,0x837cfc,0x338880,Kind::Source},
    {".?AVCSfxGenPcDie@@",0x7b6de8,0x837ce0,0x3389c0,Kind::Source},
    {".?AVCSfxGenWaterCircle@@",0x7b6ce0,0x837c98,0x338e00,Kind::NoOp},
    {".?AVCSfxGenRainCircle@@",0x7b6d38,0x837c78,0x338f40,Kind::NoOp},
    {".?AVCSfxGenWaterCrown@@",0x7b6c30,0x837c58,0x339020,Kind::NoOp},
    {".?AVCSfxItemWandAtk1@@",0x7b6bd8,0x837c38,0x339ae0,Kind::NoOp},
    {".?AVCSfxItemRangeAtk1@@",0x7b6b80,0x837c18,0x33a160,Kind::NoOp},
    {".?AVCSfxItemRangeAtk1_Allow@@",0x7b6ad0,0x837bf0,0x33c570,Kind::NoOp},
    {".?AVCSfxItemRangeAtk1_AllowRain@@",0x7b69c8,0x837bc4,0x33c800,Kind::NoOp},
    {".?AVCSfxItemRangeAtk1_Stone@@",0x7b6a20,0x837b9c,0x33cb20,Kind::NoOp},
    {".?AVCSfxItemRangeAtk1_StoneRain@@",0x7b6918,0x837b70,0x33ce10,Kind::NoOp},
    {".?AVCSfxItemRangeAtk_JunkBow@@",0x7b6a78,0x837b48,0x33baa0,Kind::NoOp},
    {".?AVCSfxItemYoyoAtk@@",0x7b6b28,0x837b28,0x33b410,Kind::NoOp},
    {".?AVCSfxAtkStraight@@",0x7b6970,0x837b08,0x33d420,Kind::NoOp},
    {".?AVCSfxItemWandAtkAir@@",0x7b6c88,0x837ae4,0x339490,Kind::NoOp},
    {".?AVCSfxItemWandAtk2@@",0x7b68c0,0x837ac4,0x33d820,Kind::SecondaryPlusOne},
    {".?AVCSfxItemWandAtk3@@",0x7b6810,0x837aa4,0x33dce0,Kind::SecondaryPlusOne},
    {".?AVCSfxItemWandAtk4@@",0x7b6868,0x837a84,0x33e1a0,Kind::SecondaryPlusOne},
    {".?AVCSfxSkillMerKeenWheel@@",0x7b6600,0x8379f0,0x33ed00,Kind::NoOp},
    {".?AVCSfxSkillMerSplmash@@",0x7b6658,0x8379cc,0x33ee40,Kind::NoOp},
    {".?AVCSfxSkillMerBlindSide@@",0x7b6550,0x8379a8,0x33f0a0,Kind::NoOp},
    {".?AVCSfxSkillMagFireCasting@@",0x7b63f0,0x837980,0x33fa40,Kind::NoOp},
    {".?AVCSfxSkillCastingFrontDynamic@@",0x7b6130,0x837ef8,0x3431a0,Kind::NoOp},
    {".?AVCSfxSkillCastingLSideDynamic@@",0x7b6028,0x837ecc,0x343300,Kind::NoOp},
    {".?AVCSfxSkillAboveTargetDynamic@@",0x7b6080,0x837ea0,0x343460,Kind::NoOp},
    {".?AVCSfxSkillMagWindCasting@@",0x7b6448,0x837958,0x33fb70,Kind::NoOp},
    {".?AVCSfxSkillAssBurstcrack@@",0x7b65a8,0x837930,0x33f1a0,Kind::PrimaryYaw},
    {".?AVCSfxSkillAssTampinghole@@",0x7b64a0,0x837908,0x33f310,Kind::PrimaryYaw},
    {".?AVCSfxNpcDirSteam@@",0x7b64f8,0x8378e8,0x33f530,Kind::NoOp},
    {".?AVCSfxSkillMagStrongWind@@",0x7b6340,0x8378c0,0x33fd60,Kind::SecondaryMover},
    {".?AVCSfxSkillMagSwordWind@@",0x7b6398,0x83789c,0x340230,Kind::Secondary},
    {".?AVCSfxSkillMagFireBoomerang@@",0x7b6290,0x837874,0x3406d0,Kind::SecondaryScaled},
    {".?AVCSfxSkillMagHotAir@@",0x7b6238,0x83782c,0x340ec0,Kind::HotAir},
    {".?AVCSfxSkillMagIceMissile@@",0x7b6188,0x837804,0x341db0,Kind::SecondaryScaled},
    {".?AVCSfxSkillMagLightningBall@@",0x7b61e0,0x8377dc,0x3424a0,Kind::SecondaryScaled},
    {".?AVCSfxSkillMagSpikeStone@@",0x7b60d8,0x8377b4,0x342b90,Kind::SecondaryScaled},
    {".?AVCSfxShootWave@@",0x7b5fd0,0x837798,0x3440e0,Kind::Wave},
    {".?AVCSfxFixed@@",0x7b66b0,0x837780,0x33e980,Kind::NoOp},
    {".?AVCSfxPartsLinkShoulder@@",0x7b5f20,0x83775c,0x3450e0,Kind::NoOp},
    {".?AVCSfxCursor@@",0x7b5ec8,0x837740,0x345710,Kind::NoOp},
    {".?AVCSfxLinkMover@@",0x7b5e18,0x837724,0x345870,Kind::NoOp},
    {".?AVCSfxCrown@@",0x7b5dc0,0x8376f0,0x345c10,Kind::NoOp},
    {".?AVCSfxInchant@@",0x7b5e70,0x837708,0x357da0,Kind::Destination},
    {".?AVCSfxSkillMagFireBomb@@",0x7b62e8,0x837850,0x340ce0,Kind::NoOp},
    {".?AVCSfxSkillVagBrandish@@",0x7b6708,0x837a14,0x33eb00,Kind::Brandish},
    {".?AVCSfxSkillVagCleanHit@@",0x7b67b8,0x837a38,0x33e7b0,Kind::CleanHit},
    {".?AVCSfxSkillVagOverCutter@@",0x7b6760,0x837a5c,0x33e580,Kind::OverCutter},
    {".?AVCSfxGenMonsterSpawn@@",0x7b6e40,0x837cbc,0x338b00,Kind::Source},
    {".?AVCSfxGenLevelUp@@",0x7b6ff8,0x837d34,0x337e50,Kind::Stretch},
    {".?AVCSfxGenIncrease01@@",0x7b7100,0x837d54,0x337d10,Kind::Source},
    {".?AVCSfxGenCure@@",0x7b70a8,0x837d74,0x337bd0,Kind::Source},
    {".?AVCSfxGenMonsterDmg01@@",0x7b7158,0x837db4,0x337940,Kind::Source},
    {".?AVCSfxGenSkillDmg01@@",0x7b7260,0x837dd8,0x337830,Kind::NoOp},
    {".?AVCSfxGenSuperDmg01@@",0x7b7208,0x837df8,0x337720,Kind::NoOp},
};
inline std::unordered_map<void **,Kind> bindings;
inline bool installed=false;
inline Resolve resolve=nullptr;
inline SetPos setPos=nullptr;
inline SetAngle setAngle=nullptr;
inline Hand hand=nullptr;
inline Force force=nullptr;
inline RefreshBones boneRefresh=&native_animation::refreshBones;
inline void *ctrlMap=nullptr,*angleEntry=nullptr;
inline bool active() { return installed && hybrid_time::frame.active && hybrid_time::frame.thread==GetCurrentThreadId() &&
    hybrid_time::frame.stage==hybrid_time::Stage::World; }
inline bool finite(Vec v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline bool finite(const Matrix &m) { for(float f:m) if(!std::isfinite(f)) return false;return true; }
inline bool valid(void *p) { return p && !(field<DWORD>(p,4)&1u); }
inline void *target(void *object,size_t id,bool mover=false) {
    if(!resolve || !ctrlMap) return nullptr;
    const uint32_t value=field<uint32_t>(object,id);void *p=nullptr;
    if(value==UINT32_MAX || !resolve(ctrlMap,value,&p) || !valid(p) || (mover && field<DWORD>(p,0x170)!=5)) return nullptr;
    return p;
}
inline void position(void *object,Vec value) { if(finite(value) && setPos) setPos(object,&value); }
inline void angle(void *object,float value) { if(std::isfinite(value) && setAngle) setAngle(object,value); }
inline Vec transform(Vec v,const float *m) {
    const float w=v.x*m[3]+v.y*m[7]+v.z*m[11]+m[15];
    if(!std::isfinite(w) || std::fabs(w)<1e-12f) return {NAN,NAN,NAN};
    return {(v.x*m[0]+v.y*m[4]+v.z*m[8]+m[12])/w,
        (v.x*m[1]+v.y*m[5]+v.z*m[9]+m[13])/w,(v.x*m[2]+v.y*m[6]+v.z*m[10]+m[14])/w};
}
inline bool ownerMatrix(void *owner,Matrix &world) {
    std::memcpy(world.data(),static_cast<char *>(owner)+0xe4,64);return finite(world);
}
inline bool bonePosition(void *owner,int event,Vec &out) {
    if(event<0 || event>=8) return false;
    auto model=field<void *>(owner,0x178);if(!model) return false;
    if(field<void *>(model,0x108) && (!boneRefresh || !boneRefresh(model))) return false;
    auto bones=field<void *>(model,0x100);
    auto matrices=field<float *>(model,0x118);
    if(!bones || !matrices) return false;
    const int count=field<int>(bones,0x44),index=field<int>(bones,size_t(0x1c0)+size_t(event)*4);
    if(count<1 || count>256 || index<0 || index>=count) return false;
    Matrix world;if(!ownerMatrix(owner,world)) return false;
    const Vec local=field<Vec>(bones,size_t(0x160)+size_t(event)*12);
    out=transform(transform(local,matrices+size_t(index)*16),world.data());return finite(out);
}
inline bool handPosition(void *owner,int part,bool collector,Vec &out) {
    auto model=field<void *>(owner,0x178);if(!model) return false;
    if(field<void *>(model,0x108) && (!boneRefresh || !boneRefresh(model))) return false;
    auto bones=field<void *>(model,0x100);
    if(!bones || !field<void *>(model,0x118)) return false;
    const int count=field<int>(bones,0x44),index=field<int>(bones,part==10 ? 0x150 : 0x154);
    if(count<1 || count>256 || index<0 || index>=count) return false;
    Matrix world;if(!ownerMatrix(owner,world)) return false;
    out={NAN,NAN,NAN};
    if(collector) { if(!force) return false;force(model,&out,0,part,world.data()); }
    else { if(!hand) return false;hand(model,&out,part,world.data()); }
    return finite(out);
}
inline bool refresh(void *object) {
    if(!active() || !valid(object)) return false;
    auto found=bindings.find(field<void **>(object,0));
    if(found==bindings.end()) return false; // no inheritance fallback for unknown classes
    const Kind kind=found->second;
    if(kind==Kind::NoOp) return true;
    if(kind==Kind::Destination) {
        if(field<uint32_t>(object,0x5fc)==UINT32_MAX) return true;
        if(auto owner=target(object,0x5fc)) field<Vec>(object,0x600)=field<Vec>(owner,0x160);
        position(object,field<Vec>(object,0x600));return true;
    }
    if(kind==Kind::Source || kind==Kind::Stretch) {
        if(auto owner=target(object,0x5f8)) {
            auto value=field<Vec>(owner,0x160);
            if(kind==Kind::Stretch) { value.y+=.1f;angle(object,-field<float>(owner,0x18)); }
            position(object,value);
        }
        return true;
    }
    if(kind==Kind::Collect || kind==Kind::Parts || kind==Kind::AllowParts || kind==Kind::Charge || kind==Kind::Cannon || kind==Kind::Mushmoot) {
        if(auto owner=target(object,0x5f8,true)) {
            Vec value;bool updated=false;
            if(kind==Kind::Collect) updated=handPosition(owner,10,true,value);
            else if(kind==Kind::Parts || kind==Kind::AllowParts) {
                const size_t slot=kind==Kind::Parts ? 0x688 : 0x684;
                updated=handPosition(owner,field<int>(object,slot)==0 ? 10 : 9,false,value);
            } else {
                const int event=kind==Kind::Charge ? 2 : kind==Kind::Cannon ? 1 : field<int>(object,0x684);
                updated=bonePosition(owner,event,value);
            }
            if(updated) position(object,value);
            if(kind==Kind::Charge || kind==Kind::Mushmoot) angle(object,-field<float>(owner,0x18));
        }
        return true;
    }
    if(kind==Kind::PrimaryYaw) {
        if(auto owner=target(object,0x5f8)) if(auto model=field<void *>(object,0x5f4))
            field<float>(model,0x13c)=180.f-field<float>(owner,0x18);
        return true;
    }
    if(kind==Kind::HotAir) {
        if(auto owner=target(object,0x5f8,true)) if(auto model=field<void *>(object,0x5f4)) {
            field<Vec>(model,0x12c)=field<Vec>(owner,0x160);
            field<Vec>(model,0x138)={0,-field<float>(owner,0x18),0};
        }
    }
    if(kind==Kind::Secondary || kind==Kind::SecondaryMover || kind==Kind::SecondaryPlusOne || kind==Kind::SecondaryScaled || kind==Kind::HotAir || kind==Kind::Wave) {
        if(auto owner=target(object,0x5fc,kind==Kind::SecondaryMover || kind==Kind::HotAir)) {
            auto value=field<Vec>(owner,0x160);
            if(kind==Kind::SecondaryPlusOne) value.y+=1;
            if(kind==Kind::SecondaryScaled) value.y+=.3f;
            if(finite(value)) field<Vec>(object,0x444)=value;
            if(kind==Kind::SecondaryScaled) field<Vec>(object,0x45c)={2,2,2};
            if(kind==Kind::Wave) { const float scale=field<float>(object,0x6a4);if(std::isfinite(scale)) field<Vec>(object,0x45c)={scale,scale,scale}; }
        }
        return true;
    }
    // Native-only v15 skill classes retain their verified source semantics.
    if(kind==Kind::Brandish || kind==Kind::CleanHit || kind==Kind::OverCutter) {
        auto source=target(object,0x5f8),destination=target(object,0x5fc);
        if(kind==Kind::CleanHit) { if(source) { position(object,field<Vec>(object,0x600));angle(object,180-field<float>(source,0x18)); } }
        else if(destination && (kind==Kind::Brandish || source)) {
            auto value=field<Vec>(destination,0x160);value.y+=1;position(object,value);
            angle(object,180-field<float>(kind==Kind::Brandish ? destination : source,0x18));
        }
        return true;
    }
    return true;
}
inline void refreshCallback(void *object) { refresh(object); }
inline void nativeAngle(void *object,float value) {
#if FENB_NATIVE_X86_ASM
    __asm {
        mov ecx,object
        movss xmm1,value
        call angleEntry
    }
#endif
}
inline bool install() {
    if(installed) return true;
#if !FENB_NATIVE_X86_ASM
    return false;
#else
    auto image=native_patch::image();native_binding::Image scan(image);
    if(!scan.bytes(0x2d7180,"55 8B EC 83 EC 08 53 8B 5D 08 0F B6 C3 35 C5 9D 1C 81") ||
       !scan.bytes(0x32de20,"55 8B EC 83 EC 10 53 56 8B F1 F3 0F 10 86 60 01 00 00") ||
       !scan.bytes(0x32e0e0,"0F 57 C0 F3 0F 5A C1 56 8B F1 83 BE 70 01 00 00 05") ||
       !scan.bytes(0x368c30,"55 8B EC 83 EC 54 A1 ?? ?? ?? ?? 33 C5 89 45 FC") ||
       !scan.bytes(0x368fb0,"55 8B EC 83 EC 58 A1 ?? ?? ?? ?? 33 C5 89 45 F8")) return false;
    bindings.clear();
    for(const auto &r:records) {
        auto **vtable=reinterpret_cast<void **>(scan.address(r.vtable));
        auto *locator=static_cast<unsigned char *>(vtable[-1]);
        if(vtable[6]!=scan.address(r.process) || !locator || field<void *>(locator,12)!=scan.address(r.type) ||
           std::strcmp(reinterpret_cast<const char *>(scan.address(r.type)+8),r.name)!=0) { bindings.clear();return false; }
        bindings.emplace(vtable,r.kind);
    }
    resolve=reinterpret_cast<Resolve>(scan.address(0x2d7180));ctrlMap=scan.address(0x86c12c);
    setPos=reinterpret_cast<SetPos>(scan.address(0x32de20));angleEntry=scan.address(0x32e0e0);setAngle=&nativeAngle;
    hand=reinterpret_cast<Hand>(scan.address(0x368fb0));force=reinterpret_cast<Force>(scan.address(0x368c30));
    boneRefresh=&native_animation::refreshBones;
    installed=true;Log("Native SFX: verified class-specific zero-tick attachment refresh installed.");return true;
#endif
}
inline void reset() {}
inline void uninstall() { installed=false;bindings.clear();resolve=nullptr;setPos=nullptr;setAngle=nullptr;hand=nullptr;force=nullptr;ctrlMap=angleEntry=nullptr;boneRefresh=&native_animation::refreshBones; }
}
