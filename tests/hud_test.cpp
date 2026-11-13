#include "common.hpp"
#include "settings.hpp"
#include "scene_tags.hpp"
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
// Optional native material identity check; test data stays in the local client.
IDirect3DTexture9* NativeArrow(IDirect3DDevice9* device){
    FILE* file=_wfopen(L"X:/Games/Homesickness/Model/Texture/etc_arrow-01.dds",L"rb");
    if(!file){puts("SKIP: native quest-arrow asset unavailable");return nullptr;}
    DWORD header[32]{};bool ok=fread(header,1,sizeof(header),file)==sizeof(header);
    const UINT w=header[4],h=header[3],pitch=((w+3)/4)*8,rows=(h+3)/4;
    IDirect3DTexture9* texture=nullptr;
    ok=ok&&header[0]==0x20534444&&header[21]==D3DFMT_DXT1&&w==128&&h==256;
    if(ok)ok=SUCCEEDED(device->CreateTexture(w,h,1,0,D3DFMT_DXT1,D3DPOOL_MANAGED,&texture,nullptr));
    D3DLOCKED_RECT lock{};
    if(ok){
        ok=SUCCEEDED(texture->LockRect(0,&lock,nullptr,0));
        if(ok){for(UINT y=0;y<rows;++y)ok=(fread((BYTE*)lock.pBits+y*lock.Pitch,1,pitch,file)==pitch)&&ok;texture->UnlockRect(0);}
    }
    fclose(file);if(!ok)Drop(texture);return texture;
}
bool OverlayFrame(IDirect3DDevice9* device,IDirect3DTexture9* arrow,bool beforeHUD,bool visible){
    Com<IDirect3DSurface9> bb;device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,bb.out());
    if(FAILED(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202830,1,0))||FAILED(device->BeginScene()))return false;
    D3DMATRIX id{};id._11=id._22=id._33=id._44=1;
    auto p=id;p._33=100.f/99.9f;p._34=1;p._43=-.1f*p._33;p._44=0;
    device->SetTransform(D3DTS_WORLD,&id);device->SetTransform(D3DTS_VIEW,&id);device->SetTransform(D3DTS_PROJECTION,&p);
    device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);device->SetTexture(0,nullptr);
    device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_FOGENABLE,FALSE);
    device->SetRenderState(D3DRS_ZENABLE,TRUE);device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
    device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    Marker(device);
    // Different sky and world colors expose fog applied to invisible marker depth.
    device->ColorFill(bb,nullptr,0xff404820);
    struct UI{float x,y,z,w;DWORD color;};
    UI hud[]={{20,20,0,1,0xff40a0e0},{100,20,0,1,0xff40a0e0},{20,60,0,1,0xff40a0e0},{100,60,0,1,0xff40a0e0}};
    auto drawHUD=[&](){device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);device->SetRenderState(D3DRS_ZENABLE,FALSE);
        return SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,hud,sizeof(UI)));};
    if(!beforeHUD&&!drawHUD())return false;
    struct V{float x,y,z,nx,ny,nz,u,v;};
    V vertices[31]={{-2,-2,5,0,0,-1,.5f,.5f},{2,-2,5,0,0,-1,.5f,.5f},{0,2,5,0,0,-1,.5f,.5f}};
    WORD indices[84]{};for(int i=0;i<84;++i)indices[i]=WORD(i%3);
    D3DMATERIAL9 material{};material.Ambient=material.Diffuse={1,1,1,visible?1.f:0.f};
    device->SetMaterial(&material);device->SetRenderState(D3DRS_AMBIENT,0xffffffff);
    device->SetRenderState(D3DRS_LIGHTING,TRUE);device->SetRenderState(D3DRS_ZENABLE,TRUE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);device->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE);
    device->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_ALWAYS);device->SetRenderState(D3DRS_ALPHAREF,0);
    device->SetFVF(0x112);device->SetTexture(0,arrow);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
    device->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
    device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
    if(FAILED(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,31,28,indices,D3DFMT_INDEX16,vertices,sizeof(V))))return false;
    D3DMATERIAL9 restored{};device->GetMaterial(&restored);
    if(restored.Diffuse.a!=material.Diffuse.a){puts("Native marker material was modified");return false;}
    device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);device->SetTexture(0,nullptr);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    if(!drawHUD())return false;
    UI translucent[]={{220,150,0,1,0x8040a0e0},{280,150,0,1,0x8040a0e0},{220,190,0,1,0x8040a0e0},{280,190,0,1,0x8040a0e0}};
    device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
    device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
    if(FAILED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,translucent,sizeof(UI)))||FAILED(device->EndScene()))return false;
    const DWORD center=Pixel(device,bb,160,120),outside=Pixel(device,bb,250,120),hudColor=Pixel(device,bb,40,40);
    printf("Native 3D overlay: first=%d visible=%d center=%06lx outside=%06lx HUD=%06lx\n",beforeHUD,visible,center,outside,hudColor);
    if(hudColor!=0x40a0e0||(visible?center==outside:center!=outside))return false;
    const DWORD blended=Pixel(device,bb,250,170);
    for(unsigned shift:{0u,8u,16u}){
        const int expected=(((0x40a0e0>>shift)&255)*128+((outside>>shift)&255)*127+127)/255;
        if(std::abs(int((blended>>shift)&255)-expected)>1){puts("Translucent HUD was filtered instead of natively blended");return false;}
    }
    return SUCCEEDED(device->Present(nullptr,nullptr,nullptr,nullptr));
}
bool Frame(IDirect3DDevice9* device,bool world,bool swapchain,bool readAfter,int preWorld=0,bool newWorld=false,bool visibilityProbe=false){
    Com<IDirect3DSurface9> bb;device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,bb.out());
    device->ColorFill(bb,nullptr,0xff202830);
    if(FAILED(device->BeginScene()))return false;
    D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
    device->SetTransform(D3DTS_WORLD,&identity);device->SetTransform(D3DTS_VIEW,&identity);device->SetTransform(D3DTS_PROJECTION,&identity);
    device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);device->SetTexture(0,nullptr);
    device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_ZENABLE,TRUE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    struct UI{float x,y,z,w;DWORD color;};UI v[]={{20,20,0,1,0xff40a0e0},{100,20,0,1,0xff40a0e0},{20,60,0,1,0xff40a0e0},{100,60,0,1,0xff40a0e0}};
    auto perspective=identity;perspective._33=perspective._34=1;perspective._43=-.1f;perspective._44=0;
    device->SetTransform(D3DTS_PROJECTION,&perspective);
    if(preWorld){
        device->SetRenderState(D3DRS_ZWRITEENABLE,preWorld==2);
        device->SetRenderState(D3DRS_ZENABLE,preWorld!=2);
        Marker(device); // Sky geometry alone is not the opaque world phase.
        device->SetRenderState(D3DRS_ZENABLE,FALSE);
        device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
        if(FAILED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(UI))))return false;
        if(preWorld==2)device->ColorFill(bb,nullptr,0xff202830);
        else device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff202830,1,0);
    }
    device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
    device->SetRenderState(D3DRS_ZENABLE,TRUE);
    if(world)Marker(device);
    if(visibilityProbe){
        device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE,0);
        if(FAILED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(UI))))return false;
        device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
        device->ColorFill(bb,nullptr,0xff202830);
    }
    if(newWorld){
        device->SetRenderState(D3DRS_ZENABLE,FALSE);
        device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
        if(FAILED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(UI))))return false;
        // A second main render after a completed world/HUD epoch, before Present.
        if(FAILED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff202830,1,0)))return false;
        device->SetRenderState(D3DRS_ZENABLE,TRUE);
        Marker(device);
    }
    device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
    device->SetRenderState(D3DRS_ZENABLE,FALSE);
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
bool SurfaceBatchFrame(IDirect3DDevice9 *d,int mutation,std::vector<DWORD> &pixels) {
    struct V {float x,y,z,nx,ny,nz,u,v,u1,v1;};
    V floor[8]{};floor[4]={-4,-1,2,0,1,0,0,0,0,0};floor[5]={-4,-1,12,0,1,0,0,1,0,0};
    floor[6]={4,-1,2,0,1,0,1,0,0,0};floor[7]={4,-1,12,0,1,0,1,1,0,0};
    WORD indices[]={0,0,0,0,0,0,0,1,2,2,1,3};
    Com<IDirect3DVertexBuffer9> vb;Com<IDirect3DIndexBuffer9> ib;Com<IDirect3DTexture9> texture;
    if(FAILED(d->CreateVertexBuffer(sizeof(floor),D3DUSAGE_WRITEONLY,0x212,D3DPOOL_MANAGED,vb.out(),nullptr)) ||
       FAILED(d->CreateIndexBuffer(sizeof(indices),D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_MANAGED,ib.out(),nullptr)) ||
       FAILED(d->CreateTexture(2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,texture.out(),nullptr)))return false;
    void *data=nullptr;D3DLOCKED_RECT lock{};
    if(FAILED(vb->Lock(0,0,&data,0)))return false;memcpy(data,floor,sizeof(floor));vb->Unlock();
    if(FAILED(ib->Lock(0,0,&data,0)))return false;memcpy(data,indices,sizeof(indices));ib->Unlock();
    if(FAILED(texture->LockRect(0,&lock,nullptr,0)))return false;
    for(int y=0;y<2;++y)for(int x=0;x<2;++x)((DWORD*)((BYTE*)lock.pBits+y*lock.Pitch))[x]=0xff505050;
    texture->UnlockRect(0);DWORD tag=32;texture->SetPrivateData(EmitterTag,&tag,4,0);
    if(FAILED(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202830,1,0)) || FAILED(d->BeginScene()))return false;
    D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
    auto projection=identity;projection._33=100.f/99.9f;projection._34=1;projection._43=-.1f*projection._33;projection._44=0;
    d->SetTransform(D3DTS_WORLD,&identity);d->SetTransform(D3DTS_VIEW,&identity);d->SetTransform(D3DTS_PROJECTION,&projection);
    d->SetVertexShader(nullptr);d->SetPixelShader(nullptr);d->SetFVF(0x212);
    for(auto key:{D3DRS_LIGHTING,D3DRS_FOGENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_ALPHATESTENABLE})d->SetRenderState(key,FALSE);
    d->SetRenderState(D3DRS_ZENABLE,TRUE);d->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);d->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
    d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);d->SetTexture(0,texture);
    d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
    d->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,0);
    d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    d->SetStreamSource(0,vb,0,sizeof(V));d->SetIndices(ib);
    if(FAILED(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,4,0,4,6,2)))return false;
    // These native proxy writes must flush the old geometry/coverage before mutation.
    if(mutation==1){if(FAILED(vb->Lock(0,0,&data,0)))return false;for(auto &v:floor)v.x+=100;memcpy(data,floor,sizeof(floor));vb->Unlock();}
    if(mutation==2){if(FAILED(ib->Lock(0,0,&data,0)))return false;memset(data,0,sizeof(indices));ib->Unlock();}
    if(mutation==3){if(FAILED(texture->LockRect(0,&lock,nullptr,0)))return false;
        for(int y=0;y<2;++y)for(int x=0;x<2;++x)((DWORD*)((BYTE*)lock.pBits+y*lock.Pitch))[x]=0xff000000;texture->UnlockRect(0);}
    struct C {float x,y,z;DWORD color;};C wall[]={{-.7f,-1,6,0xff1030d0},{.7f,-1,6,0xff1030d0},{-.7f,1,6,0xff1030d0},{.7f,1,6,0xff1030d0}};
    d->SetTexture(0,nullptr);d->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
    d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,wall,sizeof(C));
    struct U{float x,y,z,w;DWORD color;};U hud[]={{8,8,0,1,0xff40a0e0},{40,8,0,1,0xff40a0e0},{8,28,0,1,0xff40a0e0},{40,28,0,1,0xff40a0e0}};
    d->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);d->SetRenderState(D3DRS_ZENABLE,FALSE);
    d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,hud,sizeof(U));d->EndScene();
    Com<IDirect3DSurface9> bb,resolved,read;d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,bb.out());D3DSURFACE_DESC desc{};bb->GetDesc(&desc);
    if(FAILED(d->CreateRenderTarget(320,240,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,resolved.out(),nullptr)) ||
       FAILED(d->StretchRect(bb,nullptr,resolved,nullptr,D3DTEXF_NONE)) ||
       FAILED(d->CreateOffscreenPlainSurface(320,240,desc.Format,D3DPOOL_SYSTEMMEM,read.out(),nullptr)) ||
       FAILED(d->GetRenderTargetData(resolved,read)) || FAILED(read->LockRect(&lock,nullptr,D3DLOCK_READONLY)))return false;
    pixels.resize(320*240);for(int y=0;y<240;++y)memcpy(pixels.data()+y*320,(BYTE*)lock.pBits+y*lock.Pitch,320*4);read->UnlockRect();
    return SUCCEEDED(d->Present(nullptr,nullptr,nullptr,nullptr));
}
int main(){
    auto path=Directory()+L"FENB.ini";Settings saved;saved.load(path);Settings s;s.openOnStart=false;s.exposure=2;s.bloomStrength=1;s.smaa=true;
    s.fog=true;s.fogStart=0;s.fogDensity=.03f;s.fogFalloff=0;
    s.surfaceReflections=true;s.surfaceStrength=1;s.surfaceRoughness=.2f;s.save(path);
    struct Restore{Settings settings;std::wstring path;~Restore(){settings.save(path);}}restore{saved,path};
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FENBHudTest";RegisterClassW(&wc);
    HWND window=CreateWindowW(wc.lpszClassName,L"HUD test",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,wc.hInstance,nullptr);
    Com<IDirect3D9> root;root.p=Direct3DCreate9(D3D_SDK_VERSION);CHECK(root.p);
    for(int msaa:{0,8}){
        DWORD quality=0;if(msaa&&FAILED(root->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,TRUE,D3DMULTISAMPLE_TYPE(msaa),&quality))){puts("SKIP: 8x MSAA unavailable");continue;}
        D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=msaa?D3DSWAPEFFECT_DISCARD:D3DSWAPEFFECT_COPY;pp.hDeviceWindow=window;pp.BackBufferWidth=320;pp.BackBufferHeight=240;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.MultiSampleType=D3DMULTISAMPLE_TYPE(msaa);pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24X8;
        Com<IDirect3DDevice9> device;CHECK(SUCCEEDED(root->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,device.out())));
        CHECK(Frame(device,true,false,!msaa));CHECK(Frame(device,true,true,!msaa));CHECK(Frame(device,false,false,!msaa));
        CHECK(Frame(device,true,false,!msaa,true)); // Pre-world RHW passes cannot disable post effects.
        CHECK(Frame(device,true,false,!msaa,2)); // Depth-disabled 3D markers are not the world phase.
        CHECK(Frame(device,true,false,!msaa,false,true)); // Full main clears establish a new render epoch.
        CHECK(Frame(device,true,false,!msaa,0,false,true)); // Visibility queries must not end the scene.
        Com<IDirect3DTexture9> arrow;arrow.p=NativeArrow(device);
        if(arrow.p){
            CHECK(OverlayFrame(device,arrow,true,false));CHECK(OverlayFrame(device,arrow,false,false));
            CHECK(OverlayFrame(device,arrow,true,true));CHECK(OverlayFrame(device,arrow,false,true));
        }
        arrow.out();
        std::vector<DWORD> buffered;CHECK(SurfaceBatchFrame(device,0,buffered));
        unsigned reflected=0;for(int y=145;y<195;++y)for(int x=100;x<220;++x) {
            const auto p=buffered[y*320+x];reflected+=(p&255)>((p>>16)&255)+8;
        }
        printf("Native proxy buffered reflection pixels: %u\n",reflected);CHECK(reflected>20);
        CHECK((buffered[12*320+12]&0xffffff)==0x40a0e0);
        for(int mutation:{1,2,3}) {
            std::vector<DWORD> modified;CHECK(SurfaceBatchFrame(device,mutation,modified));unsigned difference=0;
            for(size_t i=0;i<buffered.size();++i)for(int shift:{0,8,16})difference=std::max(difference,
                unsigned(abs(int((buffered[i]>>shift)&255)-int((modified[i]>>shift)&255))));
            printf("Buffered surface write barrier: mutation=%d, delta=%u\n",mutation,difference);
            CHECK(difference<=(msaa?32u:1u));
        }
        CHECK(SUCCEEDED(device->Reset(&pp)));CHECK(Frame(device,true,false,!msaa));
        printf("PASS: HUD exact RGB preserved, world graded once, UI-only skipped, Reset, MSAA=%d\n",msaa);
    }
    DestroyWindow(window);return 0;
}
