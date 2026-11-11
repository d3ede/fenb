#include "common.hpp"
#include "generated/shaders.hpp"
#include <cstdio>
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);std::wstring dir(exe);dir=dir.substr(0,dir.find_last_of(L"\\/")+1);
    auto request=dir+L"FENB-capture.request";FILE* f=_wfopen(request.c_str(),L"wb");CHECK(f);fclose(f);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FENBProbeTest";RegisterClassW(&wc);
    HWND window=CreateWindowW(wc.lpszClassName,L"Probe test",WS_OVERLAPPEDWINDOW,0,0,800,600,nullptr,nullptr,wc.hInstance,nullptr);
    Com<IDirect3D9> root;root.p=Direct3DCreate9(D3D_SDK_VERSION);CHECK(root.p);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_COPY;pp.hDeviceWindow=window;pp.BackBufferWidth=800;pp.BackBufferHeight=600;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
    Com<IDirect3DDevice9> device;CHECK(SUCCEEDED(root->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,device.out())));
    Com<IDirect3DTexture9> texture;CHECK(SUCCEEDED(device->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,texture.out(),nullptr)));
    D3DLOCKED_RECT lock{};CHECK(SUCCEEDED(texture->LockRect(0,&lock,nullptr,0)));for(int y=0;y<4;++y)for(int x=0;x<4;++x)*reinterpret_cast<DWORD*>((BYTE*)lock.pBits+y*lock.Pitch+x*4)=0xff336699;texture->UnlockRect(0);
    Com<IDirect3DPixelShader9> shader;CHECK(SUCCEEDED(device->CreatePixelShader(ShaderBright,shader.out())));
    D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
    struct V{float x,y,z,nx,ny,nz,u,v;};V vertices[]={{-.5f,-.5f,.5f,0,0,-1,0,0},{0,.5f,.5f,0,0,-1,.5f,1},{.5f,-.5f,.5f,0,0,-1,1,0}};
    for(int frame=0;frame<4;++frame){
        CHECK(SUCCEEDED(device->BeginScene()));
        device->SetTransform(D3DTS_WORLD,&identity);device->SetTransform(D3DTS_VIEW,&identity);device->SetTransform(D3DTS_PROJECTION,&identity);
        device->SetFVF(D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1);device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetTexture(0,texture);
        device->SetPixelShader(nullptr);CHECK(SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,vertices,sizeof(V))));
        device->SetPixelShader(shader);CHECK(SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,vertices,sizeof(V))));
        CHECK(SUCCEEDED(device->EndScene()));CHECK(SUCCEEDED(device->Present(nullptr,nullptr,nullptr,nullptr)));
    }
    DestroyWindow(window);puts("PASS: probe emitted three frames with fixed-function and shader draws.");return 0;
}
