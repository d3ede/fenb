#pragma once
#include "native_animation.hpp"
#include "native_mover.hpp"
#include "native_mover_legacy.hpp"
#include "native_mover_services.hpp"
#include "native_world.hpp"
#include "native_camera.hpp"
#include "native_app.hpp"
#include "native_sfx_follow.hpp"

namespace native_hybrid {
inline bool installed=false;
inline hybrid_time::Clock clock;
inline uint64_t generation=0;
inline void reset() {
    ++generation;
    clock.reset();hybrid_time::frame.active=false;hybrid_time::frame.stage=hybrid_time::Stage::Idle;
    native_animation::reset();native_mover::reset();native_mover_legacy::reset();native_camera::reset();
    native_world::alphaHistory.clear();native_sfx_follow::reset();native_app::hasReceived=false;
}
inline void uninstall() {
    installed=false;hybrid_time::frame.active=false;
    native_app::uninstall();native_sfx_follow::uninstall();native_camera::uninstall();native_world::uninstall();
    native_mover_services::uninstall();native_mover_legacy::uninstall();native_mover::uninstall();native_animation::uninstall();
    reset();
}
inline void refreshSfx(void *object) { native_sfx_follow::refresh(object); }
inline bool install() {
    if(installed) return true;
    if(!native_animation::install()) { Log("Native hybrid unavailable: animation ABI guard.");uninstall();return false; }
    if(!native_mover::install()) { Log("Native hybrid unavailable: action ABI guard.");uninstall();return false; }
    if(!native_mover_legacy::install()) { Log("Native hybrid unavailable: legacy mover ABI guard.");uninstall();return false; }
    if(!native_mover_services::install()) { Log("Native hybrid unavailable: mover services ABI guard.");uninstall();return false; }
    if(!native_world::install()) { Log("Native hybrid unavailable: world ABI guard.");uninstall();return false; }
    if(!native_camera::install()) { Log("Native hybrid unavailable: camera/input ABI guard.");uninstall();return false; }
    if(!native_sfx_follow::install()) { Log("Native hybrid unavailable: SFX ABI guard.");uninstall();return false; }
    if(!native_app::install()) { Log("Native hybrid unavailable: application ABI guard.");uninstall();return false; }
    native_world::afterObject=&native_mover_services::afterProcess;
    native_world::sfxRefresh=&refreshSfx;
    reset();installed=true;
    Log("Native hybrid FPS: continuous authoritative world/action/animation/input/camera and integer legacy services enabled.");
    return true;
}
template<class F> void continuousSubsteps(const hybrid_time::Frame &frame,F &&world,DWORD *processCounter=nullptr) {
    if(!std::isfinite(frame.ticks) || frame.ticks<=0) return;
    const unsigned count=unsigned(std::ceil(frame.ticks));
    const double interval=double(frame.ticks)/count;
    const double start=std::clamp(double(frame.legacy)+double(frame.fraction)-double(frame.ticks),0.0,1.0);
    unsigned consumed=0;
    const DWORD finalCounter=processCounter ? *processCounter : 0;
    for(unsigned i=0;i<count;++i) {
        const unsigned due=i+1==count ? frame.legacy :
            std::min(frame.legacy,unsigned(std::max(0.0,std::floor(start+interval*(i+1)+1e-7))));
        hybrid_time::Scope step(hybrid_time::Stage::World,float(interval),due-consumed);
        if(processCounter) *processCounter=finalCounter-frame.legacy+due;
        world();consumed=due;
    }
    if(processCounter) *processCounter=finalCounter;
}
template<class Move,class Continue> HRESULT run(void *application,float *elapsed,int64_t now,int64_t frequency,Move move,Continue &&keepGoing) {
    hybrid_time::frame=clock.advance(now,frequency);
    hybrid_time::frame.thread=GetCurrentThreadId();
    const auto epoch=generation;
    auto alive=[&]() { return installed && generation==epoch && keepGoing(); };
    const auto frame=hybrid_time::frame;
    const float renderElapsed=*elapsed;
    HRESULT result=S_OK;
    native_app::updateClocks();native_app::receiveAll();
    for(unsigned i=0;i<frame.legacy && alive();++i) {
        hybrid_time::Scope legacy(hybrid_time::Stage::Legacy,1.f,1);
        *elapsed=.016f;result=move(application);
        if(FAILED(result)) break;
    }
    if(SUCCEEDED(result) && alive()) {
        native_app::preview();
        native_app::heldInput();
        continuousSubsteps(frame,[&]() { if(alive()) native_world::continuousWorld(); },native_app::processCounter);
        if(alive()) {
            hybrid_time::Scope input(hybrid_time::Stage::Input);
            native_camera::applyKeyboard(native_world::activeWndWorld());
            native_camera::processCurrent(native_world::getDevice());
        }
    }
    hybrid_time::frame.stage=hybrid_time::Stage::Idle;
    hybrid_time::frame.active=alive();
    *elapsed=renderElapsed;
    return result;
}
}
