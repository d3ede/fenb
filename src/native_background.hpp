#pragma once
#include "native_binding.hpp"

// Run the existing game loop on its window thread while the inactive message
// wait or a lost DX9 device would otherwise prevent FrameMove from running.
namespace native_background {
template<class T> T &field(void *p,size_t at) { return *reinterpret_cast<T *>(static_cast<char *>(p)+at); }
using Environment=HRESULT(__thiscall *)(void *);
using CanRender=BOOL(__thiscall *)(void *);
using GetMessage=BOOL(WINAPI *)(LPMSG,HWND,UINT,UINT);
using Move=HRESULT(*)(void *);
using KeyState=SHORT(WINAPI *)(int);
using KeyboardState=BOOL(WINAPI *)(PBYTE);
inline Environment originalEnvironment=nullptr;
inline CanRender originalCanRender=nullptr;
inline GetMessage originalGetMessage=nullptr;
inline KeyState originalAsyncKey=nullptr,originalKey=nullptr;
inline KeyboardState originalKeyboard=nullptr;
inline native_binding::Set patches;
inline void *application=nullptr;
inline DWORD thread=0;
inline Move move=nullptr;
inline void **playerSlot=nullptr;
inline bool (*ready)()=nullptr;
inline bool installed=false,pumping=false,scheduled=false;
inline uint64_t moves=0;
inline int64_t last=0,frequency=0;
struct WakeTimer {
    HANDLE handle=nullptr;
    explicit WakeTimer(HANDLE value):handle(value) {}
    ~WakeTimer() { if(handle)CloseHandle(handle); }
};
inline std::shared_ptr<WakeTimer> wakeTimer;
inline int64_t counter() { LARGE_INTEGER n;QueryPerformanceCounter(&n);return n.QuadPart; }
inline bool eligible() {
    return installed && application && thread==GetCurrentThreadId() &&
        move && ready && ready() && field<unsigned char>(application,0xa8)!=0;
}
inline bool background() {
    const HWND window=field<HWND>(application,0xac);
    return window && (IsIconic(window) || GetForegroundWindow()!=window);
}
inline bool ignoreInput() { return pumping && application && background(); }
inline SHORT WINAPI asyncKeyHook(int key) { return ignoreInput()?0:originalAsyncKey(key); }
inline SHORT WINAPI keyHook(int key) { return ignoreInput()?0:originalKey(key); }
inline BOOL WINAPI keyboardHook(PBYTE keys) {
    if(ignoreInput() && keys) { std::memset(keys,0,256);return TRUE; }
    return originalKeyboard(keys);
}
inline void resetSchedule() { scheduled=false;last=0; }
inline bool due(int64_t now) {
    if(!scheduled || now<last || now-last>frequency/4)return true;
    return now-last>=std::max<int64_t>(1,frequency*16/1000);
}
inline HRESULT tick() {
    if(!eligible())return S_OK;
    const auto before=moves;const bool prior=pumping;pumping=true;
    HRESULT result=originalEnvironment(application);
    // A stopped focus-pause timer or DEVICELOST can skip normal FrameMove.
    // Keep the CPU world alive without bypassing device recovery or advancing twice.
    if(SUCCEEDED(result) && eligible() && moves==before)result=move(application);
    pumping=prior;return result;
}
inline void waitMessages(DWORD milliseconds) {
    MsgWaitForMultipleObjectsEx(0,nullptr,milliseconds,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
}
inline DWORD remaining() {
    if(!scheduled || frequency<=0)return 1;
    const int64_t left=std::max<int64_t>(0,frequency*16/1000-(counter()-last));
    return DWORD(std::clamp<int64_t>((left*1000+frequency-1)/frequency,0,16));
}
inline int64_t remaining100ns(int64_t now) {
    if(!scheduled || frequency<=0 || now<last)return 0;
    const int64_t left=std::max<int64_t>(0,frequency*16/1000-(now-last));
    return (left*10000000+frequency-1)/frequency;
}
inline void waitNext() {
    const auto left=remaining100ns(counter());if(!left)return;
    const auto timer=std::atomic_load(&wakeTimer);
    LARGE_INTEGER deadline;deadline.QuadPart=-left;
    if(timer && SetWaitableTimer(timer->handle,&deadline,0,nullptr,nullptr,FALSE)) {
        // Keep the handle alive if another device releases its background request.
        MsgWaitForMultipleObjectsEx(1,&timer->handle,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    } else waitMessages(remaining());
}
inline BOOL WINAPI getMessageHook(LPMSG message,HWND window,UINT first,UINT end) {
    const auto original=originalGetMessage;
    while(eligible() && background()) {
        const bool queued=PeekMessageA(message,window,first,end,PM_REMOVE)!=FALSE;
        if(queued && message->message==WM_QUIT)return FALSE;
        const auto now=counter();
        HRESULT result=S_OK;
        if(due(now)) { scheduled=true;last=now;result=tick(); }
        if(queued)return TRUE;
        if(FAILED(result))break;
        if(!eligible() || !background())break;
        waitNext(); // Wake for real messages; never fabricate window/focus/input events.
    }
    resetSchedule();return original(message,window,first,end);
}
inline BOOL __fastcall canRenderHook(void *app,void *) {
    // An inactive visible window still needs the complete native Render/Present
    // lifecycle. Device-loss handling remains in the original environment call.
    return pumping && app==application ? TRUE : originalCanRender(app);
}
inline HRESULT __fastcall environmentHook(void *app,void *) {
    if(app!=application || !eligible())return originalEnvironment(app);
    if(background()) {
        // This native loop is already running: keep its FPS cap/uncap and all
        // post-render services. Only the blocking inactive message loop is paced.
        resetSchedule();return tick();
    }
    resetSchedule();const auto before=moves;
    HRESULT result=originalEnvironment(app);
    if(SUCCEEDED(result) && eligible() && moves==before && field<unsigned char>(app,0xa2)) {
        const bool prior=pumping;pumping=true;result=move(app);pumping=prior;
    }
    return result;
}
inline void WINAPI sleepHook(DWORD duration) { Sleep(eligible() ? std::min(duration,DWORD(16)) : duration); }
inline bool replaceImport(native_binding::Image &scan,DWORD reference,DWORD import,void *hook) {
    auto *site=scan.address(reference),*slot=scan.address(import);
    if(!site || !slot)return false;
    unsigned char before[6]={0xff,0x15},after[6]={0xe8,0,0,0,0,0x90};
    const uint32_t address=uint32_t(reinterpret_cast<uintptr_t>(slot));std::memcpy(before+2,&address,4);
    const uint32_t displacement=uint32_t(reinterpret_cast<uintptr_t>(hook)-reinterpret_cast<uintptr_t>(site)-5);
    std::memcpy(after+1,&displacement,4);return patches.add(site,before,after,6);
}
inline bool install() {
    if(installed)return true;
    native_binding::Image scan(native_patch::image());
    // The shipped client inlines the framework Run loop in WinMain. Its three
    // call sites are distinct from the otherwise equivalent member-function
    // loop below; guarding/patching only that member leaves the real loop native.
    if(!scan.bytes(0xe2f40,"80 3D ?? ?? ?? ?? 00 8D 44 24 14 74 0D") ||
       !scan.bytes(0xe2f61,"FF 15 ?? ?? ?? ?? 85 C0 0F 95 C0 84 C0") ||
       !scan.bytes(0xe2fa6,"80 3D ?? ?? ?? ?? 00 74 08 6A 64") ||
       !scan.bytes(0xe2fb1,"FF 15 ?? ?? ?? ?? 80 3D ?? ?? ?? ?? 00") ||
       !scan.bytes(0xe2fc0,"B9 ?? ?? ?? ?? E8") ||
       !scan.bytes(0xeba92,"80 BE A1 00 00 00 00 8D 44 24 0C 74 0D") ||
       !scan.bytes(0xebab3,"FF 15 ?? ?? ?? ?? 85 C0 0F 95 C0 84 C0") ||
       !scan.bytes(0xebb04,"FF 15 ?? ?? ?? ?? 80 BE A1 00 00 00 00") ||
       !scan.bytes(0xebe20,"55 8B EC 83 EC 14 53 56 57 8B F1 E8") ||
       !scan.bytes(0xebe54,"80 BE A2 00 00 00 00 0F 84") ||
       !scan.bytes(0xec118,"8B 06 8B CE F3 0F 10 45 FC F3 0F 11 86 C0 00 00 00") ||
       !scan.bytes(0xec250,"83 3D ?? ?? ?? ?? 00 57 8B F9") ||
       !scan.calls(0xe2fc5,0xebe20) || !scan.calls(0xebb15,0xebe20) ||
       !scan.calls(0xec186,0xec250))return false;
    const DWORD entryApplication=scan.value(0xe2fc0,1);
    if(scan.value(0xe2f40,2)!=entryApplication+0xa1 ||
       scan.value(0xe2fa6,2)!=entryApplication+0xa2)return false;
    originalEnvironment=reinterpret_cast<Environment>(scan.address(0xebe20));
    originalCanRender=reinterpret_cast<CanRender>(scan.address(0xec250));
    playerSlot=reinterpret_cast<void **>(scan.address(0x842168));if(!playerSlot)return false;
    auto *getMessage=scan.address(0x6d87e4);if(!getMessage)return false;
    std::memcpy(&originalGetMessage,getMessage,sizeof(originalGetMessage));
    auto **asyncKey=reinterpret_cast<void **>(scan.address(0x6d881c));
    auto **key=reinterpret_cast<void **>(scan.address(0x6d8b4c));
    auto **keyboard=reinterpret_cast<void **>(scan.address(0x6d8a90));
    if(!asyncKey || !key || !keyboard)return false;
    originalAsyncKey=reinterpret_cast<KeyState>(*asyncKey);originalKey=reinterpret_cast<KeyState>(*key);
    originalKeyboard=reinterpret_cast<KeyboardState>(*keyboard);
    patches.clear();
    const unsigned char entryEnvironmentCall[5]={0xe8,0x56,0x8e,0,0};
    const unsigned char environmentCall[5]={0xe8,6,3,0,0},renderCall[5]={0xe8,0xc5,0,0,0};
    if(!replaceImport(scan,0xe2f61,0x6d87e4,reinterpret_cast<void *>(&getMessageHook)) ||
       !replaceImport(scan,0xe2fb1,0x6d8368,reinterpret_cast<void *>(&sleepHook)) ||
       !patches.call(scan.address(0xe2fc5),entryEnvironmentCall,reinterpret_cast<void *>(&environmentHook)) ||
       !replaceImport(scan,0xebab3,0x6d87e4,reinterpret_cast<void *>(&getMessageHook)) ||
       !replaceImport(scan,0xebb04,0x6d8368,reinterpret_cast<void *>(&sleepHook)) ||
       !patches.call(scan.address(0xebb15),environmentCall,reinterpret_cast<void *>(&environmentHook)) ||
       !patches.call(scan.address(0xec186),renderCall,reinterpret_cast<void *>(&canRenderHook)) ||
       !patches.pointer(asyncKey,*asyncKey,reinterpret_cast<void *>(&asyncKeyHook)) ||
       !patches.pointer(key,*key,reinterpret_cast<void *>(&keyHook)) ||
       !patches.pointer(keyboard,*keyboard,reinterpret_cast<void *>(&keyboardHook)) || !patches.apply()) {
        patches.clear();return false;
    }
    LARGE_INTEGER value;QueryPerformanceFrequency(&value);frequency=value.QuadPart;
    HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_MODIFY_STATE|SYNCHRONIZE);
    if(!timer)timer=CreateWaitableTimerExW(nullptr,nullptr,0,TIMER_MODIFY_STATE|SYNCHRONIZE);
    std::atomic_store(&wakeTimer,timer?std::make_shared<WakeTimer>(timer):std::shared_ptr<WakeTimer>{});
    resetSchedule();installed=true;
    Log("Native background loop: WinMain and framework loops connected; native render throttle bypass, deadline message wait and lost-device CPU updates installed.");return true;
}
inline void uninstall() {
    installed=false;patches.clear();application=nullptr;move=nullptr;ready=nullptr;playerSlot=nullptr;thread=0;
    std::atomic_store(&wakeTimer,std::shared_ptr<WakeTimer>{});
    pumping=false;resetSchedule();
}
}
