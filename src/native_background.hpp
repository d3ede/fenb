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
    return DWORD(std::clamp<int64_t>((left*1000+frequency-1)/frequency,1,16));
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
        waitMessages(remaining()); // Wake for real messages; never fabricate window/focus/input events.
    }
    resetSchedule();return original(message,window,first,end);
}
inline BOOL __fastcall canRenderHook(void *app,void *) {
    return pumping && app==application ? FALSE : originalCanRender(app);
}
inline HRESULT __fastcall environmentHook(void *app,void *) {
    if(app!=application || !eligible())return originalEnvironment(app);
    if(background()) {
        const auto now=counter();HRESULT result=S_OK;
        if(due(now)) { scheduled=true;last=now;result=tick(); }
        waitMessages(remaining());return result;
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
    if(!scan.bytes(0xeba92,"80 BE A1 00 00 00 00 8D 44 24 0C 74 0D") ||
       !scan.bytes(0xebab3,"FF 15 ?? ?? ?? ?? 85 C0 0F 95 C0 84 C0") ||
       !scan.bytes(0xebb04,"FF 15 ?? ?? ?? ?? 80 BE A1 00 00 00 00") ||
       !scan.bytes(0xebe20,"55 8B EC 83 EC 14 53 56 57 8B F1 E8") ||
       !scan.bytes(0xebe54,"80 BE A2 00 00 00 00 0F 84") ||
       !scan.bytes(0xec118,"8B 06 8B CE F3 0F 10 45 FC F3 0F 11 86 C0 00 00 00") ||
       !scan.bytes(0xec250,"83 3D ?? ?? ?? ?? 00 57 8B F9") ||
       !scan.calls(0xebb15,0xebe20) || !scan.calls(0xec186,0xec250))return false;
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
    const unsigned char environmentCall[5]={0xe8,6,3,0,0},renderCall[5]={0xe8,0xc5,0,0,0};
    if(!replaceImport(scan,0xebab3,0x6d87e4,reinterpret_cast<void *>(&getMessageHook)) ||
       !replaceImport(scan,0xebb04,0x6d8368,reinterpret_cast<void *>(&sleepHook)) ||
       !patches.call(scan.address(0xebb15),environmentCall,reinterpret_cast<void *>(&environmentHook)) ||
       !patches.call(scan.address(0xec186),renderCall,reinterpret_cast<void *>(&canRenderHook)) ||
       !patches.pointer(asyncKey,*asyncKey,reinterpret_cast<void *>(&asyncKeyHook)) ||
       !patches.pointer(key,*key,reinterpret_cast<void *>(&keyHook)) ||
       !patches.pointer(keyboard,*keyboard,reinterpret_cast<void *>(&keyboardHook)) || !patches.apply()) {
        patches.clear();return false;
    }
    LARGE_INTEGER value;QueryPerformanceFrequency(&value);frequency=value.QuadPart;
    resetSchedule();installed=true;
    Log("Native background loop: inactive message wait and lost-device CPU updates installed; native window/input state retained.");return true;
}
inline void uninstall() {
    installed=false;patches.clear();application=nullptr;move=nullptr;ready=nullptr;playerSlot=nullptr;thread=0;
    pumping=false;resetSchedule();
}
}
