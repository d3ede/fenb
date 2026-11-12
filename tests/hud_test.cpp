#include "common.hpp"
#include "settings.hpp"
#include <cstdio>
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
std::wstring Directory(){wchar_t p[32768]{};GetModuleFileNameW(nullptr,p,32768);std::wstring s(p);return s.substr(0,s.find_last_of(L"\\/")+1);}
DWORD Pixel(IDirect3DDevice9* device,IDirect3DSurface9* source,UINT x,UINT y){
    D3DSURFACE_DESC d{};source->GetDesc(&d);Com<IDirect3DSurface9> resolved,read;
    if(FAILED(device->CreateRenderTarget(d.Width,d.Height,d.Format,D3DMULTISAMPLE_NONE,0,FALSE,resolved.out(),nullptr)))return 0;
    if(FAILED(device->StretchRect(source,nullptr,resolved,nullptr,D3DTEXF_NONE)))return 0;
    if(FAILED(device->CreateOffscreenPlainSurface(d.Width,d.Height,d.Format,D3DPOOL_SYSTEMMEM,read.out(),nullptr)))return 0;
    if(FAILED(device->GetRenderTargetData(resolved,read)))return 0;
    D3DLOCKED_RECT lock{};if(FAILED(read->LockRect(&lock,nullptr,D3DLOCK_READONLY)))return 0;
    DWORD value=*reinterpret_cast<DWORD*>((BYTE*)lock.pBits+size_t(y)*lock.Pitch+x*4)&0xffffff;read->UnlockRect();return value;
}
void Marker(IDirect3DDevice9* device){
    struct V{float x,y,z;DWORD color;};V v[]={{3,3,.5f,0xffffffff},{4,3,.5f,0xffffffff},{3,4,.5f,0xffffffff}};
    device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,v,sizeof(V));
}
bool Frame(IDirect3DDevice9* device,bool world,bool swapchain,bool readAfter,bool preWorld=false){
    Com<IDirect3DSurface9> bb;device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,bb.out());
    device->ColorFill(bb,nullptr,0xff202830);
    if(FAILED(device->BeginScene()))return false;
    D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
    device->SetTransform(D3DTS_WORLD,&identity);device->SetTransform(D3DTS_VIEW,&identity);device->SetTransform(D3DTS_PROJECTION,&identity);
    device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);device->SetTexture(0,nullptr);
    device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    struct UI{float x,y,z,w;DWORD color;};UI v[]={{20,20,0,1,0xff40a0e0},{100,20,0,1,0xff40a0e0},{20,60,0,1,0xff40a0e0},{100,60,0,1,0xff40a0e0}};
    auto perspective=identity;perspective._33=perspective._34=1;perspective._43=-.1f;perspective._44=0;
    device->SetTransform(D3DTS_PROJECTION,&perspective);
    if(preWorld){
        device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
        Marker(device); // Sky geometry alone is not the opaque world phase.
        device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
        if(FAILED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(UI))))return false;
        device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff202830,1,0);
    }
    device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
    if(world)Marker(device);
    device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
    if(FAILED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(UI))))return false;
    device->SetTransform(D3DTS_PROJECTION,&identity);
    Marker(device); // 3D portrait/preview after HUD must not trigger another full-screen pass
    if(FAILED(device->EndScene()))return false;
    DWORD hud=Pixel(device,bb,40,40),background=Pixel(device,bb,200,150);
    if(hud!=0x40a0e0){printf("HUD pixel modified: %06lx\n",hud);return false;}
    if(world?((background&255)<=0x30):(background!=0x202830)){printf("Unexpected world pixel: %06lx (world=%d)\n",background,world);return false;}
    HRESULT hr;
    if(swapchain){Com<IDirect3DSwapChain9> chain;device->GetSwapChain(0,chain.out());hr=chain->Present(nullptr,nullptr,nullptr,nullptr,0);}
    else hr=device->Present(nullptr,nullptr,nullptr,nullptr);
    if(FAILED(hr))return false;
    if(readAfter&&(Pixel(device,bb,40,40)!=hud||Pixel(device,bb,200,150)!=background)){puts("Present modified HUD or applied scene effects twice");return false;}
    return true;
}
int main(){
    auto path=Directory()+L"FENB.ini";Settings saved;saved.load(path);Settings s;s.openOnStart=false;s.exposure=2;s.bloomStrength=1;s.smaa=true;s.save(path);
    struct Restore{Settings settings;std::wstring path;~Restore(){settings.save(path);}}restore{saved,path};
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FENBHudTest";RegisterClassW(&wc);
    HWND window=CreateWindowW(wc.lpszClassName,L"HUD test",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,wc.hInstance,nullptr);
    Com<IDirect3D9> root;root.p=Direct3DCreate9(D3D_SDK_VERSION);CHECK(root.p);
    for(int msaa:{0,8}){
        DWORD quality=0;if(msaa&&FAILED(root->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,TRUE,D3DMULTISAMPLE_TYPE(msaa),&quality))){puts("SKIP: 8x MSAA unavailable");continue;}
        D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=msaa?D3DSWAPEFFECT_DISCARD:D3DSWAPEFFECT_COPY;pp.hDeviceWindow=window;pp.BackBufferWidth=320;pp.BackBufferHeight=240;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.MultiSampleType=D3DMULTISAMPLE_TYPE(msaa);
        Com<IDirect3DDevice9> device;CHECK(SUCCEEDED(root->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,device.out())));
        CHECK(Frame(device,true,false,!msaa));CHECK(Frame(device,true,true,!msaa));CHECK(Frame(device,false,false,!msaa));
        CHECK(Frame(device,true,false,!msaa,true)); // Pre-world RHW passes cannot disable post effects.
        CHECK(SUCCEEDED(device->Reset(&pp)));CHECK(Frame(device,true,false,!msaa));
        printf("PASS: HUD exact RGB preserved, world graded once, UI-only skipped, Reset, MSAA=%d\n",msaa);
    }
    DestroyWindow(window);return 0;
}
