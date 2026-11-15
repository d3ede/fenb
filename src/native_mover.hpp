#pragma once
#include "native_animation.hpp"
#include "native_patch.hpp"
#include "visual_transform.hpp"

// Authoritative ActionMover integration. Velocities retain the client's native
// units; only displacement/acceleration and legacy counter consumption use time.
namespace native_mover {
using visual_transform::Vec;
template<class T> T &field(void *p,size_t offset) { return native_animation::field<T>(p,offset); }
using Action=int(__thiscall *)(void *,const Vec *);
using State=void(__thiscall *)(void *,DWORD,float);
using Message=int(__thiscall *)(void *,int,int,int,int,int,int);
using Tracking=void(__thiscall *)(void *);
using Speed=float(*)(void *,float);
using Particle=void(*)(void *,float);
inline bool installed=false;
inline void **playerSlot=nullptr;
inline int *speedIndicator=nullptr;
inline void *sendEntry=nullptr,*networkClient=nullptr;
inline Action originalAction=nullptr;
inline State originalState=nullptr;
inline Tracking tracking=nullptr;
inline Speed getSpeed=nullptr;
inline Particle particles=nullptr;
inline void *speedEntry=nullptr,*particleEntry=nullptr;
inline native_binding::Set patches;
inline bool active() { return installed && hybrid_time::frame.active && hybrid_time::frame.thread==GetCurrentThreadId() &&
    hybrid_time::frame.stage==hybrid_time::Stage::World; }
inline float ticks() { return std::isfinite(hybrid_time::frame.ticks) ? std::clamp(hybrid_time::frame.ticks,0.f,15.625f) : 0.f; }
inline DWORD state(void *action) { return field<DWORD>(action,4); }
inline bool flying(void *action) { return (field<DWORD>(action,8)&8)!=0; }
inline int message(void *action,int id) {
    auto table=field<void **>(action,0);
    return reinterpret_cast<Message>(table[1])(action,id,0,0,0,0,0);
}
inline float wrap(float angle) { float value=std::fmod(angle,360.f);return value<0 ? value+360.f : value; }
inline Vec normalized(Vec v) { const float length=std::sqrt(visual_transform::dot(v,v));return length>1e-8f ? v*(1/length) : Vec{}; }
inline float nativeSpeed(void *mover,float speed) {
    float result=0;
#if FENB_NATIVE_X86_ASM
    // Verified optimized ABI: ECX=mover, XMM1=base speed, XMM0=result.
    __asm {
        mov ecx,mover
        movss xmm1,speed
        call speedEntry
        movss result,xmm0
    }
#endif
    return result;
}
inline void nativeParticles(void *action,float squaredSpeed) {
#if FENB_NATIVE_X86_ASM
    // The native helper reads squared speed from XMM1; no stack arguments.
    __asm {
        mov ecx,action
        movss xmm1,squaredSpeed
        call particleEntry
    }
#endif
}
inline void __fastcall stateHook(void *action,void *,DWORD mask,float speed) {
    if(!active()) { originalState(action,mask,speed);return; }
    auto *mover=field<void *>(action,0xc);
    if(!mover) return;
    auto *model=field<void *>(mover,0x178);
    const auto beforeAngles=field<Vec>(mover,0x18);
    const int pause=native_animation::capturePause(model);
    originalState(action,mask,speed);
    if(field<void *>(mover,0x178)==model) native_animation::settlePause(model,pause);
    // Native turn/look states contain fixed increments independent of fSpeed.
    if(mask&0xf00) {
        auto &angles=field<Vec>(mover,0x18);
        const float dt=ticks();
        angles.x=wrap(beforeAngles.x+std::remainder(angles.x-beforeAngles.x,360.f)*dt);
        angles.y=std::clamp(beforeAngles.y+(angles.y-beforeAngles.y)*dt,-45.f,45.f);
        field<DWORD>(mover,4)|=8;
    }
}
inline Vec integrateFlight(Vec &velocity,Vec acceleration,float dt,bool sharpTurn,bool accelerating,bool fast) {
    const float maximum=fast ? .45f : .3f;
    if(!accelerating || visual_transform::dot(velocity,velocity)>=maximum*maximum) acceleration={};
    const double drag=sharpTurn ? .985*.989 : .989;
    const double decay=std::pow(drag,double(dt));
    // Fractional powers of the native affine one-step update preserve both its
    // velocity and summed displacement, independent of render frequency.
    const double sum=drag*(1-decay)/(1-drag),equilibrium=.989/(1-drag);
    const Vec displacement=velocity*float(sum)+acceleration*float(equilibrium*(double(dt)-sum));
    velocity=velocity*float(decay)+acceleration*float(equilibrium*(1-decay));
    return displacement;
}
inline Vec flyMove(void *action,float dt) {
    auto &velocity=field<Vec>(action,0x50);
    const float squaredSpeed=visual_transform::dot(velocity,velocity);
    auto flags=field<DWORD>(action,8);
    if(squaredSpeed==0 && !(flags&0x10)) return {};
    auto *mover=field<void *>(action,0xc);
    float power=field<float>(action,0x4c);
    // Preserve native tracking decisions while scaling their fixed angular step.
    if(tracking) {
        const Vec before=field<Vec>(mover,0x18);tracking(action);
        auto &after=field<Vec>(mover,0x18);
        after.x=wrap(before.x+std::remainder(after.x-before.x,360.f)*dt);
        after.y=std::clamp(before.y+(after.y-before.y)*dt,-45.f,45.f);
    }
    if((flags&0x90)==0x90) {
        if(field<int>(mover,0x700)>0) power*=1.6f;
        else { field<int>(mover,0x700)=0;message(action,24); }
    }
    if(particles) for(unsigned i=0;i<hybrid_time::frame.legacy;++i) particles(action,squaredSpeed);
    const Vec angles=field<Vec>(mover,0x18);
    constexpr float radians=.017453292519943295f;
    const float yaw=angles.x*radians,pitch=angles.y*radians;
    Vec acceleration{std::sin(yaw)*std::cos(pitch)*power,-std::sin(pitch)*power,-std::cos(yaw)*std::cos(pitch)*power};
    bool sharp=false;
    if(power>0 && squaredSpeed>.01f)
        sharp=visual_transform::dot(normalized(velocity),normalized(acceleration))<.63331902f;
    if(sharp) acceleration=acceleration*2;
    const Vec displacement=integrateFlight(velocity,acceleration,dt,sharp,power>0,(flags&0x80)!=0);
    if(field<float>(action,0x4c)==0 && visual_transform::dot(velocity,velocity)<.00000004f) {
        velocity={};field<DWORD>(action,8)&=~0x10u;
    }
    if(playerSlot && *playerSlot==mover) {
        const float length=std::sqrt(visual_transform::dot(velocity,velocity));
        *speedIndicator=int(float(int(length*60.f*60.f*60.f))/200.f);
    }
    return displacement;
}
inline float integrateGravity(float &velocity,float dt) {
    constexpr double gravity=.005,terminal=-.5;
    const double before=velocity;
    const double falling=std::clamp((before-terminal)/gravity,0.0,double(dt));
    const double displacement=before*falling-gravity*.5*falling*(falling+1.0)+terminal*(double(dt)-falling);
    velocity=float(std::max(terminal,before-gravity*double(dt)));
    return float(std::max(terminal*double(dt),displacement));
}
inline void integrateAction(void *action,const Vec &position,float dt,unsigned legacy) {
    auto *mover=field<void *>(action,0xc);
    auto &velocity=field<Vec>(action,0x50),&external=field<Vec>(action,0x5c),&accumulated=field<Vec>(action,0x68);
    const bool fly=flying(action);
    const bool moving=velocity.x!=0 || velocity.z!=0 || external.x!=0 || external.y!=0 || external.z!=0;
    field<int>(action,0xb0)=moving ? 1 : 0;
    if(velocity.y!=0 || moving || std::fabs(position.y-field<Vec>(action,0x74).y)>.001f) field<int>(action,0x44)=0;
    Vec displacement{};
    if(!fly) {
        displacement={velocity.x,velocity.y*dt,velocity.z};
        if(!(field<DWORD>(mover,0x328)&0x10000) && (state(action)&0xf000000)!=0x9000000 && field<int>(action,0x44)!=1)
            displacement.y=integrateGravity(velocity.y,dt);
    } else displacement=flyMove(action,dt);
    const auto actionState=state(action);
    if(actionState&0x8000000) external={};
    // Ground State1 has already time-scaled horizontal movement via fSpeed.
    accumulated=accumulated+displacement;
    const float externalTime=std::min(dt,10.f);
    const float decay=std::pow(.9f,externalTime);
    accumulated=accumulated+external*((actionState&0x2000000) ? externalTime : (1-decay)/.1f);
    if(!(actionState&0x2000000)) {
        if(legacy && visual_transform::dot(external,external)<.01f) external={};
        else external=external*decay;
    }
    field<DWORD>(action,0x40)+=legacy;
}
inline int __fastcall actionHook(void *action,void *,const Vec *position) {
    if(!active()) return originalAction(action,position);
    if(!position) return 0;
    const float dt=ticks();
    if(dt<=0) return 1;
    auto *mover=field<void *>(action,0xc);
    if(!mover) return 0;
    if(!flying(action)) { field<float>(action,0x50)=0;field<float>(action,0x58)=0; }
    if(state(action)==0) {
        message(action,3);
        if(playerSlot && *playerSlot==mover && flying(action)) {
            using Send=void(__thiscall *)(void *);
            reinterpret_cast<Send>(sendEntry)(networkClient);
        }
    } else {
        const float speed=getSpeed ? getSpeed(mover,field<float>(action,0x48))*dt : 0;
        auto table=field<void **>(action,0);
        auto dispatch=reinterpret_cast<State>(table[2]);
        for(DWORD mask:{0xffu,0x300u,0xc00u,0xf000u,0xff0000u,0xf000000u,0xf0000000u}) {
            const DWORD current=state(action)&mask;
            if(current) dispatch(action,current,speed);
        }
    }
    integrateAction(action,*position,dt,hybrid_time::frame.legacy);
    return 1;
}
inline bool install() {
#if !FENB_NATIVE_X86_ASM
    return false;
#else
    if(installed) return true;
    auto image=native_patch::image();native_binding::Image scan(image);
    if(!scan.bytes(0x3cc540,"55 8B EC 83 EC 08 56 8B F1 57 F6 46 08 08") ||
       !scan.calls(0x309ad8,0x3cc540) ||
       !scan.bytes(0x3cc8a0,"55 8B EC F6 41 08 08 FF 75 08 8B 41 0C 50 74 09") ||
       !scan.bytes(0x323ee0,"55 8B EC") || !scan.bytes(0x3d2b00,"53 8B DC 83 EC 08 83 E4 F8")) return false;
    auto **table=reinterpret_cast<void **>(scan.address(0x7b9bbc));
    if(table[0]!=scan.address(0x3cc100) || table[1]!=scan.address(0x3cc860) || table[2]!=scan.address(0x3cc8a0)) return false;
    const unsigned char actionCall[]={0xe8,0x63,0x2a,0x0c,0};
    patches.clear();
    if(!patches.call(scan.address(0x309ad8),actionCall,reinterpret_cast<void *>(&actionHook)) ||
       !patches.pointer(table+2,table[2],reinterpret_cast<void *>(&stateHook))) { patches.clear();return false; }
    originalAction=reinterpret_cast<Action>(scan.address(0x3cc540));originalState=reinterpret_cast<State>(scan.address(0x3cc8a0));
    speedEntry=scan.address(0x323ee0);particleEntry=scan.address(0x3d2610);
    tracking=reinterpret_cast<Tracking>(scan.address(0x3d2b00));getSpeed=&nativeSpeed;particles=&nativeParticles;
    if(!patches.apply()) { patches.clear();return false; }
    playerSlot=reinterpret_cast<void **>(scan.address(0x842168));speedIndicator=reinterpret_cast<int *>(scan.address(0x841e38));
    sendEntry=scan.address(0x9bc70);networkClient=scan.address(0x868f90);installed=true;
    Log("Native hybrid mover: continuous action/gravity/flight integration and legacy counters installed.");return true;
#endif
}
inline void reset() {}
inline void uninstall() { installed=false;patches.clear();playerSlot=nullptr;speedIndicator=nullptr;sendEntry=networkClient=nullptr; }
}
