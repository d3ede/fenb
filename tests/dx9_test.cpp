#include "common.hpp"
#include "settings.hpp"
#include "state_guard.hpp"
#include "generated/shaders.hpp"
#include <gdiplus.h>
#include <cstdio>
#include <cstring>
#include <psapi.h>

namespace {
int failures=0;
bool Check(bool ok,const char* name) { printf("%s: %s\n",ok?"PASS":"FAIL",name); if(!ok)++failures; return ok; }
LRESULT CALLBACK Wnd(HWND h,UINT m,WPARAM w,LPARAM l) {
    if(m==WM_CLOSE){DestroyWindow(h);return 0;}
    if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,m,w,l);
}
std::wstring Directory() {
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);std::wstring p(path);
    return p.substr(0,p.find_last_of(L"\\/")+1);
}
void Pump() {MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
SIZE_T PrivateBytes() {
    PROCESS_MEMORY_COUNTERS_EX counters{};counters.cb=sizeof(counters);
    GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters));
    return counters.PrivateUsage;
}
DWORD Sample(IDirect3DDevice9* dev,IDirect3DSurface9* source,UINT x,UINT y) {
    D3DSURFACE_DESC d{};source->GetDesc(&d);Com<IDirect3DSurface9> copy;
    if(FAILED(dev->CreateOffscreenPlainSurface(d.Width,d.Height,d.Format,D3DPOOL_SYSTEMMEM,copy.out(),nullptr)))return 0;
    if(FAILED(dev->GetRenderTargetData(source,copy)))return 0;
    D3DLOCKED_RECT l{};if(FAILED(copy->LockRect(&l,nullptr,D3DLOCK_READONLY)))return 0;
    DWORD pixel=*reinterpret_cast<DWORD*>(static_cast<BYTE*>(l.pBits)+y*l.Pitch+x*4);
    copy->UnlockRect();return pixel&0xffffff;
}
void Hotkey(HWND window,bool right,bool repeat=false,bool release=true) {
    BYTE previous[256]{},keys[256]{};GetKeyboardState(previous);
    keys[VK_SHIFT]=0x80;keys[right?VK_RSHIFT:VK_LSHIFT]=0x80;keys[VK_RETURN]=0x80;
    SetKeyboardState(keys);
    SendMessageW(window,WM_KEYDOWN,VK_RETURN,1 | (repeat?(1LL<<30):0));
    if(release)SendMessageW(window,WM_KEYUP,VK_RETURN,static_cast<LPARAM>(0xC0000001u));
    SendMessageW(window,WM_CHAR,'\r',1);
    SetKeyboardState(previous);
}
void EffectsHotkey(HWND window) {
    BYTE previous[256]{},keys[256]{};GetKeyboardState(previous);
    keys[VK_SHIFT]=keys[VK_RSHIFT]=0x80;SetKeyboardState(keys);
    SendMessageW(window,WM_KEYDOWN,VK_F12,1);
    SendMessageW(window,WM_KEYUP,VK_F12,static_cast<LPARAM>(0xC0000001u));
    SetKeyboardState(previous);
}
bool Screenshot(IDirect3DDevice9* dev,IDirect3DSurface9* source,const wchar_t* path) {
    D3DSURFACE_DESC d{};source->GetDesc(&d);
    Com<IDirect3DSurface9> copy;
    if(FAILED(dev->CreateOffscreenPlainSurface(d.Width,d.Height,d.Format,D3DPOOL_SYSTEMMEM,copy.out(),nullptr)))return false;
    if(FAILED(dev->GetRenderTargetData(source,copy)))return false;
    D3DLOCKED_RECT lock{};if(FAILED(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY)))return false;
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;Gdiplus::GdiplusStartup(&token,&input,nullptr);
    UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);std::vector<unsigned char> data(size);
    auto* enc=reinterpret_cast<Gdiplus::ImageCodecInfo*>(data.data());Gdiplus::GetImageEncoders(count,size,enc);
    CLSID png{};for(UINT i=0;i<count;++i)if(wcscmp(enc[i].MimeType,L"image/png")==0)png=enc[i].Clsid;
    bool ok=false;
    {Gdiplus::Bitmap image(d.Width,d.Height,lock.Pitch,PixelFormat32bppRGB,(BYTE*)lock.pBits);ok=image.Save(path,&png,nullptr)==Gdiplus::Ok;}
    Gdiplus::GdiplusShutdown(token);copy->UnlockRect();return ok;
}
void Scene(IDirect3DDevice9* dev,IDirect3DSurface9* surface,UINT width,UINT height) {
    dev->ColorFill(surface,nullptr,D3DCOLOR_XRGB(35,39,57));
    // A repeatable stylized scene for checking bloom, color, and the overlay.
    for(int i=0;i<12;++i){
        RECT r{LONG(i*width/12),LONG(height*.60f-(i%4)*25),LONG((i+1)*width/12+2),LONG(height)};
        dev->ColorFill(surface,&r,D3DCOLOR_XRGB(38+i*4,44+i*3,65+i*5));
    }
    RECT moon{LONG(width*.77f),45,LONG(width*.77f+46),91};
    dev->ColorFill(surface,&moon,D3DCOLOR_XRGB(249,232,255));
    // A real world draw (fully clipped) distinguishes this frame from UI-only screens.
    StateGuard state(dev);
    if(state.valid()){
        state.isolateTargets();dev->SetRenderTarget(0,surface);
        if(SUCCEEDED(dev->BeginScene())){
            D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
            auto perspective=identity;perspective._33=perspective._34=1;perspective._43=-.1f;perspective._44=0;
            dev->SetTransform(D3DTS_WORLD,&identity);dev->SetTransform(D3DTS_VIEW,&identity);dev->SetTransform(D3DTS_PROJECTION,&perspective);
            dev->SetVertexShader(nullptr);dev->SetPixelShader(nullptr);dev->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
            dev->SetRenderState(D3DRS_LIGHTING,FALSE);dev->SetRenderState(D3DRS_ZENABLE,TRUE);dev->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
            struct V{float x,y,z;DWORD color;};V marker[]={{3,3,.5f,0xffffffff},{4,3,.5f,0xffffffff},{3,4,.5f,0xffffffff}};
            dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,marker,sizeof(V));dev->EndScene();
        }
    }
}
}
int main(int argc,char** argv) {
    bool interactive=argc>1 && strcmp(argv[1],"--preview")==0;
    bool aaTest=argc>1 && strcmp(argv[1],"--aa-test")==0;
    auto dir=Directory(); auto config=dir+L"FENB.ini";
    Settings saved;saved.load(config);
    Settings test; test.openOnStart=!aaTest;test.exposure=.12f;test.save(config);
    HMODULE dll=LoadLibraryW((dir+L"d3d9.dll").c_str());
    if(!Check(dll!=nullptr,"load local proxy"))return 2;
    // Invoke a tail-jump forwarding export BEFORE creating a device (lazy loader / stdcall ABI).
    using Begin=int(WINAPI*)(D3DCOLOR,LPCWSTR);using End=int(WINAPI*)();
    auto begin=reinterpret_cast<Begin>(GetProcAddress(dll,"D3DPERF_BeginEvent"));
    auto end=reinterpret_cast<End>(GetProcAddress(dll,"D3DPERF_EndEvent"));
    if(Check(begin&&end,"performance exports")){begin(0,L"FENB smoke test");end();Check(true,"lazy export forwarding ABI");}
    using Create=IDirect3D9*(WINAPI*)(UINT);
    auto create=reinterpret_cast<Create>(GetProcAddress(dll,"Direct3DCreate9"));
    Com<IDirect3D9> d3d;d3d.p=create(D3D_SDK_VERSION);
    if(!Check(d3d.p!=nullptr,"Direct3DCreate9"))return 3;
    IUnknown* identity=nullptr;d3d->QueryInterface(IID_IUnknown,(void**)&identity);
    Check(identity==d3d.p,"root COM identity");Drop(identity);
    HINSTANCE instance=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};wc.lpfnWndProc=Wnd;wc.hInstance=instance;wc.lpszClassName=L"FENBPreview";wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));RegisterClassW(&wc);
    HWND window=CreateWindowW(wc.lpszClassName,L"FENB — DX9 preview",WS_OVERLAPPEDWINDOW,80,70,1180,850,nullptr,nullptr,instance,nullptr);
    if(interactive)ShowWindow(window,SW_SHOWNORMAL);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=(interactive||aaTest)?D3DSWAPEFFECT_DISCARD:D3DSWAPEFFECT_COPY;pp.hDeviceWindow=window;
    pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferWidth=1160;pp.BackBufferHeight=800;pp.BackBufferCount=1;
    pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D16;
    Com<IDirect3DDevice9> dev;
    HRESULT hr=d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,dev.out());
    if(FAILED(hr))hr=d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,dev.out());
    if(!Check(SUCCEEDED(hr),"create DX9 device"))return 4;
    Com<IDirect3D9> parent;dev->GetDirect3D(parent.out());Check(parent.p==d3d.p,"GetDirect3D returns wrapped identity");Drop(parent.p);
    Com<IDirect3DTexture9> texture;
    Check(SUCCEEDED(dev->CreateTexture(16,16,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,texture.out(),nullptr)),"create wrapped texture");
    Com<IDirect3DDevice9> resourceDevice;texture->GetDevice(resourceDevice.out());
    Check(resourceDevice.p==dev.p,"resource GetDevice preserves wrapped identity");Drop(resourceDevice.p);
    Com<IDirect3DSurface9> level;texture->GetSurfaceLevel(0,level.out());
    Com<IDirect3DTexture9> container;level->GetContainer(IID_IDirect3DTexture9,(void**)container.out());
    Check(container.p==texture.p,"surface GetContainer preserves identity");Drop(container.p);Drop(level.p);
    Com<IDirect3DSurface9> bb;dev->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,bb.out());
    Com<IDirect3DSwapChain9> chain;dev->GetSwapChain(0,chain.out());
    Com<IDirect3DDevice9> chainDevice;chain->GetDevice(chainDevice.out());Check(chainDevice.p==dev.p,"swap-chain device identity");Drop(chainDevice.p);
    auto recover=[&](){
        Drop(bb.p);Drop(chain.p);
        HRESULT cooperative=dev->TestCooperativeLevel();
        if(cooperative!=D3DERR_DEVICENOTRESET)return false;
        auto reset=pp;
        if(FAILED(dev->Reset(&reset)))return false;
        return SUCCEEDED(dev->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,bb.out())) &&
               SUCCEEDED(dev->GetSwapChain(0,chain.out()));
    };
    if(aaTest){
        unsigned resets=0;
        for(int samples:{2,4,8,-1,0}){
            DWORD quality=0;
            if(samples>0 && FAILED(d3d->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,pp.BackBufferFormat,
                                                   TRUE,D3DMULTISAMPLE_TYPE(samples),&quality)))continue;
            test.msaaSamples=samples;test.save(config);
            Sleep(550);
            Scene(dev,bb,1160,800);
            HRESULT result=dev->Present(nullptr,nullptr,nullptr,nullptr);
            Check(result==D3DERR_DEVICELOST,"MSAA requests host recovery after Present");
            if(result==D3DERR_DEVICELOST){Check(recover(),"host releases resources and resets MSAA");++resets;}
            D3DPRESENT_PARAMETERS actual{};if(chain.p)chain->GetPresentParameters(&actual);
            Check(actual.MultiSampleType==(samples>0?D3DMULTISAMPLE_TYPE(samples):D3DMULTISAMPLE_NONE),
                  "MSAA active samples match selected mode");
            Scene(dev,bb,1160,800);
            Check(SUCCEEDED(dev->Present(nullptr,nullptr,nullptr,nullptr)),"MSAA does not request repeated resets");
        }
        Check(resets>=3,"multiple live MSAA changes exercised");
        Drop(bb.p);Drop(chain.p);Drop(texture.p);Drop(dev.p);saved.save(config);
        return failures?1:0;
    }
    if(interactive){
        while(IsWindow(window)){
            Pump();
            if(!bb.p){recover();Sleep(8);continue;}
            Scene(dev,bb,1160,800);
            if(dev->Present(nullptr,nullptr,nullptr,nullptr)==D3DERR_DEVICELOST)recover();
            Sleep(8);
        }
        Drop(bb.p);Drop(chain.p);Drop(texture.p);Drop(dev.p);saved.save(config);return 0;
    }
    Com<IDirect3DSurface9> custom,depth;
    dev->CreateRenderTarget(320,200,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,custom.out(),nullptr);
    dev->CreateDepthStencilSurface(320,200,D3DFMT_D16,D3DMULTISAMPLE_NONE,0,TRUE,depth.out(),nullptr);
    Com<IDirect3DPixelShader9> shader;dev->CreatePixelShader(ShaderComposite,shader.out());
    dev->SetRenderTarget(0,custom);dev->SetDepthStencilSurface(depth);
    D3DVIEWPORT9 vp{5,7,111,83,.1f,.9f};RECT rect{3,8,67,88};
    dev->SetViewport(&vp);dev->SetScissorRect(&rect);dev->SetRenderState(D3DRS_FILLMODE,D3DFILL_WIREFRAME);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);dev->SetRenderState(D3DRS_SRGBWRITEENABLE,TRUE);
    dev->SetPixelShader(shader);dev->SetTexture(0,texture);
    float constants[4]={.17f,.31f,.63f,.91f};dev->SetPixelShaderConstantF(0,constants,1);
    SIZE_T before=PrivateBytes();
    for(int frame=0;frame<4;++frame){Pump();Scene(dev,bb,pp.BackBufferWidth,pp.BackBufferHeight);hr=dev->Present(nullptr,nullptr,nullptr,nullptr);}
    printf("INFO: warm-up private commit delta at 1160x800 (effects + UI + driver): %.2f MiB\n",double(PrivateBytes()-before)/1048576.0);
    Check(SUCCEEDED(hr),"device Present with postprocessing and panel");
    DWORD state=0;dev->GetRenderState(D3DRS_FILLMODE,&state);Check(state==D3DFILL_WIREFRAME,"restore fill mode");
    dev->GetRenderState(D3DRS_ALPHABLENDENABLE,&state);Check(state==TRUE,"restore alpha blending");
    dev->GetRenderState(D3DRS_SRGBWRITEENABLE,&state);Check(state==TRUE,"restore sRGB state");
    D3DVIEWPORT9 gotVP{};dev->GetViewport(&gotVP);Check(memcmp(&vp,&gotVP,sizeof(vp))==0,"restore viewport");
    RECT gotRect{};dev->GetScissorRect(&gotRect);Check(EqualRect(&rect,&gotRect),"restore scissor");
    float gotConstants[4]{};dev->GetPixelShaderConstantF(0,gotConstants,1);Check(memcmp(constants,gotConstants,sizeof(constants))==0,"restore shader constants");
    Com<IDirect3DSurface9> gotTarget;dev->GetRenderTarget(0,gotTarget.out());Check(gotTarget.p==custom.p,"restore offscreen render target");Drop(gotTarget.p);
    dev->GetDepthStencilSurface(gotTarget.out());Check(gotTarget.p==depth.p,"restore depth surface");Drop(gotTarget.p);
    Com<IDirect3DBaseTexture9> gotTexture;dev->GetTexture(0,gotTexture.out());Check(gotTexture.p==texture.p,"restore texture binding");Drop(gotTexture.p);
    Com<IDirect3DPixelShader9> gotShader;dev->GetPixelShader(gotShader.out());Check(gotShader.p==shader.p,"restore pixel shader");Drop(gotShader.p);
    Check(Screenshot(dev,bb,(dir+L"FENB-panel.png").c_str()),"save actual DX9 panel screenshot");
    auto frameSample=[&](bool panel=false){Scene(dev,bb,pp.BackBufferWidth,pp.BackBufferHeight);dev->Present(nullptr,nullptr,nullptr,nullptr);return Sample(dev,bb,panel?540:316,panel?500:300);};
    // Sample the blank panel interior, away from rows that change on navigation.
    DWORD panelPixel=frameSample(true);
    Hotkey(window,true);DWORD hiddenPixel=frameSample(true);Check(hiddenPixel!=panelPixel,"Right Shift + Enter closes panel");
    Hotkey(window,false,false,false);Check(frameSample(true)==panelPixel,"Left Shift + Enter opens panel");
    Hotkey(window,false,true);Check(frameSample(true)==panelPixel,"Enter auto-repeat does not toggle panel");
    Hotkey(window,true);
    // Master state belongs to this running client, not external INI writes.
    EffectsHotkey(window);Sleep(500);frameSample();
    DWORD original=Sample(dev,bb,316,300);
    Check(original==0x232739,"disabled effects preserve source pixels");
    EffectsHotkey(window);Sleep(500);frameSample();
    test.enabled=false;test.bloom=false;test.color=true;test.exposure=1;test.save(config);Sleep(550);frameSample();
    DWORD graded=Sample(dev,bb,316,300);
    Check((graded&255)>(original&255),"shared exposure changes pixels without importing the master switch");
    test.color=false;test.bloom=true;test.threshold=.5f;test.bloomStrength=1;test.radius=2;test.save(config);Sleep(550);frameSample();
    DWORD glow=Sample(dev,bb,885,65);
    Check((glow&255)>(original&255),"bloom spreads outside bright geometry");
    Scene(dev,bb,pp.BackBufferWidth,pp.BackBufferHeight);
    Check(SUCCEEDED(chain->Present(nullptr,nullptr,nullptr,nullptr,0)),"implicit swap-chain Present");
    D3DPRESENT_PARAMETERS additionalParams=pp;additionalParams.BackBufferWidth=640;additionalParams.BackBufferHeight=480;
    Com<IDirect3DSwapChain9> additional;
    Check(SUCCEEDED(dev->CreateAdditionalSwapChain(&additionalParams,additional.out())),"create additional swap chain");
    if(additional.p){Com<IDirect3DSurface9> back;additional->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,back.out());Scene(dev,back,640,480);
        Check(SUCCEEDED(additional->Present(nullptr,nullptr,nullptr,nullptr,0)),"additional swap-chain Present");}
    Drop(additional.p);
    dev->SetRenderTarget(0,bb);dev->SetDepthStencilSurface(nullptr);dev->SetTexture(0,nullptr);dev->SetPixelShader(nullptr);
    Drop(custom.p);Drop(depth.p);Drop(shader.p);Drop(chain.p);Drop(bb.p);
    Hotkey(window,true); // Exercise font atlas / vertex buffer recreation with the panel visible.
    // COPY only permits one back buffer, so this deliberately fails Reset.
    auto invalid=pp;invalid.BackBufferCount=2;
    Check(FAILED(dev->Reset(&invalid)),"failed Reset handled");
    for(int i=0;i<6;++i){
        pp.BackBufferWidth=i%2?1160:1024;pp.BackBufferHeight=i%2?800:720;
        if(!Check(SUCCEEDED(dev->Reset(&pp)),"successful Reset / resize cycle"))break;
        Com<IDirect3DSurface9> current;dev->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,current.out());
        Scene(dev,current,pp.BackBufferWidth,pp.BackBufferHeight);
        Check(SUCCEEDED(dev->Present(nullptr,nullptr,nullptr,nullptr)),"draw after Reset");
        Check(Sample(dev,current,540,500)==panelPixel,"panel redraw after Reset");
    }
    Drop(texture.p);Drop(dev.p);
    Check(true,"device and UI teardown completed");
    using CreateEx=HRESULT(WINAPI*)(UINT,IDirect3D9Ex**);
    auto createEx=reinterpret_cast<CreateEx>(GetProcAddress(dll,"Direct3DCreate9Ex"));
    Com<IDirect3D9Ex> ex;
    if(createEx&&SUCCEEDED(createEx(D3D_SDK_VERSION,ex.out()))){
        Com<IDirect3DDevice9Ex> dx;
        hr=ex->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,nullptr,dx.out());
        if(Check(SUCCEEDED(hr),"CreateDeviceEx")){
            Com<IDirect3DSurface9> back;dx->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,back.out());Scene(dx,back,pp.BackBufferWidth,pp.BackBufferHeight);
            Check(SUCCEEDED(dx->PresentEx(nullptr,nullptr,nullptr,nullptr,0)),"PresentEx");Drop(back.p);
            Check(SUCCEEDED(dx->ResetEx(&pp,nullptr)),"ResetEx");
        }
    }else puts("SKIP: DX9Ex unavailable");
    DestroyWindow(window);saved.save(config);
    printf("%s: %d failure(s)\n",failures?"FAILED":"PASSED",failures);
    return failures?1:0;
}
