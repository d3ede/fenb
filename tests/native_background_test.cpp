#include "native_background.hpp"
#include <cassert>
#include <cstdio>
void Log(const char *) {}
namespace {
alignas(16) unsigned char app[512]{};
unsigned environments=0,updates=0,renders=0,messageCalls=0;
bool world=true,deviceLost=false,moveBeforeLoss=false,stoppedTimer=false;
HRESULT environmentResult=S_OK,moveResult=S_OK;
bool ready() { return world; }
HRESULT step(void *p) { assert(p==app);++updates;++native_background::moves;return moveResult; }
BOOL __thiscall canRender(void *p) { assert(p==app);++renders;return TRUE; }
HRESULT __thiscall environment(void *p) {
    assert(p==app);++environments;
    if(FAILED(environmentResult))return environmentResult;
    if((!deviceLost && !stoppedTimer) || moveBeforeLoss)step(p);
    if(!deviceLost)native_background::canRenderHook(p,nullptr);
    return S_OK;
}
BOOL WINAPI getMessage(LPMSG m,HWND,UINT,UINT) { ++messageCalls;m->message=WM_APP+4;return TRUE; }
}
int main() {
    using namespace native_background;
    application=app;installed=true;move=&step;native_background::ready=&::ready;thread=GetCurrentThreadId();
    field<unsigned char>(app,0xa8)=1;field<unsigned char>(app,0xa1)=0;
    originalEnvironment=&environment;originalCanRender=&canRender;originalGetMessage=&getMessage;
    LARGE_INTEGER f;QueryPerformanceFrequency(&f);frequency=f.QuadPart;
    assert(eligible());
    tick();assert(environments==1 && updates==1 && renders==0 && !pumping);
    assert(canRenderHook(app,nullptr) && renders==1);
    deviceLost=true;field<unsigned char>(app,0xa2)=1;
    tick();assert(environments==2 && updates==2 && renders==1);
    moveBeforeLoss=true;tick();assert(environments==3 && updates==3);
    field<unsigned char>(app,0xa8)=0;tick();assert(environments==3);
    field<unsigned char>(app,0xa8)=1;world=false;tick();assert(environments==3);world=true;
    thread=GetCurrentThreadId()+1;tick();assert(environments==3);thread=GetCurrentThreadId();
    frequency=1000000;scheduled=true;last=100;
    assert(!due(100) && !due(16099) && due(16100) && due(99) && due(300000));
    QueryPerformanceFrequency(&f);frequency=f.QuadPart;resetSchedule();
    const auto window=CreateWindowExA(0,"STATIC","FENB disposable background test",WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    assert(window);field<HWND>(app,0xac)=window;
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
    DestroyWindow(window);uninstall();
    puts("Background loop: native updates without rendering, lost-device fallback, no duplicate updates, pause/world/thread guards, cadence and real message/quit delivery passed.");
}
