#include "material.hpp"
#include "water.hpp"
#include "state_guard.hpp"
#include <gdiplus.h>
#include <cstdio>
#include <cstring>
#include <array>

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
    Com<IDirect3DTexture9> art,layer,shadow;
    bool two=false,lit=true,point=false,rangeFog=false,transform=false,used=false,scaled=false,orbit=false,zeroNormals=false;
    int fog=0,table=0,stateErrors=0;
    bool third=false,passThrough=false;
    bool flaris=false;
    float winterShift=0;
    int shadowCoordinates=0;
    bool followingNative=false,buffered=false,absolute=false,coplanar=false,cutout=false,foliageBlend=false,depthWrite=true;
    D3DFORMAT depthFormat=D3DFMT_D24X8;
    float repeats=1,cameraDistance=3.2f,uStart=0,uSpan=1,vStart=0,vSpan=1,sunSide=1;
    explicit Fixture(IDirect3DDevice9 *device):d(device),fx(device){}
    std::vector<DWORD> render(const Settings &s){
        StateGuard guard(d);if(!guard.valid())return {};
        guard.isolateTargets();
        D3DVIEWPORT9 viewport{0,0,W,H,0,1};d->SetViewport(&viewport);
        d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff293441,1,0);
        Com<IDirect3DSurface9> depth;
        D3DSURFACE_DESC desc{};Com<IDirect3DSurface9> target;d->GetRenderTarget(0,target.out());target->GetDesc(&desc);
        if(FAILED(d->CreateDepthStencilSurface(W,H,depthFormat,desc.MultiSampleType,0,TRUE,depth.out(),nullptr)))return {};
        d->SetDepthStencilSurface(depth);d->Clear(0,nullptr,D3DCLEAR_ZBUFFER,0,1,0);
        D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
        auto projection=identity;projection._11=1.35f;projection._22=1.8f;
        projection._33=100.f/99.9f;projection._34=1;projection._43=-.1f*projection._33;projection._44=0;
        auto world=identity,view=identity;
        if(absolute){
            const float cy=std::cos(.43f),sy=std::sin(.43f),cp=std::cos(.67f),sp=std::sin(.67f);
            view._11=cy;view._12=-sy*sp;view._13=sy*cp;
            view._21=0;view._22=cp;view._23=sp;
            view._31=-sy;view._32=-cy*sp;view._33=cy*cp;
            for(int axis=0;axis<3;++axis)view.m[3][axis]=-(8192*view.m[0][axis]+128*view.m[1][axis]+4096*view.m[2][axis]);
        }
        if(scaled){world._11=.83f;world._22=1.12f;world._33=.94f;}
        if(flaris){
            // Keep the same camera-space quad while placing it across many world-space cells.
            world._11=world._22=world._33=18.f;
            world._41=7000.f+winterShift;world._43=3410.f;
            view._41=-world._41;view._43=-world._43;
        }
        if(orbit){view._11=view._33=std::cos(.16f);view._13=std::sin(.16f);view._31=-view._13;}
        d->SetTransform(D3DTS_WORLD,&world);d->SetTransform(D3DTS_VIEW,&view);d->SetTransform(D3DTS_PROJECTION,&projection);
        d->SetVertexShader(nullptr);d->SetPixelShader(nullptr);d->SetFVF(two?0x212:0x112);
        for(auto pair:{std::pair{D3DRS_ZENABLE,DWORD(TRUE)},std::pair{D3DRS_ZWRITEENABLE,DWORD(depthWrite)},std::pair{D3DRS_ZFUNC,DWORD(D3DCMP_LESSEQUAL)},
            std::pair{D3DRS_ALPHATESTENABLE,DWORD(cutout)},std::pair{D3DRS_ALPHABLENDENABLE,DWORD(two||foliageBlend)},
            std::pair{D3DRS_ALPHAFUNC,DWORD(D3DCMP_GREATER)},std::pair{D3DRS_ALPHAREF,DWORD(100)},
            std::pair{D3DRS_SRCBLEND,DWORD(D3DBLEND_SRCALPHA)},std::pair{D3DRS_DESTBLEND,DWORD(D3DBLEND_INVSRCALPHA)},
            std::pair{D3DRS_CULLMODE,DWORD(D3DCULL_NONE)},std::pair{D3DRS_LIGHTING,DWORD(lit)},
            std::pair{D3DRS_SPECULARENABLE,DWORD(FALSE)},std::pair{D3DRS_NORMALIZENORMALS,DWORD(TRUE)},
            std::pair{D3DRS_AMBIENT,DWORD(0xff454850)}})d->SetRenderState(pair.first,pair.second);
        D3DMATERIAL9 material{};material.Diffuse={.9f,.95f,1,.8f};material.Ambient={1,1,1,1};d->SetMaterial(&material);
        for(DWORD index=0;index<8;++index)d->LightEnable(index,FALSE);
        D3DLIGHT9 sun{};sun.Type=D3DLIGHT_DIRECTIONAL;sun.Diffuse={.78f,.75f,.7f,1};
        sun.Direction={.38f*sunSide,-.48f,.79f};float length=std::sqrt(.38f*.38f+.48f*.48f+.79f*.79f);
        sun.Direction.x/=length;sun.Direction.y/=length;sun.Direction.z/=length;
        d->SetLight(0,&sun);d->LightEnable(0,TRUE);
        if(point){D3DLIGHT9 light{};light.Type=D3DLIGHT_POINT;light.Position={-1,1,1};light.Diffuse={.2f,.07f,.03f,1};
            light.Range=20;light.Attenuation0=1;light.Attenuation1=.1f;d->SetLight(1,&light);d->LightEnable(1,TRUE);}
        d->SetRenderState(D3DRS_FOGENABLE,fog||table);d->SetRenderState(D3DRS_FOGVERTEXMODE,fog);
        d->SetRenderState(D3DRS_FOGTABLEMODE,table);d->SetRenderState(D3DRS_RANGEFOGENABLE,rangeFog);
        d->SetRenderState(D3DRS_FOGCOLOR,0xff526881);
        const float fogValues[]={.12f,1.f,12.f};const D3DRENDERSTATETYPE keys[]={D3DRS_FOGDENSITY,D3DRS_FOGSTART,D3DRS_FOGEND};
        for(int i=0;i<3;++i){DWORD bits;memcpy(&bits,fogValues+i,4);d->SetRenderState(keys[i],bits);}
        d->SetTexture(0,art);d->SetTexture(1,passThrough?nullptr:layer.p);d->SetTexture(2,shadow);
        for(DWORD stage=0;stage<3;++stage){
            d->SetSamplerState(stage,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);d->SetSamplerState(stage,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
            d->SetSamplerState(stage,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);d->SetSamplerState(stage,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);d->SetSamplerState(stage,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
            d->SetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,stage);d->SetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,transform?D3DTTFF_COUNT2:D3DTTFF_DISABLE);
            auto uv=identity;uv._11=.8f;uv._22=1.2f;uv._31=.07f;uv._32=-.11f;d->SetTransform(D3DTRANSFORMSTATETYPE(D3DTS_TEXTURE0+stage),&uv);
        }
        d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);d->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
        d->SetTextureStageState(1,D3DTSS_COLOROP,(two||passThrough)?D3DTOP_SELECTARG1:D3DTOP_DISABLE);d->SetTextureStageState(1,D3DTSS_COLORARG1,D3DTA_CURRENT);
        d->SetTextureStageState(1,D3DTSS_ALPHAOP,passThrough?D3DTOP_DISABLE:D3DTOP_SELECTARG1);d->SetTextureStageState(1,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
        d->SetTextureStageState(2,D3DTSS_COLOROP,third?D3DTOP_MODULATE:D3DTOP_DISABLE);
        d->SetTextureStageState(2,D3DTSS_COLORARG1,D3DTA_CURRENT);d->SetTextureStageState(2,D3DTSS_COLORARG2,D3DTA_TEXTURE);
        d->SetTextureStageState(2,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);d->SetTextureStageState(2,D3DTSS_ALPHAARG1,D3DTA_CURRENT);
        d->SetTextureStageState(3,D3DTSS_COLOROP,D3DTOP_DISABLE);
        auto shadowMatrix=identity;shadowMatrix._11=.18f;shadowMatrix._22=.24f;shadowMatrix._31=.17f;shadowMatrix._32=.23f;
        if(shadowCoordinates>=2){shadowMatrix._31=shadowMatrix._32=0;shadowMatrix._41=.5f;shadowMatrix._42=.5f;}
        if(shadowCoordinates==3){shadowMatrix._13=.03f;shadowMatrix._23=.02f;shadowMatrix._33=.05f;shadowMatrix._43=1;}
        if(shadowCoordinates==4){shadowMatrix._14=.03f;shadowMatrix._24=.02f;shadowMatrix._34=.05f;shadowMatrix._44=1;}
        d->SetTransform(D3DTS_TEXTURE2,&shadowMatrix);
        d->SetTextureStageState(2,D3DTSS_TEXCOORDINDEX,shadowCoordinates>=2?D3DTSS_TCI_CAMERASPACEPOSITION:shadowCoordinates);
        d->SetTextureStageState(2,D3DTSS_TEXTURETRANSFORMFLAGS,shadowCoordinates==0?D3DTTFF_DISABLE:
            shadowCoordinates==3?D3DTTFF_COUNT3|D3DTTFF_PROJECTED:shadowCoordinates==4?D3DTTFF_COUNT4|D3DTTFF_PROJECTED:D3DTTFF_COUNT2);
        struct Vertex{float x,y,z,nx,ny,nz,u,v,a,b;};
        std::vector<float> vertices;
        auto vertex=[&](float x,float y){
            float z=cameraDistance+y*.3f+.08f*std::sin(x*3),nx=.24f*std::cos(x*3),ny=.3f,nz=-1;
            float length=std::sqrt(nx*nx+ny*ny+1);Vertex v{x,y,z,nx/length,ny/length,nz/length,uStart+(x+1.8f)/3.6f*repeats*uSpan,vStart+(1.2f-y)/2.4f*repeats*vSpan,(x+1.8f)/3.6f,(1.2f-y)/2.4f};
            if(absolute){
                const float position[]={v.x-view._41,v.y-view._42,v.z-view._43},normal[]={v.nx,v.ny,v.nz};
                for(int axis=0;axis<3;++axis){
                    (&v.x)[axis]=position[0]*view.m[axis][0]+position[1]*view.m[axis][1]+position[2]*view.m[axis][2];
                    (&v.nx)[axis]=normal[0]*view.m[axis][0]+normal[1]*view.m[axis][1]+normal[2]*view.m[axis][2];
                }
            }
            if(zeroNormals)v.nx=v.ny=v.nz=0;
            auto *data=&v.x;vertices.insert(vertices.end(),data,data+(two?10:8));
        };
        for(int y=0;y<12;++y)for(int x=0;x<20;++x){float l=-1.8f+x*3.6f/20,r=l+3.6f/20,t=1.2f-y*2.4f/12,b=t-2.4f/12;
            vertex(l,t);vertex(r,t);vertex(l,b);vertex(l,b);vertex(r,t);vertex(r,b);}
        if(FAILED(d->BeginScene()))return {};
        Com<IDirect3DVertexBuffer9> vb;Com<IDirect3DIndexBuffer9> ib;
        const UINT stride=(two?10:8)*sizeof(float),count=UINT(vertices.size()*sizeof(float)/stride);
        if(buffered){
            if(FAILED(d->CreateVertexBuffer(UINT(vertices.size()*sizeof(float)),D3DUSAGE_WRITEONLY,0,D3DPOOL_MANAGED,vb.out(),nullptr))||
               FAILED(d->CreateIndexBuffer(count*2,D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_MANAGED,ib.out(),nullptr)))return {};
            void *data=nullptr;vb->Lock(0,0,&data,0);memcpy(data,vertices.data(),vertices.size()*sizeof(float));vb->Unlock();
            ib->Lock(0,0,&data,0);for(UINT index=0;index<count;++index)static_cast<WORD *>(data)[index]=WORD(index);ib->Unlock();
            d->SetStreamSource(0,vb,0,stride);d->SetIndices(ib);
        }
        auto draw=[&](){return buffered?d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,count,0,20*12*2):
            d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,20*12*2,vertices.data(),stride);};
        DWORD beforeFVF=0;d->GetFVF(&beforeFVF);
        Com<IDirect3DVertexDeclaration9> beforeDecl;d->GetVertexDeclaration(beforeDecl.out());
        struct Extra {DWORD index,transform,wrap;D3DMATRIX matrix;};
        auto coordinates=[&](){std::array<Extra,2> result{};for(int index=0;index<2;++index){
            const DWORD stage=3+index;auto &v=result[index];
            d->GetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,&v.index);d->GetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,&v.transform);
            d->GetRenderState(D3DRENDERSTATETYPE(D3DRS_WRAP3+index),&v.wrap);d->GetTransform(D3DTRANSFORMSTATETYPE(D3DTS_TEXTURE0+stage),&v.matrix);
        }return result;};
        for(int index=0;index<2;++index){
            d->SetTextureStageState(3+index,D3DTSS_TEXCOORDINDEX,index);d->SetTextureStageState(3+index,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_COUNT2);
            d->SetRenderState(D3DRENDERSTATETYPE(D3DRS_WRAP3+index),index?D3DWRAP_V:D3DWRAP_U);
            auto matrix=identity;matrix._11=.7f;matrix._31=.17f;d->SetTransform(D3DTRANSFORMSTATETYPE(D3DTS_TEXTURE3+index),&matrix);
        }
        const auto beforeCoordinates=coordinates();
        d->SetTexture(3,shadow);
        constexpr D3DSAMPLERSTATETYPE samplerKeys[]={D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,
            D3DSAMP_MIPFILTER,D3DSAMP_SRGBTEXTURE,D3DSAMP_MAXMIPLEVEL,D3DSAMP_MIPMAPLODBIAS};
        const DWORD samplerValues[]={D3DTADDRESS_CLAMP,D3DTADDRESS_MIRROR,D3DTEXF_POINT,D3DTEXF_POINT,D3DTEXF_POINT,TRUE,2,0x3e800000};
        for(size_t i=0;i<std::size(samplerKeys);++i)d->SetSamplerState(3,samplerKeys[i],samplerValues[i]);
        float beforeVS[96][4]{},beforePS[17][4]{},afterVS[96][4]{},afterPS[17][4]{};
        d->GetVertexShaderConstantF(0,beforeVS[0],96);d->GetPixelShaderConstantF(0,beforePS[0],17);
        if(coplanar){
            d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_TFACTOR);d->SetRenderState(D3DRS_TEXTUREFACTOR,0xff202020);
            draw();d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);d->SetRenderState(D3DRS_ZFUNC,D3DCMP_EQUAL);
        }
        used=fx.begin(s,flaris);
        if(used){Com<IDirect3DBaseTexture9> detail;d->GetTexture(3,detail.out());stateErrors+=shadow.p&&detail.p==shadow.p;}
        auto hr=draw();
        fx.end();
        DWORD restoredWrite=0,restoredTest=0,restoredBlend=0;
        d->GetRenderState(D3DRS_ZWRITEENABLE,&restoredWrite);d->GetRenderState(D3DRS_ALPHATESTENABLE,&restoredTest);d->GetRenderState(D3DRS_ALPHABLENDENABLE,&restoredBlend);
        stateErrors+=restoredWrite!=DWORD(depthWrite)||restoredTest!=DWORD(cutout)||restoredBlend!=DWORD(two||foliageBlend);
        Com<IDirect3DBaseTexture9> restoredTexture;d->GetTexture(3,restoredTexture.out());stateErrors+=restoredTexture.p!=shadow.p;
        for(size_t i=0;i<std::size(samplerKeys);++i){DWORD value=0;d->GetSamplerState(3,samplerKeys[i],&value);stateErrors+=value!=samplerValues[i];}
        const auto afterCoordinates=coordinates();stateErrors+=memcmp(beforeCoordinates.data(),afterCoordinates.data(),sizeof(beforeCoordinates))!=0;
        DWORD afterFVF=0;d->GetFVF(&afterFVF);Com<IDirect3DVertexDeclaration9> afterDecl;d->GetVertexDeclaration(afterDecl.out());
        if(beforeFVF!=afterFVF||beforeDecl.p!=afterDecl.p){printf("Input state changed: FVF %lx -> %lx declaration %p -> %p\n",beforeFVF,afterFVF,beforeDecl.p,afterDecl.p);++stateErrors;}
        if(followingNative){d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff293441,1,0);hr=draw();}
        d->GetVertexShaderConstantF(0,afterVS[0],96);d->GetPixelShaderConstantF(0,afterPS[0],17);
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
        const char *files[]={"World/Texture/ground_n_ice02.dds","World/Texture/brick_MaFl01_new.dds","Model/Texture/Ctrl_HouFurCabWood01.dds","World/Texture/Bahara_Sand01_01.dds","World/Texture/Rartesia_Grass01.dds","Model/Texture/Obj_AzriaTree01-B2.dds","Model/Texture/Obj_AzriaTree01-1.dds","Model/Texture/Obj_AzriaTree05-1.dds","Model/Texture/Obj_AzriaTree06-2.dds"};
        const wchar_t *names[]={L"snow",L"stone",L"wood",L"sand",L"grass",L"bark",L"snow-needles",L"snow-canopy",L"snow-branches"};
        on.materialStrength=on.materialRelief=on.materialSnow=on.materialGlints=1;
        for(int kind=0;kind<9;++kind){std::string path=std::string(argv[2])+"/"+files[kind];std::wstring wide(path.begin(),path.end());
            CHECK(SUCCEEDED(load(d,wide.c_str(),UINT(-1),UINT(-1),0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,1,1,0,nullptr,nullptr,fixture.art.out())));
            fixture.cutout=kind>=6;
            fixture.uStart=kind==2?.04f:0;fixture.uSpan=kind==2?.28f:1;
            fixture.vStart=kind==2?.1f:0;fixture.vSpan=kind==2?.8f:1;
            for(int distance=0;distance<2;++distance){fixture.repeats=.85f;fixture.cameraDistance=distance?6.4f:3.2f;
                auto a=fixture.render(off),b=fixture.render(on);CHECK(fixture.used&&Delta(a,b)>0);
                std::wstring prefix=std::wstring(L"artifacts/materials/")+names[kind]+(distance?L"-wide":L"-close");
                CHECK(Save(a,prefix+L"-off.png")&&Save(b,prefix+L"-on.png"));
                Settings flat=on;flat.materialRelief=0;
                CHECK(Save(fixture.render(flat),prefix+L"-relief0.png"));
                printf("Material %d distance %d: maximum RGB delta=%u\n",kind,distance,Delta(a,b));
            }
        }
        CHECK(!fixture.stateErrors);puts("PASS: original client artwork rendered through the actual material path; no replacement textures.");return 0;
    }
    CHECK(SUCCEEDED(d->CreateTexture(64,64,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,fixture.art.out(),nullptr)));
    CHECK(SUCCEEDED(d->CreateTexture(8,8,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,fixture.layer.out(),nullptr)));
    CHECK(SUCCEEDED(d->CreateTexture(8,8,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,fixture.shadow.out(),nullptr)));
    for(auto *texture:{fixture.art.p,fixture.layer.p,fixture.shadow.p}){D3DSURFACE_DESC desc{};texture->GetLevelDesc(0,&desc);D3DLOCKED_RECT lock{};CHECK(SUCCEEDED(texture->LockRect(0,&lock,nullptr,0)));
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
    fixture.followingNative=true;
    for(bool buffered:{false,true})for(bool two:{false,true}){
        fixture.buffered=buffered;fixture.two=two;
        auto a=fixture.render(off),b=fixture.render(on);
        printf("Following native draw buffered=%d terrain=%d: %u\n",buffered,two,Delta(a,b));
        CHECK(Delta(a,b)<=(msaa?2u:0u));
    }
    fixture.followingNative=fixture.buffered=fixture.two=false;
    fixture.absolute=fixture.coplanar=true;
    for(auto format:{D3DFMT_D16,D3DFMT_D24X8})for(bool buffered:{false,true})for(bool two:{false,true})for(int fog:{0,3}){
        fixture.depthFormat=format;fixture.buffered=buffered;fixture.two=fixture.third=two;fixture.shadowCoordinates=3;fixture.fog=fog;
        auto a=fixture.render(off),b=fixture.render(neutral);
        printf("Absolute coplanar layers depth=%d buffered=%d terrain=%d fog=%d: %u\n",int(format),buffered,two,fog,Delta(a,b));
        CHECK(fixture.used&&Delta(a,b)<=2);
    }
    fixture.absolute=fixture.coplanar=fixture.buffered=fixture.two=fixture.third=false;
    fixture.depthFormat=D3DFMT_D24X8;fixture.shadowCoordinates=fixture.fog=0;
    // Native terrain combines artwork, blend coverage and a third projected mask.
    // It must receive styling without losing that mask or its blend-map alpha.
    kind=1;fixture.art->SetPrivateData(MaterialTag,&kind,4,0);
    fixture.two=fixture.third=true;
    for(int coordinates=0;coordinates<5;++coordinates)for(int fog:{0,3}){
        fixture.shadowCoordinates=coordinates;fixture.fog=fog;
        auto a=fixture.render(off),b=fixture.render(neutral),styled=fixture.render(on);
        printf("Three-stage terrain coordinates=%d fog=%d: native delta=%u styled delta=%u\n",coordinates,fog,Delta(a,b),Delta(a,styled));
        CHECK(fixture.used&&Delta(a,b)<=2&&Delta(a,styled)>0);
    }
    // Direct3D stops at the first disabled stage, even if later state is left enabled.
    fixture.two=false;
    CHECK(Delta(fixture.render(off),fixture.render(neutral))<=2&&fixture.used);
    fixture.two=fixture.third=false;fixture.fog=fixture.shadowCoordinates=0;
    for(DWORD type=1;type<=5;++type){
        fixture.art->SetPrivateData(MaterialTag,&type,4,0);
        auto a=fixture.render(off),b=fixture.render(on);
        CHECK(fixture.used&&Delta(a,b)>0);
        // Ordinary painted materials retain their tones instead of acquiring deep grooves.
        if(type!=1)CHECK(Delta(a,b)<=12);
        CHECK(Delta(b,fixture.render(on))<=(msaa?2u:0u));
    }
    // Maximum snow correction compresses the painted mottling but retains a
    // readable brightness pattern, rather than replacing it with a flat fill.
    D3DLOCKED_RECT snowLock{};CHECK(SUCCEEDED(fixture.art->LockRect(0,&snowLock,nullptr,0)));
    for(UINT y=0;y<64;++y)for(UINT x=0;x<64;++x){int grain=int(20*std::sin(x*.5)*std::cos(y*.7));
        ((DWORD *)((BYTE *)snowLock.pBits+y*snowLock.Pitch))[x]=D3DCOLOR_ARGB(128,180+grain,195+grain,220+grain);}
    fixture.art->UnlockRect(0);DWORD snowKind=1;fixture.art->SetPrivateData(MaterialTag,&snowKind,4,0);
    Settings snowStyle=on;snowStyle.materialStrength=snowStyle.materialSnow=1;snowStyle.materialRelief=snowStyle.materialGlints=0;
    auto sourceSnow=fixture.render(off),correctedSnow=fixture.render(snowStyle);
    auto energy=[&](const std::vector<DWORD> &pixels){
        auto luma=[](DWORD p){return .2126*((p>>16)&255)+.7152*((p>>8)&255)+.0722*(p&255);};double sum=0;
        for(UINT y=H/3;y<2*H/3;++y)for(UINT x=W/3;x<2*W/3;++x){const auto i=y*W+x;
            sum+=std::abs(luma(pixels[i])-luma(pixels[i+1]))+std::abs(luma(pixels[i])-luma(pixels[i+W]));}
        return sum;
    };
    const double sourceEnergy=energy(sourceSnow),correctedEnergy=energy(correctedSnow);
    printf("Snow luminance detail retained at maximum strength: %.2f%%\n",100*correctedEnergy/sourceEnergy);
    CHECK(sourceEnergy>100&&correctedEnergy>sourceEnergy*.25&&correctedEnergy<sourceEnergy*.65&&Delta(sourceSnow,correctedSnow)>0);
    // Relief must visibly change lighting, not merely the snow's color correction.
    Settings raised=snowStyle;raised.materialRelief=1;
    // Resolve the finer grain in a close crop; it must fade rather than alias at distance.
    fixture.repeats=.3f;correctedSnow=fixture.render(snowStyle);
    const auto raisedSnow=fixture.render(raised);
    printf("Snow relief 0 -> 1: RGB delta=%u\n",Delta(correctedSnow,raisedSnow));
    CHECK(Delta(correctedSnow,raisedSnow)>=1&&Delta(correctedSnow,raisedSnow)<=4);
    CHECK(Delta(raisedSnow,fixture.render(raised))<=(msaa?2u:0u));
    fixture.sunSide=-1;
    auto reverseFlat=fixture.render(snowStyle),reverseRelief=fixture.render(raised);
    double lightingResponse=0;
    for(size_t i=0;i<raisedSnow.size();++i)lightingResponse+=std::abs(
        int(raisedSnow[i]&255)-int(correctedSnow[i]&255)-int(reverseRelief[i]&255)+int(reverseFlat[i]&255));
    CHECK(lightingResponse>W*H*.02);fixture.sunSide=1;fixture.repeats=1;
    CHECK(Delta(fixture.render(snowStyle),fixture.render(raised))<=(msaa?2u:0u));
    // Only exact bark identities can opt into alpha-tested material draws. Native
    // alpha, depth coverage and a following unstyled draw must all remain intact.
    CHECK(SUCCEEDED(d->CreateTexture(64,64,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,fixture.art.out(),nullptr)));
    D3DLOCKED_RECT barkLock{};CHECK(SUCCEEDED(fixture.art->LockRect(0,&barkLock,nullptr,0)));
    for(UINT y=0;y<64;++y)for(UINT x=0;x<64;++x){int grain=int(25*std::sin(x*.8)+8*std::cos(y*.4));
        ((DWORD *)((BYTE *)barkLock.pBits+y*barkLock.Pitch))[x]=D3DCOLOR_ARGB(x*4,150+grain,100+grain,45+grain);}
    fixture.art->UnlockRect(0);DWORD barkKind=6;fixture.art->SetPrivateData(MaterialTag,&barkKind,4,0);
    for(UINT level=1;level<fixture.art->GetLevelCount();++level){
        D3DSURFACE_DESC desc{};fixture.art->GetLevelDesc(level,&desc);D3DLOCKED_RECT src{},dst{};
        CHECK(SUCCEEDED(fixture.art->LockRect(level-1,&src,nullptr,D3DLOCK_READONLY)));
        CHECK(SUCCEEDED(fixture.art->LockRect(level,&dst,nullptr,0)));
        for(UINT y=0;y<desc.Height;++y)for(UINT x=0;x<desc.Width;++x){DWORD pixel=0;
            for(unsigned shift:{0u,8u,16u,24u}){unsigned sum=0;
                for(UINT b=0;b<2;++b)for(UINT a=0;a<2;++a)sum+=(((DWORD *)((BYTE *)src.pBits+(y*2+b)*src.Pitch))[x*2+a]>>shift)&255;
                pixel|=((sum+2)/4)<<shift;}
            ((DWORD *)((BYTE *)dst.pBits+y*dst.Pitch))[x]=pixel;}
        fixture.art->UnlockRect(level);fixture.art->UnlockRect(level-1);
    }
    fixture.cutout=fixture.absolute=fixture.coplanar=true;
    auto nativeBark=fixture.render(off),neutralBark=fixture.render(neutral),styledBark=fixture.render(raised);
    const auto barkDelta=Delta(nativeBark,styledBark);
    printf("Bark relief at maximum strength: RGB delta=%u\n",barkDelta);
    CHECK(fixture.used&&Delta(nativeBark,neutralBark)<=2&&barkDelta>=6&&barkDelta<=48);
    for(size_t i=0;i<nativeBark.size();++i)CHECK((nativeBark[i]==0xff293441)==(styledBark[i]==0xff293441));
    // A client upload without mipmaps uses the softer source-gradient fallback.
    Com<IDirect3DTexture9> singleLevel;CHECK(SUCCEEDED(d->CreateTexture(64,64,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,singleLevel.out(),nullptr)));
    D3DLOCKED_RECT original{},copy{};CHECK(SUCCEEDED(fixture.art->LockRect(0,&original,nullptr,D3DLOCK_READONLY)));
    CHECK(SUCCEEDED(singleLevel->LockRect(0,&copy,nullptr,0)));
    for(UINT y=0;y<64;++y)memcpy((BYTE *)copy.pBits+y*copy.Pitch,(BYTE *)original.pBits+y*original.Pitch,64*4);
    singleLevel->UnlockRect(0);fixture.art->UnlockRect(0);singleLevel->SetPrivateData(MaterialTag,&barkKind,4,0);
    std::swap(singleLevel.p,fixture.art.p);
    CHECK(Delta(fixture.render(off),fixture.render(neutral))<=2&&fixture.used);
    CHECK(Delta(fixture.render(off),fixture.render(raised))<=24&&fixture.used);
    std::swap(singleLevel.p,fixture.art.p);
    fixture.followingNative=true;CHECK(Delta(fixture.render(off),fixture.render(raised))<=(msaa?2u:0u));
    fixture.followingNative=fixture.absolute=fixture.coplanar=false;
    // Winter foliage is a thin alpha-masked card, not a raised bark surface.
    // Its painted snow changes while green needles, warm branches and alpha stay native.
    Com<IDirect3DTexture9> foliage;CHECK(SUCCEEDED(d->CreateTexture(64,64,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,foliage.out(),nullptr)));
    D3DLOCKED_RECT foliageLock{};CHECK(SUCCEEDED(foliage->LockRect(0,&foliageLock,nullptr,0)));
    for(UINT y=0;y<64;++y)for(UINT x=0;x<64;++x){int grain=int((x*17+y*13)%17)-8;
        const DWORD rgb=y<21?D3DCOLOR_XRGB(164+grain,195+grain,212+grain):y<42?D3DCOLOR_XRGB(40,95,36):D3DCOLOR_XRGB(120,78,28);
        ((DWORD *)((BYTE *)foliageLock.pBits+y*foliageLock.Pitch))[x]=(rgb&0xffffff)|(x*4<<24);}
    foliage->UnlockRect(0);DWORD foliageKind=7;foliage->SetPrivateData(MaterialTag,&foliageKind,4,0);
    std::swap(foliage.p,fixture.art.p);fixture.lit=false;fixture.cutout=true;
    auto nativeFoliage=fixture.render(off),styledFoliage=fixture.render(raised);
    CHECK(fixture.used&&Delta(fixture.render(off),fixture.render(neutral))<=2);
    unsigned greenPixels=0,barkPixels=0,snowPixels=0;
    for(size_t i=0;i<nativeFoliage.size();++i){DWORD a=nativeFoliage[i],b=styledFoliage[i];
        CHECK((a==0xff293441)==(b==0xff293441));
        const int red=(a>>16)&255,green=(a>>8)&255,blue=a&255;
        unsigned delta=0;for(unsigned shift:{0u,8u,16u})delta=std::max(delta,unsigned(std::abs(int((a>>shift)&255)-int((b>>shift)&255))));
        if(green>red+25&&green>blue+25){++greenPixels;CHECK(delta<=2);}
        if(red>green+25&&green>blue+25){++barkPixels;CHECK(delta<=2);}
        if(blue>red+25&&green>red+15&&delta>5)++snowPixels;
    }
    CHECK(greenPixels>100&&barkPixels>100&&snowPixels>100);
    Settings snowOnly=raised;snowOnly.materialRelief=snowOnly.materialGlints=0;
    CHECK(Delta(styledFoliage,fixture.render(snowOnly))<=(msaa?2u:0u));
    for(bool write:{false,true})for(int fog:{0,1,2,3}){
        fixture.depthWrite=write;fixture.foliageBlend=true;fixture.fog=fog;
        auto a=fixture.render(off),b=fixture.render(neutral);CHECK(fixture.used&&Delta(a,b)<=2);
        CHECK(Delta(a,fixture.render(raised))>5&&fixture.used);
    }
    fixture.fx.invalidate();CHECK(fixture.render(raised).size()==W*H&&fixture.used);
    fixture.followingNative=true;CHECK(Delta(fixture.render(off),fixture.render(raised))<=(msaa?2u:0u));
    fixture.followingNative=fixture.foliageBlend=false;fixture.depthWrite=true;fixture.fog=0;fixture.lit=true;
    std::swap(foliage.p,fixture.art.p);
    kind=2;fixture.art->SetPrivateData(MaterialTag,&kind,4,0);fixture.render(raised);CHECK(!fixture.used);
    fixture.cutout=false;
    fixture.fx.invalidate();fixture.art->SetPrivateData(MaterialTag,&snowKind,4,0);fixture.repeats=.3f;
    const auto recreatedSnow=fixture.render(raised);fixture.fx.invalidate();CHECK(Delta(recreatedSnow,fixture.render(raised))<=(msaa?2u:0u));fixture.repeats=1;
    // A uniformly painted matte surface must not acquire procedural grain or relief,
    // even at maximum relief strength. Curved geometry still receives native lighting.
    D3DLOCKED_RECT flat{};CHECK(SUCCEEDED(fixture.art->LockRect(0,&flat,nullptr,0)));
    for(UINT y=0;y<64;++y)for(UINT x=0;x<64;++x)
        ((DWORD *)((BYTE *)flat.pBits+y*flat.Pitch))[x]=D3DCOLOR_ARGB(128,110,125,145);
    fixture.art->UnlockRect(0);
    DWORD matte=2;fixture.art->SetPrivateData(MaterialTag,&matte,4,0);
    Settings flatStyle=on;flatStyle.materialRelief=flatStyle.materialStrength=1;flatStyle.materialGlints=0;
    CHECK(Delta(fixture.render(off),fixture.render(flatStyle))<=2);
    on.materialStrength=0;auto a=fixture.render(off),b=fixture.render(on);
    CHECK(!fixture.used&&Delta(a,b)<=(msaa?2u:0u));
    on.materialStrength=.75f;InvalidateWaterTag(fixture.art);fixture.render(on);CHECK(!fixture.used);
    kind=2;fixture.art->SetPrivateData(MaterialTag,&kind,4,0);fixture.fx.invalidate();fixture.render(on);CHECK(fixture.used);
    // A uniform, repeating terrain atlas must pick up variation from world position alone.
    Settings winter;winter.winterGround=true;
    CHECK(SUCCEEDED(d->CreateTexture(64,64,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,fixture.art.out(),nullptr)));
    CHECK(SUCCEEDED(fixture.art->LockRect(0,&flat,nullptr,0)));
    for(UINT y=0;y<64;++y)
        std::fill_n((DWORD*)((BYTE*)flat.pBits+y*flat.Pitch),64,D3DCOLOR_ARGB(220,107,124,73));
    fixture.art->UnlockRect(0);
    DWORD winterKind=unsigned(WinterSurface::Ground)<<8;
    fixture.art->SetPrivateData(MaterialTag,&winterKind,4,0);
    fixture.render(winter);CHECK(!fixture.used);
    fixture.flaris=true;fixture.lit=false;fixture.followingNative=false;
    fixture.cameraDistance=3.2f;fixture.two=fixture.third=fixture.coplanar=false;
    auto bare=fixture.render(off),covered=fixture.render(winter);
    printf("World snow enabled=%d maximum RGB delta=%u\n",fixture.used,Delta(bare,covered));
    CHECK(fixture.used&&Delta(bare,covered)>8);
    fixture.repeats=4;
    CHECK(Delta(covered,fixture.render(winter))<=(msaa?2u:1u));
    bare=fixture.render(off);
    fixture.winterShift=9.f;
    CHECK(Delta(bare,fixture.render(off))<=(msaa?2u:1u));
    const unsigned shift=Delta(covered,fixture.render(winter));
    printf("World snow placement delta=%u\n",shift);
    CHECK(shift>8);
    fixture.two=true;fixture.third=true;fixture.coplanar=true;
    fixture.cutout=true;
    bare=fixture.render(off);covered=fixture.render(winter);
    CHECK(fixture.used&&Delta(bare,covered)>3);
    winterKind=unsigned(WinterSurface::Plant)<<8;
    fixture.art->SetPrivateData(MaterialTag,&winterKind,4,0);
    fixture.render(winter);CHECK(!fixture.used);
    winter.winterGround=false;winter.winterProps=true;
    covered=fixture.render(winter);
    CHECK(fixture.used&&Delta(bare,covered)>8);
    // Captured Flaris props have one mesh UV. Stage 1 selects CURRENT while
    // retaining UV index 1, and stage 2 can still apply the projected shadow.
    fixture.two=fixture.coplanar=false;
    fixture.passThrough=fixture.buffered=true;
    fixture.shadowCoordinates=3;
    for(bool shadowed:{false,true})for(auto surface:{WinterSurface::Plant,WinterSurface::Exterior}){
        fixture.third=shadowed;
        winterKind=unsigned(surface)<<8;
        fixture.art->SetPrivateData(MaterialTag,&winterKind,4,0);
        bare=fixture.render(off);covered=fixture.render(winter);
        CHECK(fixture.used&&Delta(bare,covered)>8);
    }
    fixture.cutout=false;
    kind=2;fixture.art->SetPrivateData(MaterialTag,&kind,4,0);
    bare=fixture.render(off);
    CHECK(Delta(bare,fixture.render(neutral))<=2&&fixture.used);
    CHECK(!fixture.stateErrors);
    puts("PASS: surface materials, fixed-function parity, directional/point lighting, blend coverage, fog, UV transforms, zero/off, identity invalidation, state restoration and recreation.");
    return 0;
}
