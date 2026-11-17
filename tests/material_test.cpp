#include "material.hpp"
#include "water.hpp"
#include "state_guard.hpp"
#include <gdiplus.h>
#include <cstdio>
#include <cstring>

#define CHECK(x) do { if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;} } while(0)
namespace {
UINT W=384,H=256;
unsigned Delta(const std::vector<DWORD> &a,const std::vector<DWORD> &b){
    if(a.size()!=b.size()||a.empty())return 999;
    unsigned value=0;for(size_t i=0;i<a.size();++i)for(unsigned shift:{0u,8u,16u})
        value=std::max(value,unsigned(std::abs(int((a[i]>>shift)&255)-int((b[i]>>shift)&255))));
    return value;
}
std::vector<DWORD> Read(IDirect3DDevice9 *d){
    Com<IDirect3DSurface9> source,resolved,read;d->GetRenderTarget(0,source.out());
    D3DSURFACE_DESC desc{};source->GetDesc(&desc);
    if(desc.MultiSampleType!=D3DMULTISAMPLE_NONE){
        if(FAILED(d->CreateRenderTarget(W,H,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,resolved.out(),nullptr))||
           FAILED(d->StretchRect(source,nullptr,resolved,nullptr,D3DTEXF_NONE)))return {};
        source.p->Release();source.p=resolved.p;source.p->AddRef();
    }
    if(FAILED(d->CreateOffscreenPlainSurface(W,H,desc.Format,D3DPOOL_SYSTEMMEM,read.out(),nullptr))||
       FAILED(d->GetRenderTargetData(source,read)))return {};
    D3DLOCKED_RECT lock{};if(FAILED(read->LockRect(&lock,nullptr,D3DLOCK_READONLY)))return {};
    std::vector<DWORD> pixels(W*H);
    for(UINT y=0;y<H;++y)memcpy(pixels.data()+y*W,(BYTE *)lock.pBits+y*lock.Pitch,W*4);
    read->UnlockRect();return pixels;
}
bool Save(const std::vector<DWORD> &pixels,const std::wstring &path){
    if(pixels.size()!=W*H)return false;
    UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);
    std::vector<BYTE> bytes(size);auto *enc=(Gdiplus::ImageCodecInfo *)bytes.data();
    Gdiplus::GetImageEncoders(count,size,enc);CLSID png{};
    for(UINT i=0;i<count;++i)if(!wcscmp(enc[i].MimeType,L"image/png"))png=enc[i].Clsid;
    Gdiplus::Bitmap image(W,H,W*4,PixelFormat32bppRGB,(BYTE *)pixels.data());
    return image.Save(path.c_str(),&png,nullptr)==Gdiplus::Ok;
}
struct Fixture {
    IDirect3DDevice9 *d;
    MaterialFX fx;
    Com<IDirect3DTexture9> art,layer;
    bool two=false,lit=true,point=false,rangeFog=false,transform=false,used=false,scaled=false,orbit=false,zeroNormals=false;
    int fog=0,table=0,stateErrors=0;
    float repeats=1,cameraDistance=3.2f,uStart=0,uSpan=1,vStart=0,vSpan=1;
    explicit Fixture(IDirect3DDevice9 *device):d(device),fx(device){}
    std::vector<DWORD> render(const Settings &s){
        StateGuard guard(d);if(!guard.valid())return {};
        guard.isolateTargets();
        D3DVIEWPORT9 viewport{0,0,W,H,0,1};d->SetViewport(&viewport);
        d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff293441,1,0);
        Com<IDirect3DSurface9> depth;
        D3DSURFACE_DESC desc{};Com<IDirect3DSurface9> target;d->GetRenderTarget(0,target.out());target->GetDesc(&desc);
        if(FAILED(d->CreateDepthStencilSurface(W,H,D3DFMT_D24X8,desc.MultiSampleType,0,TRUE,depth.out(),nullptr)))return {};
        d->SetDepthStencilSurface(depth);d->Clear(0,nullptr,D3DCLEAR_ZBUFFER,0,1,0);
        D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
        auto projection=identity;projection._11=1.35f;projection._22=1.8f;
        projection._33=100.f/99.9f;projection._34=1;projection._43=-.1f*projection._33;projection._44=0;
        auto world=identity,view=identity;
        if(scaled){world._11=.83f;world._22=1.12f;world._33=.94f;}
        if(orbit){view._11=view._33=std::cos(.16f);view._13=std::sin(.16f);view._31=-view._13;}
        d->SetTransform(D3DTS_WORLD,&world);d->SetTransform(D3DTS_VIEW,&view);d->SetTransform(D3DTS_PROJECTION,&projection);
        d->SetVertexShader(nullptr);d->SetPixelShader(nullptr);d->SetFVF(two?0x212:0x112);
        for(auto pair:{std::pair{D3DRS_ZENABLE,DWORD(TRUE)},std::pair{D3DRS_ZWRITEENABLE,DWORD(TRUE)},
            std::pair{D3DRS_ALPHATESTENABLE,DWORD(FALSE)},std::pair{D3DRS_ALPHABLENDENABLE,DWORD(two)},
            std::pair{D3DRS_SRCBLEND,DWORD(D3DBLEND_SRCALPHA)},std::pair{D3DRS_DESTBLEND,DWORD(D3DBLEND_INVSRCALPHA)},
            std::pair{D3DRS_CULLMODE,DWORD(D3DCULL_NONE)},std::pair{D3DRS_LIGHTING,DWORD(lit)},
            std::pair{D3DRS_SPECULARENABLE,DWORD(FALSE)},std::pair{D3DRS_NORMALIZENORMALS,DWORD(TRUE)},
            std::pair{D3DRS_AMBIENT,DWORD(0xff454850)}})d->SetRenderState(pair.first,pair.second);
        D3DMATERIAL9 material{};material.Diffuse={.9f,.95f,1,.8f};material.Ambient={1,1,1,1};d->SetMaterial(&material);
        for(DWORD index=0;index<8;++index)d->LightEnable(index,FALSE);
        D3DLIGHT9 sun{};sun.Type=D3DLIGHT_DIRECTIONAL;sun.Diffuse={.78f,.75f,.7f,1};
        sun.Direction={.38f,-.48f,.79f};float length=std::sqrt(.38f*.38f+.48f*.48f+.79f*.79f);
        sun.Direction.x/=length;sun.Direction.y/=length;sun.Direction.z/=length;
        d->SetLight(0,&sun);d->LightEnable(0,TRUE);
        if(point){D3DLIGHT9 light{};light.Type=D3DLIGHT_POINT;light.Position={-1,1,1};light.Diffuse={.2f,.07f,.03f,1};
            light.Range=20;light.Attenuation0=1;light.Attenuation1=.1f;d->SetLight(1,&light);d->LightEnable(1,TRUE);}
        d->SetRenderState(D3DRS_FOGENABLE,fog||table);d->SetRenderState(D3DRS_FOGVERTEXMODE,fog);
        d->SetRenderState(D3DRS_FOGTABLEMODE,table);d->SetRenderState(D3DRS_RANGEFOGENABLE,rangeFog);
        d->SetRenderState(D3DRS_FOGCOLOR,0xff526881);
        const float fogValues[]={.12f,1.f,12.f};const D3DRENDERSTATETYPE keys[]={D3DRS_FOGDENSITY,D3DRS_FOGSTART,D3DRS_FOGEND};
        for(int i=0;i<3;++i){DWORD bits;memcpy(&bits,fogValues+i,4);d->SetRenderState(keys[i],bits);}
        d->SetTexture(0,art);d->SetTexture(1,layer);
        for(DWORD stage=0;stage<2;++stage){
            d->SetSamplerState(stage,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);d->SetSamplerState(stage,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
            d->SetSamplerState(stage,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);d->SetSamplerState(stage,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);d->SetSamplerState(stage,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
            d->SetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,stage);d->SetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,transform?D3DTTFF_COUNT2:D3DTTFF_DISABLE);
            auto uv=identity;uv._11=.8f;uv._22=1.2f;uv._31=.07f;uv._32=-.11f;d->SetTransform(D3DTRANSFORMSTATETYPE(D3DTS_TEXTURE0+stage),&uv);
        }
        d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);d->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
        d->SetTextureStageState(1,D3DTSS_COLOROP,two?D3DTOP_SELECTARG1:D3DTOP_DISABLE);d->SetTextureStageState(1,D3DTSS_COLORARG1,D3DTA_CURRENT);
        d->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);d->SetTextureStageState(1,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
        d->SetTextureStageState(2,D3DTSS_COLOROP,D3DTOP_DISABLE);
        struct Vertex{float x,y,z,nx,ny,nz,u,v,a,b;};
        std::vector<float> vertices;
        auto vertex=[&](float x,float y){
            float z=cameraDistance+y*.3f+.08f*std::sin(x*3),nx=.24f*std::cos(x*3),ny=.3f,nz=-1;
            float length=std::sqrt(nx*nx+ny*ny+1);Vertex v{x,y,z,nx/length,ny/length,nz/length,uStart+(x+1.8f)/3.6f*repeats*uSpan,vStart+(1.2f-y)/2.4f*repeats*vSpan,(x+1.8f)/3.6f,(1.2f-y)/2.4f};
            if(zeroNormals)v.nx=v.ny=v.nz=0;
            auto *data=&v.x;vertices.insert(vertices.end(),data,data+(two?10:8));
        };
        for(int y=0;y<12;++y)for(int x=0;x<20;++x){float l=-1.8f+x*3.6f/20,r=l+3.6f/20,t=1.2f-y*2.4f/12,b=t-2.4f/12;
            vertex(l,t);vertex(r,t);vertex(l,b);vertex(l,b);vertex(r,t);vertex(r,b);}
        if(FAILED(d->BeginScene()))return {};
        float beforeVS[55][4]{},beforePS[14][4]{},afterVS[55][4]{},afterPS[14][4]{};
        d->GetVertexShaderConstantF(0,beforeVS[0],55);d->GetPixelShaderConstantF(0,beforePS[0],14);
        used=fx.begin(s);
        auto hr=d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,20*12*2,vertices.data(),(two?10:8)*sizeof(float));
        fx.end();
        d->GetVertexShaderConstantF(0,afterVS[0],55);d->GetPixelShaderConstantF(0,afterPS[0],14);
        stateErrors+=memcmp(beforeVS,afterVS,sizeof(beforeVS))!=0||memcmp(beforePS,afterPS,sizeof(beforePS))!=0;
        DWORD restored=0;d->GetRenderState(D3DRS_FOGENABLE,&restored);stateErrors+=restored!=DWORD(bool(fog||table));
        d->EndScene();if(FAILED(hr))return {};
        return Read(d);
    }
};
}
int main(int argc,char **argv){
    bool preview=argc>2&&!strcmp(argv[1],"--assets"),msaa=argc>1&&!strcmp(argv[1],"--msaa");
    if(preview){W=1000;H=700;CreateDirectoryW(L"artifacts/materials",nullptr);}
    Gdiplus::GdiplusStartupInput startup;ULONG_PTR token=0;Gdiplus::GdiplusStartup(&token,&startup,nullptr);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FENBMaterials";RegisterClassW(&wc);
    HWND window=CreateWindowW(wc.lpszClassName,L"FENB material fixture",WS_OVERLAPPEDWINDOW,0,0,W,H,nullptr,nullptr,wc.hInstance,nullptr);
    Com<IDirect3D9> root;root.p=Direct3DCreate9(D3D_SDK_VERSION);CHECK(root.p);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.hDeviceWindow=window;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth=W;pp.BackBufferHeight=H;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.MultiSampleType=msaa?D3DMULTISAMPLE_8_SAMPLES:D3DMULTISAMPLE_NONE;
    if(msaa&&FAILED(root->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,pp.BackBufferFormat,TRUE,pp.MultiSampleType,nullptr))){puts("SKIP: 8x MSAA unavailable");return 0;}
    Com<IDirect3DDevice9> d;CHECK(SUCCEEDED(root->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,d.out())));
    Fixture fixture(d);
    Settings off,on;on.materials=true;
    if(preview){
        HMODULE module=LoadLibraryExW(L"d3dx9_43.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);CHECK(module);
        using Load=HRESULT(WINAPI *)(IDirect3DDevice9 *,LPCWSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,DWORD,DWORD,D3DCOLOR,void *,PALETTEENTRY *,IDirect3DTexture9 **);
        auto load=reinterpret_cast<Load>(GetProcAddress(module,"D3DXCreateTextureFromFileExW"));CHECK(load);
        const char *files[]={"World/Texture/ground_n_ice02.dds","World/Texture/brick_MaFl01_new.dds","Model/Texture/Ctrl_HouFurCabWood01.dds","World/Texture/Bahara_Sand01_01.dds","World/Texture/Rartesia_Grass01.dds"};
        const wchar_t *names[]={L"snow",L"stone",L"wood",L"sand",L"grass"};
        for(int kind=0;kind<5;++kind){std::string path=std::string(argv[2])+"/"+files[kind];std::wstring wide(path.begin(),path.end());
            CHECK(SUCCEEDED(load(d,wide.c_str(),UINT(-1),UINT(-1),0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,1,1,0,nullptr,nullptr,fixture.art.out())));
            fixture.uStart=kind==2?.04f:0;fixture.uSpan=kind==2?.28f:1;
            fixture.vStart=kind==2?.1f:0;fixture.vSpan=kind==2?.8f:1;
            for(int distance=0;distance<2;++distance){fixture.repeats=.85f;fixture.cameraDistance=distance?6.4f:3.2f;
                auto a=fixture.render(off),b=fixture.render(on);CHECK(fixture.used&&Delta(a,b)>5);
                std::wstring prefix=std::wstring(L"artifacts/materials/")+names[kind]+(distance?L"-wide":L"-close");
                CHECK(Save(a,prefix+L"-off.png")&&Save(b,prefix+L"-on.png"));
                printf("Material %d distance %d: maximum RGB delta=%u\n",kind,distance,Delta(a,b));
            }
        }
        CHECK(!fixture.stateErrors);puts("PASS: original client artwork rendered through the actual material path; no replacement textures.");return 0;
    }
    CHECK(SUCCEEDED(d->CreateTexture(64,64,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,fixture.art.out(),nullptr)));
    CHECK(SUCCEEDED(d->CreateTexture(8,8,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,fixture.layer.out(),nullptr)));
    for(auto *texture:{fixture.art.p,fixture.layer.p}){D3DSURFACE_DESC desc{};texture->GetLevelDesc(0,&desc);D3DLOCKED_RECT lock{};CHECK(SUCCEEDED(texture->LockRect(0,&lock,nullptr,0)));
        for(UINT y=0;y<desc.Height;++y)for(UINT x=0;x<desc.Width;++x){int grain=int(20*std::sin(x*.5)*std::cos(y*.7));((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=D3DCOLOR_ARGB(128,110+grain,125+grain,145+grain);}texture->UnlockRect(0);}
    DWORD kind=2;fixture.art->SetPrivateData(MaterialTag,&kind,4,0);
    Settings neutral=on;neutral.materialRelief=neutral.materialGlints=neutral.materialSnow=0;
    for(bool lit:{false,true})for(bool layer:{false,true})for(int fog:{0,1,2,3}){
        fixture.lit=lit;fixture.two=layer;fixture.fog=fog;fixture.point=lit;fixture.transform=layer;fixture.rangeFog=fog==2;
        auto a=fixture.render(off),b=fixture.render(neutral);
        printf("Native parity lit=%d layered=%d fog=%d: %u\n",lit,layer,fog,Delta(a,b));
        CHECK(fixture.used&&Delta(a,b)<=2);
    }
    fixture.fog=0;fixture.table=3;fixture.two=false;fixture.transform=false;fixture.point=false;
    CHECK(Delta(fixture.render(off),fixture.render(neutral))<=2);
    fixture.table=0;
    fixture.scaled=fixture.orbit=true;fixture.point=true;
    CHECK(Delta(fixture.render(off),fixture.render(neutral))<=2);
    fixture.scaled=fixture.orbit=false;fixture.point=false;
    fixture.zeroNormals=true;CHECK(Delta(fixture.render(off),fixture.render(neutral))<=2);fixture.zeroNormals=false;
    for(DWORD type=1;type<=5;++type){fixture.art->SetPrivateData(MaterialTag,&type,4,0);auto a=fixture.render(off),b=fixture.render(on);CHECK(fixture.used&&Delta(a,b)>5);if(type==2)CHECK(Delta(a,b)>20);CHECK(Delta(b,fixture.render(on))<=(msaa?2u:0u));}
    on.materialStrength=0;auto a=fixture.render(off),b=fixture.render(on);
    CHECK(!fixture.used&&Delta(a,b)<=(msaa?2u:0u));
    on.materialStrength=.75f;InvalidateWaterTag(fixture.art);fixture.render(on);CHECK(!fixture.used);
    kind=2;fixture.art->SetPrivateData(MaterialTag,&kind,4,0);fixture.fx.invalidate();fixture.render(on);CHECK(fixture.used);
    CHECK(!fixture.stateErrors);
    puts("PASS: surface materials, fixed-function parity, directional/point lighting, blend coverage, fog, UV transforms, zero/off, identity invalidation, state restoration and recreation.");
    return 0;
}
