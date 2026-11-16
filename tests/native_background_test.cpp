#include "native_background.hpp"
#include <cassert>
#include <cstdio>
void Log(const char *) {}
namespace {
alignas(16) unsigned char app[512]{};
unsigned environments=0,updates=0,renders=0,postRenders=0,messageCalls=0;
bool world=true,deviceLost=false,moveBeforeLoss=false,stoppedTimer=false;
HRESULT environmentResult=S_OK,moveResult=S_OK;
bool ready() { return world; }
HRESULT step(void *p) { assert(p==app);++updates;++native_background::moves;return moveResult; }
BOOL __thiscall canRender(void *p) { assert(p==app);return FALSE; }
HRESULT __thiscall environment(void *p) {
    assert(p==app);++environments;
    if(FAILED(environmentResult))return environmentResult;
    if((!deviceLost && !stoppedTimer) || moveBeforeLoss)step(p);
    if(!deviceLost && !stoppedTimer && native_background::canRenderHook(p,nullptr)) { ++renders;++postRenders; }
    return S_OK;
}
BOOL WINAPI getMessage(LPMSG m,HWND,UINT,UINT) { ++messageCalls;m->message=WM_APP+4;return TRUE; }
SHORT WINAPI keyState(int) { return SHORT(0x8001); }
BOOL WINAPI keyboardState(PBYTE keys) { if(!keys)return FALSE;std::memset(keys,0x80,256);return TRUE; }
}
int main() {
    using namespace native_background;
    application=app;installed=true;move=&step;native_background::ready=&::ready;thread=GetCurrentThreadId();
    field<unsigned char>(app,0xa8)=1;field<unsigned char>(app,0xa1)=0;
    originalEnvironment=&environment;originalCanRender=&canRender;originalGetMessage=&getMessage;
    LARGE_INTEGER f;QueryPerformanceFrequency(&f);frequency=f.QuadPart;
    assert(eligible());
    tick();assert(environments==1 && updates==1 && renders==1 && postRenders==1 && !pumping);
    assert(!canRenderHook(app,nullptr)); // Native inactive-window throttle is retained outside the opt-in update.
    deviceLost=true;field<unsigned char>(app,0xa2)=1;
    tick();assert(environments==2 && updates==2 && renders==1);
    moveBeforeLoss=true;tick();assert(environments==3 && updates==3);
    field<unsigned char>(app,0xa8)=0;tick();assert(environments==3);
    field<unsigned char>(app,0xa8)=1;world=false;tick();assert(environments==3);world=true;
    thread=GetCurrentThreadId()+1;tick();assert(environments==3);thread=GetCurrentThreadId();
    frequency=1000000;scheduled=true;last=100;
    assert(!due(100) && !due(16099) && due(16100) && due(99) && due(300000));
    assert(remaining100ns(100)==160000 && remaining100ns(16099)==10 && remaining100ns(16100)==0 && remaining100ns(16101)==0);
    QueryPerformanceFrequency(&f);frequency=f.QuadPart;resetSchedule();
    const auto window=CreateWindowExA(0,"STATIC","FENB disposable background test",WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    assert(window);field<HWND>(app,0xac)=window;
    originalAsyncKey=originalKey=&keyState;originalKeyboard=&keyboardState;
    BYTE keys[256]{};pumping=true;
    assert(asyncKeyHook('W')==0 && keyHook(VK_LBUTTON)==0 && keyboardHook(keys));
    for(auto key:keys)assert(key==0);
    pumping=false;assert(asyncKeyHook('W')==SHORT(0x8001) && keyHook(VK_LBUTTON)==SHORT(0x8001));
    assert(keyboardHook(keys) && keys['W']==0x80 && !keyboardHook(nullptr));
    MSG m{};PeekMessageA(&m,nullptr,0,0,PM_NOREMOVE);
    PostThreadMessageA(GetCurrentThreadId(),WM_APP+9,123,456);
    assert(getMessageHook(&m,nullptr,WM_APP+9,WM_APP+9));
    assert(m.message==WM_APP+9 && m.wParam==123 && m.lParam==456 && updates==4 && !messageCalls);
    PostQuitMessage(77);assert(!getMessageHook(&m,nullptr,0,0) && m.message==WM_QUIT && m.wParam==77);
    assert(updates==4 && field<unsigned char>(app,0xa1)==0 && field<unsigned char>(app,0xa8)==1);
    world=false;assert(getMessageHook(&m,nullptr,0,0) && messageCalls==1 && m.message==WM_APP+4);
    world=true;deviceLost=moveBeforeLoss=false;stoppedTimer=true;field<unsigned char>(app,0xa2)=0;
    assert(tick()==S_OK && updates==5 && renders==1);
    environmentResult=E_FAIL;assert(tick()==E_FAIL && updates==5 && !pumping);
    environmentResult=S_OK;moveResult=E_FAIL;assert(tick()==E_FAIL && updates==6 && !pumping);
    // A visible inactive native loop must not acquire a second frame limiter.
    stoppedTimer=false;moveResult=S_OK;const auto before=updates,beforeRender=renders;
    scheduled=true;last=counter();
    assert(environmentHook(app,nullptr)==S_OK && environmentHook(app,nullptr)==S_OK);
    assert(updates==before+2 && renders==beforeRender+2 && postRenders==renders);
    // Timer ownership survives an option/device release until the waiter exits.
    HANDLE handle=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_MODIFY_STATE|SYNCHRONIZE);
    if(!handle)handle=CreateWaitableTimerExW(nullptr,nullptr,0,TIMER_MODIFY_STATE|SYNCHRONIZE);
    assert(handle);std::atomic_store(&wakeTimer,std::make_shared<WakeTimer>(handle));
    auto held=std::atomic_load(&wakeTimer);DWORD handleFlags=0;
    uninstall();assert(GetHandleInformation(handle,&handleFlags));held.reset();
    assert(!std::atomic_load(&wakeTimer));
    DestroyWindow(window);uninstall();
    puts("Background loop: full render/post-render lifecycle, no extra visible-window cap, native lost-device recovery, input filtering, precise deadlines, timer lifetime and real messages passed.");
}
