#pragma once
#include "native_binding.hpp"
#include "hybrid_time.hpp"
#include "native_patch.hpp"
#include "visual_transform.hpp"
#include <limits>

namespace native_mover_services {
using Vec=visual_transform::Vec;
using Service=void(__thiscall *)(void *);
using Particle=void(__thiscall *)(void *,Vec);
using Shoot=void(__thiscall *)(void *,void *,int);
inline Service originalScale=nullptr,originalEye=nullptr,originalAngel=nullptr,originalBalloon=nullptr;
inline Particle originalParticle=nullptr;
inline Shoot originalShoot=nullptr,originalMelee=nullptr;
inline uint32_t *processCounter=nullptr;
inline bool installed=false;
inline native_binding::Set patches;
inline float scaleDecay=.001f,sparkFade=.05f,companionDivisor=10.f,balloonTravel=.006f,balloonIdle=.002f;
template<class T> T &field(void *p,size_t offset) { return *reinterpret_cast<T *>(static_cast<unsigned char *>(p)+offset); }
inline bool onThread() { return installed && hybrid_time::frame.active && hybrid_time::frame.thread==GetCurrentThreadId(); }
inline bool continuous() { return onThread() && hybrid_time::frame.stage==hybrid_time::Stage::World; }
inline float ticks() { return std::isfinite(hybrid_time::frame.ticks) ? std::clamp(hybrid_time::frame.ticks,0.f,15.625f) : 0.f; }
struct ScopedFloat {
    float &value;float before;
    ScopedFloat(float &target,float next):value(target),before(target) { value=next; }
    ~ScopedFloat() { value=before; }
    ScopedFloat(const ScopedFloat&)=delete;ScopedFloat&operator=(const ScopedFloat&)=delete;
};
inline float retentionFor(float retention,float delta) { return std::pow(std::max(retention,0.f),std::max(delta,0.f)); }
inline float divisorFor(float delta) { return delta>0 ? 10.f/delta : std::numeric_limits<float>::infinity(); }
inline unsigned advanceCounter(unsigned before,unsigned count) { return before+count; }
inline void __fastcall scaleHook(void *mover,void *) {
    if(!originalScale) return;
    if(!continuous()) { originalScale(mover);return; }
    const float before=field<float>(mover,0xd48),amount=ticks();
    if(!std::isfinite(before) || before<=0) { originalScale(mover);return; }
    const float effective=retentionFor(before,amount);
    field<float>(mover,0xd48)=effective;
    ScopedFloat decay(scaleDecay,.001f*amount);
    originalScale(mover); // Native rank validation, SetScale and dirty matrix handling.
    // Preserve a native invalid-rank reset. The logical progress parameter has a
    // linear lifetime; its per-call retention above uses the whole time interval.
    if(field<float>(mover,0xd48)==effective-scaleDecay) field<float>(mover,0xd48)=before-.001f*amount;
}
inline void __fastcall eyeHook(void *mover,void *) {
    if(!originalEye) return;
    if(!continuous()) { originalEye(mover);return; }
    void *model=field<void *>(mover,0x178);
    const bool used=model && field<int>(model,0xd8)!=0;
    const unsigned before=used ? field<unsigned>(model,0xe0) : 0;
    ScopedFloat fade(sparkFade,.05f*ticks());
    originalEye(mover);
    if(used && field<void *>(mover,0x178)==model && field<unsigned>(model,0xe0)==before+1)
        field<unsigned>(model,0xe0)=advanceCounter(before,hybrid_time::frame.legacy);
}
inline void __fastcall angelHook(void *mover,void *) {
    if(!originalAngel) return;
    if(!continuous()) { originalAngel(mover);return; }
    ScopedFloat follow(companionDivisor,divisorFor(ticks()));
    originalAngel(mover);
}
inline void __fastcall balloonHook(void *mover,void *) {
    if(!originalBalloon) return;
    if(!continuous()) { originalBalloon(mover);return; }
    const float amount=ticks();
    ScopedFloat follow(companionDivisor,divisorFor(amount)),travel(balloonTravel,.006f*amount),idle(balloonIdle,.002f*amount);
    originalBalloon(mover);
}
inline void __fastcall particleHook(void *mover,void *,Vec position) {
    if(!originalParticle) return;
    if(!continuous()) { originalParticle(mover,position);return; }
    const unsigned count=hybrid_time::frame.legacy;
    hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1.f,1);
    for(unsigned i=0;i<count;++i) originalParticle(mover,position);
}
inline unsigned quarterSteps(uint32_t end,unsigned count) {
    unsigned result=0;
    for(unsigned i=0;i<count;++i) if(!((end-count+1+i)&3u)) ++result;
    return result;
}
inline void shootTicks(void *action,void *mover,void *parameter,Shoot emit) {
    if(!emit) return;
    auto &remaining=field<int>(action,0x90);
    const unsigned count=processCounter ? quarterSteps(*processCounter,continuous() ? hybrid_time::frame.legacy : 1u) : 0u;
    hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1.f,1);
    for(unsigned i=0;i<count && remaining>0;++i) { emit(mover,parameter,1);--remaining; }
}
inline void __fastcall shootTickHook(void *action,void *mover,void *model) { shootTicks(action,mover,model,originalShoot); }
inline void __fastcall meleeTickHook(void *action,void *mover,void *parameter) { shootTicks(action,mover,parameter,originalMelee); }
#if FENB_NATIVE_X86_ASM
__declspec(naked) inline void shootTickThunk() {
    __asm {
        pushad
        push ecx
        mov ecx,edi
        mov edx,esi
        call shootTickHook
        popad
        ret
    }
}
__declspec(naked) inline void meleeTickThunk() {
    __asm {
        pushad
        push ecx
        mov ecx,esi
        mov edx,eax
        call meleeTickHook
        popad
        ret
    }
}
__declspec(naked) inline void magicTickThunk() {
    __asm {
        pushad
        push ecx
        mov ecx,esi
        mov edx,edi
        call shootTickHook
        popad
        ret
    }
}
#endif
inline std::array<float,16> rotation(Vec angles) {
    // Matches D3DXMatrixRotationYawPitchRoll used by native CObj: negated yaw,
    // pitch and roll, row-vector matrices. Flying pitch retains the ±45 limit.
    constexpr float halfRadians=-3.14159265358979323846f/360.f;
    const float yaw=angles.x*halfRadians,pitch=std::clamp(angles.y,-45.f,45.f)*halfRadians,roll=angles.z*halfRadians;
    const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch),cr=std::cos(roll),sr=std::sin(roll);
    const auto q=visual_transform::normalize({cy*sp*cr+sy*cp*sr,sy*cp*cr-cy*sp*sr,cy*cp*sr-sy*sp*cr,cy*cp*cr+sy*sp*sr});
    return visual_transform::compose({{},{1,1,1},q});
}
inline std::array<float,16> multiply(const float *a,const float *b) {
    std::array<float,16> result{};
    for(size_t row=0;row<4;++row) for(size_t column=0;column<4;++column)
        for(size_t k=0;k<4;++k) result[row*4+column]+=a[row*4+k]*b[k*4+column];
    return result;
}
inline void afterProcess(void *mover) {
    if(!continuous() || !mover || field<DWORD>(mover,0x170)!=5 || (field<DWORD>(mover,4)&1u)) return;
    const Vec angles=field<Vec>(mover,0x18);
    const Vec position=field<Vec>(mover,0x160);
    if(!std::isfinite(angles.x) || !std::isfinite(angles.y) || !std::isfinite(angles.z) ||
       !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) return;
    const auto matrix=rotation(angles);
    if(std::memcmp(static_cast<unsigned char *>(mover)+0xa4,matrix.data(),64)!=0) {
        std::memcpy(static_cast<unsigned char *>(mover)+0xa4,matrix.data(),64);
        field<DWORD>(mover,4)|=8u;
    }
    // Native SetPos deliberately skips the mover's translation matrix. The
    // source Interpolate path which used to fill it is absent from this build.
    const std::array<float,16> translation={1,0,0,0,0,1,0,0,0,0,1,0,position.x,position.y,position.z,1};
    if(std::memcmp(static_cast<unsigned char *>(mover)+0x24,translation.data(),64)!=0) {
        std::memcpy(static_cast<unsigned char *>(mover)+0x24,translation.data(),64);field<DWORD>(mover,4)|=8u;
    }
    const auto *scale=reinterpret_cast<const float *>(static_cast<unsigned char *>(mover)+0x64);
    for(size_t i=0;i<16;++i) if(!std::isfinite(scale[i])) return;
    const auto scaled=multiply(scale,matrix.data()),world=multiply(scaled.data(),translation.data());
    // Bone-bound SFX can read the owner's world matrix before Render. Publish
    // the same S*R*T now; retain dirty8 for the native OBB/bounds/plane rebuild.
    if(std::memcmp(static_cast<unsigned char *>(mover)+0xe4,world.data(),64)!=0) {
        std::memcpy(static_cast<unsigned char *>(mover)+0xe4,world.data(),64);
        field<DWORD>(mover,4)|=8u;
    }
}
inline void uninstall() {
    installed=false;patches.clear();originalScale=nullptr;originalEye=nullptr;originalAngel=nullptr;originalBalloon=nullptr;originalParticle=nullptr;originalShoot=nullptr;originalMelee=nullptr;processCounter=nullptr;
    scaleDecay=.001f;sparkFade=.05f;companionDivisor=10.f;balloonTravel=.006f;balloonIdle=.002f;
}
inline bool install() {
    if(installed) return true;
#if !FENB_NATIVE_X86_ASM
    return false;
#else
    HMODULE module=native_patch::image();native_binding::Image scan(module);
    if(!scan.bytes(0x309330,"55 8B EC 83 E4 C0 83 EC 3C 0F 57 C0 56 8B F1 F3 0F 10 8E 48 0D 00 00") ||
       !scan.bytes(0x309670,"55 8B EC 83 E4 F8 51 53 56 57 8B F1") ||
       !scan.bytes(0x30a7e0,"55 8B EC 83 E4 F0 81 EC AC 00 00 00") ||
       !scan.bytes(0x30aa60,"55 8B EC 83 E4 F0 81 EC F8 00 00 00") ||
       !scan.bytes(0x310320,"55 8B EC 83 E4 F8 83 EC 38 A1 ?? ?? ?? ?? 33 C4") ||
       !scan.bytes(0x313c90,"55 8B EC 83 EC 2C A1 ?? ?? ?? ?? 33 C5 89 45 FC 56 8B F1 57") ||
       !scan.bytes(0x3132b0,"55 8B EC")) return false;
    auto operand=[&](DWORD rva,const char *opcode,DWORD constant,float *replacement) {
        return scan.bytes(rva,opcode) && scan.relative(scan.value(rva,4))==constant &&
            patches.pointer(reinterpret_cast<void **>(scan.address(rva)+4),scan.address(constant),replacement);
    };
    auto call=[&](DWORD rva,DWORD target,void *replacement) {
        unsigned char expected[5]={0xe8};const uint32_t displacement=target-rva-5;
        std::memcpy(expected+1,&displacement,4);return patches.call(scan.address(rva),expected,replacement);
    };
    originalScale=reinterpret_cast<Service>(scan.address(0x309330));originalEye=reinterpret_cast<Service>(scan.address(0x309670));
    originalAngel=reinterpret_cast<Service>(scan.address(0x30a7e0));originalBalloon=reinterpret_cast<Service>(scan.address(0x30aa60));
    originalParticle=reinterpret_cast<Particle>(scan.address(0x310320));
    originalShoot=reinterpret_cast<Shoot>(scan.address(0x313c90));
    originalMelee=reinterpret_cast<Shoot>(scan.address(0x3132b0));processCounter=reinterpret_cast<uint32_t *>(scan.address(0x841d04));
    // This span is only the timed continuation after a native hit event. Keep
    // the separate initial-hit emission and its new shoot-count assignment.
    const unsigned char shootOld[]={0x6a,1,0x51,0x8b,0xce,0xe8,0xb8,0x2f,0xf4,0xff,0xff,0x8f,0x90,0,0,0};
    const unsigned char meleeOld[]={0x6a,1,0x51,0x8b,0xc8,0xe8,0x1a,0x22,0xf4,0xff,0xff,0x8e,0x90,0,0,0};
    const unsigned char magicOld[]={0x6a,1,0x51,0x8b,0xcf,0xe8,0x24,0x29,0xf4,0xff,0xff,0x8e,0x90,0,0,0};
    auto timedSpan=[&](DWORD rva,const unsigned char *expected,void *target) {
        unsigned char code[16];std::fill(std::begin(code),std::end(code),static_cast<unsigned char>(0x90));code[0]=0xe8;
        const uint32_t relative=uint32_t(reinterpret_cast<uintptr_t>(target)-reinterpret_cast<uintptr_t>(scan.address(rva))-5);
        std::memcpy(code+1,&relative,4);return patches.add(scan.address(rva),expected,code,sizeof(code));
    };
    auto quarterGate=[&](DWORD rva,unsigned char jump) {
        unsigned char expected[]={0xf6,0x05,0,0,0,0,3,0x75,jump},code[sizeof(expected)];
        const uint32_t counter=uint32_t(reinterpret_cast<uintptr_t>(processCounter));std::memcpy(expected+2,&counter,4);
        std::fill(std::begin(code),std::end(code),static_cast<unsigned char>(0x90));
        return patches.add(scan.address(rva),expected,code,sizeof(code));
    };
    const bool prepared=operand(0x30946e,"F3 0F 5C 05",0x7b9c80,&scaleDecay) &&
        operand(0x3096ce,"F3 0F 5C 0D",0x7b9cd8,&sparkFade) &&
        operand(0x30a98a,"F3 0F 5E 0D",0x7b9f94,&companionDivisor) &&
        operand(0x30abf1,"F3 0F 5E 0D",0x7b9f94,&companionDivisor) &&
        operand(0x30ac8e,"F3 0F 58 0D",0x7b9ca0,&balloonTravel) &&
        operand(0x30aca2,"F3 0F 5C 0D",0x7b9ca0,&balloonTravel) &&
        operand(0x30ad2c,"F3 0F 58 0D",0x7b9c88,&balloonIdle) &&
        operand(0x30ad55,"F3 0F 5C 0D",0x7b9c88,&balloonIdle) &&
        call(0x30991b,0x309330,reinterpret_cast<void *>(&scaleHook)) &&
        call(0x30a78b,0x309670,reinterpret_cast<void *>(&eyeHook)) &&
        call(0x30a62b,0x30a7e0,reinterpret_cast<void *>(&angelHook)) &&
        call(0x30a5d6,0x30aa60,reinterpret_cast<void *>(&balloonHook)) &&
        call(0x30aa3a,0x310320,reinterpret_cast<void *>(&particleHook)) &&
        quarterGate(0x3d0cbc,0x1c) && quarterGate(0x3d107a,0x19) && quarterGate(0x3d1350,0x19) &&
        timedSpan(0x3d0cce,shootOld,reinterpret_cast<void *>(&shootTickThunk)) &&
        timedSpan(0x3d108c,meleeOld,reinterpret_cast<void *>(&meleeTickThunk)) &&
        timedSpan(0x3d1362,magicOld,reinterpret_cast<void *>(&magicTickThunk));
    if(!prepared || !patches.apply()) { uninstall();return false; }
    installed=true;return true;
#endif
}
}
