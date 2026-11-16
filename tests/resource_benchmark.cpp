#include "common.hpp"
#include <cstdio>
#include <cstdlib>
#include <algorithm>

// Runs against either DLL without injecting into or changing a game client.
int wmain(int argc,wchar_t **argv) {
    if(argc!=2)return 2;
    HMODULE dll=LoadLibraryW(argv[1]);if(!dll)return 3;
    auto create=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(dll,"Direct3DCreate9"));
    if(!create)return 4;
    Com<IDirect3D9> root;root.p=create(D3D_SDK_VERSION);
    HWND window=CreateWindowW(L"STATIC",L"FENB resource benchmark",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
    pp.BackBufferWidth=pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    Com<IDirect3DDevice9> device;
    if(FAILED(root->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,device.out())))return 5;
    Com<IDirect3DTexture9> textures[192];
    for(unsigned i=0;i<192;++i) {
        auto &t=textures[i];
        if(FAILED(device->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,t.out(),nullptr)))return 6;
        D3DLOCKED_RECT pixels{};if(FAILED(t->LockRect(0,&pixels,nullptr,0)))return 7;
        for(int y=0;y<4;++y)for(int x=0;x<4;++x)*reinterpret_cast<DWORD *>(static_cast<unsigned char *>(pixels.pBits)+y*pixels.Pitch+x*4)=0xff000000u|((i*977u+x*19681u+y*7919u)&0xffffffu);
        t->UnlockRect(0);
    }
    LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    double results[7]{};
    for(int run=0;run<7;++run){
        LARGE_INTEGER a{},b{};QueryPerformanceCounter(&a);
        for(unsigned i=0;i<2000000;++i){
            // Reuse a material for several consecutive mesh bindings, then move
            // to another object; real material changes still reach DirectX.
            auto *texture=textures[(i/8)%192].p;
            device->SetTexture(0,texture);
        }
        QueryPerformanceCounter(&b);results[run]=double(b.QuadPart-a.QuadPart)*1000/frequency.QuadPart;
        printf("run=%d bindings=2000000 ms=%.3f\n",run,results[run]);
    }
    std::sort(results+1,results+7);printf("median_ms=%.3f ns_per_binding=%.3f\n",(results[3]+results[4])*.5,(results[3]+results[4])*.25);
    device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff123456,1,0);
    device->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);device->SetRenderState(D3DRS_ZENABLE,FALSE);
    device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
    if(FAILED(device->BeginScene()))return 8;
    for(unsigned i=0;i<192;++i){
        const float x=float((i%16)*4)-.5f,y=float((i/16)*5)-.5f;
        struct Vertex {float x,y,z,w,u,v;};Vertex quad[]={{x,y,.5f,1,0,0},{x+4,y,.5f,1,1,0},{x,y+5,.5f,1,0,1},{x+4,y+5,.5f,1,1,1}};
        if(FAILED(device->SetTexture(0,textures[i]))||FAILED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex))))return 9;
    }
    device->EndScene();
    Com<IDirect3DSurface9> target,copy;
    if(FAILED(device->GetRenderTarget(0,target.out()))||FAILED(device->CreateOffscreenPlainSurface(64,64,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,copy.out(),nullptr))||FAILED(device->GetRenderTargetData(target,copy)))return 10;
    D3DLOCKED_RECT pixels{};if(FAILED(copy->LockRect(&pixels,nullptr,D3DLOCK_READONLY)))return 11;
    uint32_t hash=2166136261u;
    for(int y=0;y<64;++y)for(int x=0;x<64;++x){DWORD value=*reinterpret_cast<DWORD *>(static_cast<unsigned char *>(pixels.pBits)+y*pixels.Pitch+x*4)&0xffffffu;hash=(hash^value)*16777619u;}
    copy->UnlockRect();printf("frame_rgb_hash=%08x\n",hash);copy.out();target.out();
    device->SetTexture(0,nullptr);
    for(auto &t:textures)t.out();device.out();root.out();DestroyWindow(window);
}
