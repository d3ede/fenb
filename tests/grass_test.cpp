#include "grass.hpp"
#include "grass_bounds.hpp"
#include <gdiplus.h>
#include <cstdio>
#include <cstring>
#include <filesystem>

#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
namespace {
constexpr UINT Width=1000,Height=700;
D3DMATRIX Identity(){D3DMATRIX m{};m._11=m._22=m._33=m._44=1;return m;}
struct Vertex{float x,y,z,nx,ny,nz,u,v,a,b;};
float Ground(float x,float z){return .45f*std::sin(x*.19f)+.25f*std::cos(z*.23f);}
std::vector<DWORD> Read(IDirect3DDevice9 *d){
    Com<IDirect3DSurface9> target,read,resolved;d->GetRenderTarget(0,target.out());
    D3DSURFACE_DESC desc{};target->GetDesc(&desc);
    if(desc.MultiSampleType!=D3DMULTISAMPLE_NONE){
        if(FAILED(d->CreateRenderTarget(Width,Height,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,resolved.out(),nullptr))||FAILED(d->StretchRect(target,nullptr,resolved,nullptr,D3DTEXF_NONE)))return {};
        std::swap(target.p,resolved.p);
    }
    if(FAILED(d->CreateOffscreenPlainSurface(Width,Height,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,read.out(),nullptr))||FAILED(d->GetRenderTargetData(target,read)))return {};
    D3DLOCKED_RECT lock{};if(FAILED(read->LockRect(&lock,nullptr,D3DLOCK_READONLY)))return {};
    std::vector<DWORD> p(Width*Height);for(UINT y=0;y<Height;++y)memcpy(p.data()+y*Width,(BYTE *)lock.pBits+y*lock.Pitch,Width*4);read->UnlockRect();return p;
}
bool Save(const std::vector<DWORD> &pixels,const wchar_t *path){
    UINT n=0,size=0;Gdiplus::GetImageEncodersSize(&n,&size);std::vector<BYTE> bytes(size);auto *enc=(Gdiplus::ImageCodecInfo *)bytes.data();Gdiplus::GetImageEncoders(n,size,enc);CLSID png{};
    for(UINT i=0;i<n;++i)if(!wcscmp(enc[i].MimeType,L"image/png"))png=enc[i].Clsid;
    Gdiplus::Bitmap image(Width,Height,Width*4,PixelFormat32bppRGB,(BYTE *)pixels.data());return image.Save(path,&png,nullptr)==Gdiplus::Ok;
}
unsigned Delta(const std::vector<DWORD> &a,const std::vector<DWORD> &b){unsigned result=0;
    if(a.size()!=b.size()||a.empty())return 999;
    for(size_t i=0;i<a.size();++i)for(unsigned shift:{0u,8u,16u})result=std::max(result,unsigned(std::abs(int((a[i]>>shift)&255)-int((b[i]>>shift)&255))));return result;
}
}
struct GrassFXFixture {
    static bool started(const GrassFX &g){return g.started_;}
    static bool failed(const GrassFX &g){return g.failed_;}
    static unsigned kind(GrassFX &g,IDirect3DTexture9 *texture){return g.classify(texture);}
    static UINT count(const GrassFX &g){UINT n=0;for(auto &b:g.bands_)n+=b.count;return n;}
    static std::array<float,2> center(const GrassFX &g){return {g.map_[0],g.map_[1]};}
    static size_t queued(const GrassFX &g){return g.pending_.size();}
    static std::array<float,4> surface(GrassFX &g,float x,float z){
        std::array<float,4> v{};Com<IDirect3DSurface9> read;
        if(FAILED(g.device_->CreateOffscreenPlainSurface(g.resolution_,g.resolution_,D3DFMT_A32B32G32R32F,D3DPOOL_SYSTEMMEM,read.out(),nullptr))||
            FAILED(g.device_->GetRenderTargetData(g.groundSurface_,read)))return v;
        UINT px=UINT(std::clamp((x-g.map_[0])/(2*g.map_[2])+.5f,0.f,.999f)*g.resolution_);
        UINT py=UINT(std::clamp(.5f-(z-g.map_[1])/(2*g.map_[2]),0.f,.999f)*g.resolution_);
        D3DLOCKED_RECT lock{};if(SUCCEEDED(read->LockRect(&lock,nullptr,D3DLOCK_READONLY))){memcpy(v.data(),(BYTE *)lock.pBits+py*lock.Pitch+px*16,16);read->UnlockRect();}return v;
    }
};
int main(int argc,char **argv){
    setvbuf(stdout,nullptr,_IONBF,0);
    bool msaa=false,mixed=false;const char *assets=nullptr,*proxy=nullptr;
    for(int arg=1;arg<argc;++arg){if(!strcmp(argv[arg],"--msaa"))msaa=true;else if(!strcmp(argv[arg],"--mixed"))mixed=true;else if(!strcmp(argv[arg],"--proxy")&&arg+1<argc)proxy=argv[++arg];else assets=argv[arg];}
    Gdiplus::GdiplusStartupInput startup;ULONG_PTR token;Gdiplus::GdiplusStartup(&token,&startup,nullptr);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FENBGrass";RegisterClassW(&wc);
    auto window=CreateWindowW(wc.lpszClassName,L"FENB grass",WS_OVERLAPPEDWINDOW,0,0,Width,Height,nullptr,nullptr,wc.hInstance,nullptr);
    Com<IDirect3D9> root;
    if(proxy){
        const auto dll=std::filesystem::absolute(proxy);const auto config=dll.parent_path()/L"FENB.ini";
        FILE *f=_wfopen(config.c_str(),L"wb");CHECK(f);
        fputs("[General]\nenabled=1\nopen_on_start=0\nunlock_fps=0\n[Bloom]\nbloom=0\n[Color]\ncolor=0\n[WaterFX]\nwater=0\n[SceneFX]\ngrass=1\ngrass_quality=2\ngrass_wind=0\n",f);fclose(f);
        const auto module=LoadLibraryW(dll.c_str());CHECK(module);
        using Create=IDirect3D9*(WINAPI *)(UINT);auto create=reinterpret_cast<Create>(GetProcAddress(module,"Direct3DCreate9"));CHECK(create);root.p=create(D3D_SDK_VERSION);
    }else root.p=Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(root.p);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
    pp.BackBufferWidth=Width;pp.BackBufferHeight=Height;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24X8;
    if(msaa){pp.MultiSampleType=D3DMULTISAMPLE_8_SAMPLES;CHECK(SUCCEEDED(root->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,pp.BackBufferFormat,TRUE,pp.MultiSampleType,nullptr)));}
    Com<IDirect3DDevice9> d;CHECK(SUCCEEDED(root->CreateDevice(0,D3DDEVTYPE_HAL,window,mixed?D3DCREATE_MIXED_VERTEXPROCESSING:D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,d.out())));
    if(mixed)CHECK(SUCCEEDED(d->SetSoftwareVertexProcessing(TRUE)));
    GrassFX grass(d);CHECK(grass.supported());
    if(!proxy){Com<IDirect3DVertexBuffer9> source;constexpr unsigned count=grass_bounds::Block*3;
        CHECK(SUCCEEDED(d->CreateVertexBuffer(count*sizeof(Vertex),D3DUSAGE_WRITEONLY,0,D3DPOOL_MANAGED,source.out(),nullptr)));
        void *upload=nullptr;CHECK(SUCCEEDED(source->Lock(0,0,&upload,0)));
        for(unsigned i=0;i<count;++i){Vertex v{};v.x=float(i/grass_bounds::Block)*100;v.z=0;memcpy(static_cast<BYTE *>(upload)+i*sizeof(Vertex),&v,sizeof(v));}
        grass_bounds::Write write;write.begin(source,0,0,upload,0,true);write.finish(source);CHECK(SUCCEEDED(source->Unlock()));
        const float map[]={0,0,42};const auto world=Identity();
        CHECK(!grass_bounds::outside(source,0,40,0,81,world,map));
        CHECK(grass_bounds::outside(source,0,40,81,81,world,map));
        CHECK(!grass_bounds::outside(source,0,40,-1,81,world,map));
        auto translated=world;translated._41=-100;CHECK(!grass_bounds::outside(source,0,40,81,81,translated,map));
        // Partial writes invalidate their overlapping block; no stale early cull.
        CHECK(SUCCEEDED(source->Lock(81*40,40,&upload,0)));write.begin(source,81*40,40,upload,0,true);write.finish(source);source->Unlock();
        CHECK(!grass_bounds::outside(source,0,40,81,81,world,map));
        CHECK(grass_bounds::outside(source,0,40,162,81,world,map));
        CHECK(SUCCEEDED(source->Lock(0,0,&upload,0)));write.begin(source,0,0,upload,0,false);write.finish(source);source->Unlock();
        CHECK(!grass_bounds::outside(source,0,40,162,81,world,map));}
    Com<IDirect3DTexture9> art;
    if(assets){
        auto module=LoadLibraryExW(L"d3dx9_43.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        using Load=HRESULT(WINAPI *)(IDirect3DDevice9 *,LPCWSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,DWORD,DWORD,D3DCOLOR,void *,PALETTEENTRY *,IDirect3DTexture9 **);
        auto load=reinterpret_cast<Load>(GetProcAddress(module,"D3DXCreateTextureFromFileExW"));CHECK(load);
        for(const char *name:{"ground_clay001.dds","ground_clay003.dds","ground_clay006.dds","ground_clay007.dds",
                              "ground_n_clay_lawn001.dds","rustia_lawn01.dds","ground_n_ice02.dds","ground_n_ice03.dds"}){
            const std::string file=std::string(assets)+"/World/Texture/"+name;const std::wstring native(file.begin(),file.end());
            for(auto format:{D3DFMT_UNKNOWN,D3DFMT_A8R8G8B8}){Com<IDirect3DTexture9> source;
                CHECK(SUCCEEDED(load(d,native.c_str(),UINT(-1),UINT(-1),0,0,format,D3DPOOL_MANAGED,1,1,0,nullptr,nullptr,source.out())));
                CHECK(GrassFXFixture::kind(grass,source)==(!strcmp(name,"rustia_lawn01.dds")?1u:strstr(name,"ice")?4u:3u));}
        }
        std::string path=std::string(assets)+"/World/Texture/ground_lawn001.dds";std::wstring wide(path.begin(),path.end());
        CHECK(SUCCEEDED(load(d,wide.c_str(),UINT(-1),UINT(-1),0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,1,1,0,nullptr,nullptr,art.out())));
    }else{
        CHECK(SUCCEEDED(d->CreateTexture(32,32,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,art.out(),nullptr)));
        D3DLOCKED_RECT lock{};art->LockRect(0,&lock,nullptr,0);
        for(UINT y=0;y<32;++y)for(UINT x=0;x<32;++x)((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=D3DCOLOR_ARGB(255,75+(x*17+y*13)%20,100+(x*19+y*23)%25,24);
        art->UnlockRect(0);DWORD kind=1;art->SetPrivateData(GrassTag,&kind,4,0);
    }
    std::vector<Vertex> vertices;std::vector<WORD> indices;
    constexpr int Grid=48;
    for(int z=0;z<=Grid;++z)for(int x=0;x<=Grid;++x){float wx=-30+x*60.f/Grid,wz=-12+z*70.f/Grid;
        float nx=-.45f*.19f*std::cos(wx*.19f),nz=.25f*.23f*std::sin(wz*.23f),length=std::sqrt(nx*nx+nz*nz+1);
        vertices.push_back({wx,Ground(wx,wz),wz,nx/length,1/length,nz/length,wx*.18f,wz*.18f,0,0});}
    for(int z=0;z<Grid;++z)for(int x=0;x<Grid;++x){WORD a=WORD(z*(Grid+1)+x),b=a+Grid+1;indices.insert(indices.end(),{a,b,WORD(a+1),WORD(a+1),b,WORD(b+1)});}
    Com<IDirect3DVertexBuffer9> vb;Com<IDirect3DIndexBuffer9> ib;
    const DWORD usage=D3DUSAGE_WRITEONLY|(mixed?D3DUSAGE_SOFTWAREPROCESSING:0);
    CHECK(SUCCEEDED(d->CreateVertexBuffer(UINT(vertices.size()*sizeof(Vertex)),usage,0,D3DPOOL_MANAGED,vb.out(),nullptr)));
    CHECK(SUCCEEDED(d->CreateIndexBuffer(UINT(indices.size()*2),usage,D3DFMT_INDEX16,D3DPOOL_MANAGED,ib.out(),nullptr)));
    void *data=nullptr;vb->Lock(0,0,&data,0);memcpy(data,vertices.data(),vertices.size()*sizeof(Vertex));vb->Unlock();
    ib->Lock(0,0,&data,0);memcpy(data,indices.data(),indices.size()*2);ib->Unlock();
    Com<IDirect3DVertexBuffer9> distantVB;
    CHECK(SUCCEEDED(d->CreateVertexBuffer(81*sizeof(Vertex),usage,0,D3DPOOL_MANAGED,distantVB.out(),nullptr)));
    CHECK(SUCCEEDED(distantVB->Lock(0,0,&data,0)));
    for(unsigned v=0;v<81;++v){auto vertex=vertices[v];vertex.x+=2000;memcpy(static_cast<BYTE *>(data)+v*sizeof(Vertex),&vertex,sizeof(vertex));}
    grass_bounds::Write boundsWrite;boundsWrite.begin(distantVB,0,0,data,0,true);boundsWrite.finish(distantVB);CHECK(SUCCEEDED(distantVB->Unlock()));
    const auto identity=Identity();auto view=identity;
    float cameraEye[]={0,2.3f,-5};const float player[]={0,0,8};bool followPlayer=false;
    auto camera=[&](float pitch,float yaw){
        view=identity;const float c=std::cos(pitch),s=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw);
        view._11=cy;view._12=sy*s;view._13=sy*c;
        view._21=0;view._22=c;view._23=-s;
        view._31=-sy;view._32=cy*s;view._33=cy*c;
        for(int axis=0;axis<3;++axis)
            view.m[3][axis]=-(cameraEye[0]*view.m[0][axis]+cameraEye[1]*view.m[1][axis]+cameraEye[2]*view.m[2][axis]);
    };
    camera(.16f,0);
    auto projection=identity;projection._11=1.2f;projection._22=1.2f*Width/Height;projection._33=200/199.9f;projection._34=1;projection._43=-.1f*projection._33;projection._44=0;
    Settings settings;settings.grass=true;settings.grassQuality=2;settings.grassWind=0;
    bool road=false,roadGradient=false,roadProp=false;DWORD roadAlpha=255,terrainFVF=0x212;unsigned stateErrors=0,extraDraws=0;
    bool groundTest=false,roadWrite=true,leafProp=false;float propHeight=0;
    bool distantTiles=false;size_t pendingAfterTiles=0;
    Com<IDirect3DTexture9> dirt,coverage;
    CHECK(SUCCEEDED(d->CreateTexture(8,8,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,dirt.out(),nullptr)));
    CHECK(SUCCEEDED(d->CreateTexture(8,8,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,coverage.out(),nullptr)));
    {D3DLOCKED_RECT lock{};dirt->LockRect(0,&lock,nullptr,0);for(int y=0;y<8;++y)for(int x=0;x<8;++x)((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=0xff826a40;dirt->UnlockRect(0);}
    auto render=[&](bool on){
        StateGuard outer(d);grass.beginFrame(on,followPlayer?player:nullptr);
        d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff9aaeb7,1,0);
        d->SetTransform(D3DTS_WORLD,&identity);d->SetTransform(D3DTS_VIEW,&view);d->SetTransform(D3DTS_PROJECTION,&projection);
        d->SetVertexShader(nullptr);d->SetPixelShader(nullptr);d->SetFVF(terrainFVF);
        d->SetRenderState(D3DRS_ZENABLE,TRUE);d->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);d->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
        d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);d->SetRenderState(D3DRS_ALPHATESTENABLE,groundTest);d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
        d->SetRenderState(D3DRS_LIGHTING,TRUE);d->SetRenderState(D3DRS_AMBIENT,0xff60605a);
        D3DMATERIAL9 material{};material.Diffuse={1,1,1,1};material.Ambient={1,1,1,1};d->SetMaterial(&material);
        D3DLIGHT9 light{};light.Type=D3DLIGHT_DIRECTIONAL;light.Direction={.3f,-.86f,.4f};light.Diffuse={.65f,.65f,.55f,1};d->SetLight(0,&light);d->LightEnable(0,TRUE);
        d->SetRenderState(D3DRS_FOGENABLE,FALSE);
        d->SetTexture(0,art);d->SetTexture(1,nullptr);
        d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);d->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
        for(DWORD stage=0;stage<2;++stage){d->SetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,stage);d->SetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
            d->SetSamplerState(stage,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);d->SetSamplerState(stage,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);d->SetSamplerState(stage,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);}
        d->SetStreamSource(0,vb,0,sizeof(Vertex));d->SetIndices(ib);
        d->BeginScene();const SurfaceDraw geometry{D3DPT_TRIANGLELIST,0,UINT(indices.size()/3),0,UINT(vertices.size()),0,true};
        auto draw=[&](){return d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,UINT(vertices.size()),0,UINT(indices.size()/3));};
        if(on&&!proxy)grass.capture(settings,draw,geometry);draw();
        if(on&&!proxy&&extraDraws){
            // A shallow view of the native client contains more than 2,048 terrain
            // passes. Replay small buffered tiles without changing the receiver.
            auto tile=geometry;tile.primitives=2;
            if(distantTiles){d->SetStreamSource(0,distantVB,0,sizeof(Vertex));tile.vertices=81;}
            for(unsigned pass=0;pass<extraDraws;++pass)grass.capture(settings,draw,tile);
            pendingAfterTiles=GrassFXFixture::queued(grass);
        }
        if(road){
            D3DLOCKED_RECT lock{};coverage->LockRect(0,&lock,nullptr,0);
            for(int y=0;y<8;++y)for(int x=0;x<8;++x)((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=D3DCOLOR_ARGB(roadGradient?x*255/7:roadAlpha,255,255,255);coverage->UnlockRect(0);
            std::vector<Vertex> strip;
            for(int segment=0;segment<48;++segment){float a=-12+segment*70.f/48,b=a+70.f/48;
                for(auto p:{std::pair{-1.5f,a},std::pair{-1.5f,b},std::pair{1.5f,a},std::pair{1.5f,a},std::pair{-1.5f,b},std::pair{1.5f,b}})
                    strip.push_back({p.first,Ground(p.first,p.second)+.005f+propHeight,p.second,0,1,0,p.first,p.second,roadGradient?(p.first+1.5f)/3:.5f,.5f});}
            d->SetRenderState(D3DRS_ZWRITEENABLE,roadWrite);
            d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
            DWORD tag=leafProp?5u:0u;dirt->SetPrivateData(GrassTag,&tag,4,0);
            d->SetTexture(0,dirt);d->SetTexture(1,coverage);
            d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
            d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_SELECTARG1);d->SetTextureStageState(1,D3DTSS_COLORARG1,D3DTA_CURRENT);
            d->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);d->SetTextureStageState(1,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
            if(roadProp){d->SetFVF(0x112);d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);}
            auto drawRoad=[&](){return d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(strip.size()/3),strip.data(),sizeof(Vertex));};
            if(on&&!proxy)grass.capture(settings,drawRoad);drawRoad();
        }
        if(on&&!proxy){
            if(!mixed){d->SetStreamSourceFreq(0,D3DSTREAMSOURCE_INDEXEDDATA|3);d->SetStreamSourceFreq(1,D3DSTREAMSOURCE_INSTANCEDATA|1);}
            grass.draw(settings);
            stateErrors+=d->GetSoftwareVertexProcessing()!=BOOL(mixed);
            UINT a=0,b=0;d->GetStreamSourceFreq(0,&a);d->GetStreamSourceFreq(1,&b);
            stateErrors+=a!=(mixed?1u:D3DSTREAMSOURCE_INDEXEDDATA|3)||b!=(mixed?1u:D3DSTREAMSOURCE_INSTANCEDATA|1);
            d->SetStreamSourceFreq(0,1);d->SetStreamSourceFreq(1,1);
        }
        if(proxy){
            struct Hud {float x,y,z,w;DWORD color;};const Hud marker[]={{8,8,0,1,0xffdc2020},{64,8,0,1,0xffdc2020},{8,30,0,1,0xffdc2020},{64,30,0,1,0xffdc2020}};
            d->SetVertexShader(nullptr);d->SetPixelShader(nullptr);d->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);d->SetRenderState(D3DRS_ZENABLE,FALSE);d->SetRenderState(D3DRS_LIGHTING,FALSE);
            d->SetTexture(0,nullptr);d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,marker,sizeof(Hud));
        }
        d->EndScene();return Read(d);
    };
    if(proxy){auto result=render(true);CHECK(result.size()==Width*Height);CreateDirectoryW(L"artifacts/grass",nullptr);
        CHECK(Save(result,L"artifacts/grass/field-proxy.png"));CHECK((result[15*Width+15]&0xffffff)==0xdc2020);puts("PASS: native proxy grass draw completed before the untouched HUD.");return 0;}
    auto off=render(false),on=render(true);
    printf("Grass started=%d failed=%d instances=%u bytes=%zu\n",GrassFXFixture::started(grass),GrassFXFixture::failed(grass),GrassFXFixture::count(grass),grass.bytes());
    CHECK(off.size()==Width*Height&&on.size()==off.size());CHECK(GrassFXFixture::started(grass)&&!GrassFXFixture::failed(grass));
    unsigned changed=0;for(size_t i=0;i<off.size();++i)changed+=off[i]!=on[i];printf("Changed pixels: %u\n",changed);CHECK(changed>10000);
    CreateDirectoryW(L"artifacts/grass",nullptr);CHECK(Save(off,L"artifacts/grass/field-off.png")&&Save(on,L"artifacts/grass/field-on.png"));
    CHECK(!stateErrors);auto repeated=render(true);unsigned large=0;double total=0;
    for(size_t px=0;px<on.size();++px){unsigned delta=0;for(unsigned shift:{0u,8u,16u})
        delta=std::max(delta,unsigned(std::abs(int((on[px]>>shift)&255)-int((repeated[px]>>shift)&255))));
        large+=delta>2;total+=delta;}
    printf("Repeated frame RGB delta=%u large=%u mean=%.6f\n",Delta(repeated,on),large,total/on.size());
    // Native MSAA depth rounding can flip a sub-sample at thin blade intersections.
    // Keep the affected area and mean change tiny; a whole-field shimmer must fail.
    if(msaa)CHECK(Delta(repeated,on)<=32&&large<on.size()/1000&&total<on.size()*.005);
    else CHECK(Delta(repeated,on)==0);
    extraDraws=2800;const auto busy=render(true);
    CHECK(GrassFXFixture::started(grass)&&!GrassFXFixture::failed(grass));
    CHECK(Delta(busy,on)<=(msaa?32u:0u));extraDraws=0;
    extraDraws=2800;distantTiles=true;CHECK(Delta(render(true),on)<=(msaa?32u:0u));
    CHECK(pendingAfterTiles==1);extraDraws=0;distantTiles=false;
    for(float pitch:{.03f,.35f,.9f})for(float yaw:{-.6f,0.f,.6f}){
        camera(pitch,yaw);const auto bare=render(false),blades=render(true);
        unsigned visible=0;for(size_t px=0;px<bare.size();++px)visible+=bare[px]!=blades[px];
        printf("Grass camera pitch=%.2f yaw=%.2f visible=%u\n",pitch,yaw,visible);
        CHECK(GrassFXFixture::started(grass)&&!GrassFXFixture::failed(grass)&&visible>1000);
        CHECK(GrassFXFixture::surface(grass,0,8)[1]>.99f);
    }
    camera(.16f,0);terrainFVF=0x112;
    CHECK(render(true)!=off&&GrassFXFixture::started(grass)&&!GrassFXFixture::failed(grass));terrainFVF=0x212;
    groundTest=true;CHECK(render(true)!=off&&GrassFXFixture::surface(grass,0,8)[1]>.99f);groundTest=false;
    followPlayer=true;
    for(float x:{-12.f,0.f,12.f})for(float pitch:{.1f,.9f,1.45f}){
        cameraEye[0]=x;cameraEye[1]=10;camera(pitch,0);CHECK(render(true).size()==off.size()&&!GrassFXFixture::failed(grass));
        CHECK((GrassFXFixture::center(grass)==std::array<float,2>{0,8}));
        CHECK(GrassFXFixture::surface(grass,0,8)[1]>.99f);
    }
    followPlayer=false;cameraEye[0]=0;cameraEye[1]=2.3f;camera(.16f,0);
    Com<IDirect3DTexture9> mixedSoil;CHECK(SUCCEEDED(d->CreateTexture(32,32,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,mixedSoil.out(),nullptr)));
    {D3DLOCKED_RECT lock{};CHECK(SUCCEEDED(mixedSoil->LockRect(0,&lock,nullptr,0)));
        for(int y=0;y<32;++y)for(int x=0;x<32;++x)((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=x<16?0xff8c6937:0xff416e28;
        mixedSoil->UnlockRect(0);DWORD kind=3;CHECK(SUCCEEDED(mixedSoil->SetPrivateData(GrassTag,&kind,4,0)));}
    std::swap(art.p,mixedSoil.p);CHECK(render(true).size()==off.size()&&GrassFXFixture::started(grass)&&!GrassFXFixture::failed(grass));
    CHECK(GrassFXFixture::surface(grass,1,8)[1]<.01f&&GrassFXFixture::surface(grass,4,8)[1]>.99f);
    std::swap(art.p,mixedSoil.p);
    for(DWORD alpha:{0u,128u,255u}){road=true;roadAlpha=alpha;auto result=render(true);CHECK(result.size()==on.size()&&!GrassFXFixture::failed(grass));
        const auto sample=GrassFXFixture::surface(grass,0,8);printf("Road alpha=%lu coverage=%.3f height=%.3f\n",alpha,sample[1],sample[0]);
        CHECK(std::abs(sample[1]-(1-alpha/255.f))<.01f);CHECK(std::abs(sample[0]-Ground(0,8))<.03f);
        if(alpha==255)CHECK(Save(result,L"artifacts/grass/field-road.png"));}
    roadGradient=true;CHECK(render(true).size()==on.size()&&!GrassFXFixture::failed(grass));
    const auto left=GrassFXFixture::surface(grass,-1,8),middle=GrassFXFixture::surface(grass,0,8),right=GrassFXFixture::surface(grass,1,8);
    CHECK(left[1]>.8f&&middle[1]>.45f&&middle[1]<.55f&&right[1]<.2f);
    roadGradient=false;roadProp=true;CHECK(render(true).size()==on.size()&&!GrassFXFixture::failed(grass));CHECK(GrassFXFixture::surface(grass,0,8)[1]<.01f);roadProp=false;
    // A raised roof/road cannot move the ground height, including a partial
    // blend. Native depth-write-disabled terrain overlays still erase blades.
    propHeight=5;roadAlpha=128;CHECK(render(true).size()==on.size());
    CHECK(std::abs(GrassFXFixture::surface(grass,0,8)[0]-Ground(0,8))<.03f);
    roadWrite=false;roadAlpha=255;CHECK(render(true).size()==on.size());CHECK(GrassFXFixture::surface(grass,0,8)[1]<.01f);
    roadWrite=true;roadProp=true;leafProp=true;CHECK(render(true).size()==on.size());
    CHECK(GrassFXFixture::surface(grass,0,8)[1]>.99f);leafProp=roadProp=false;propHeight=0;
    road=false;
    Com<IDirect3DTexture9> snowy;CHECK(SUCCEEDED(d->CreateTexture(32,32,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,snowy.out(),nullptr)));
    {D3DLOCKED_RECT lock{};CHECK(SUCCEEDED(snowy->LockRect(0,&lock,nullptr,0)));
        for(int y=0;y<32;++y)for(int x=0;x<32;++x)((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=0xffd8e4ee;
        snowy->UnlockRect(0);DWORD kind=4;snowy->SetPrivateData(GrassTag,&kind,4,0);}
    std::swap(art.p,snowy.p);settings.grassSnow=false;const auto noSnow=render(true);
    CHECK(!GrassFXFixture::started(grass)&&Delta(noSnow,render(false))<=(msaa?2u:0u));
    settings.grassSnow=true;const auto snowGrass=render(true);CHECK(GrassFXFixture::started(grass)&&!GrassFXFixture::failed(grass));
    CHECK(GrassFXFixture::surface(grass,0,8)[1]<-.99f&&Delta(noSnow,snowGrass)>20);
    CHECK(Save(noSnow,L"artifacts/grass/snow-grass-off.png")&&Save(snowGrass,L"artifacts/grass/snow-grass-on.png"));
    settings.grassSnow=false;CHECK(Delta(noSnow,render(true))<=(msaa?2u:0u)&&!GrassFXFixture::started(grass));std::swap(art.p,snowy.p);
    settings.grassHeight=.2f;auto shortGrass=render(true);settings.grassHeight=.95f;auto tallGrass=render(true);CHECK(shortGrass!=tallGrass);
    CHECK(Save(shortGrass,L"artifacts/grass/field-short.png")&&Save(tallGrass,L"artifacts/grass/field-tall.png"));
    settings.grassHeight=.55f;
    for(int q=0;q<3;++q){settings.grassQuality=float(q);auto result=render(true);CHECK(!GrassFXFixture::failed(grass)&&GrassFXFixture::count(grass)>0&&result!=off);}
    settings.grassQuality=2;settings.grassDistance=100;settings.grassDensity=3;
    CHECK(render(true).size()==on.size()&&!GrassFXFixture::failed(grass));CHECK(GrassFXFixture::count(grass)<=80000&&grass.bytes()<24*1024*1024);
    settings.grassDistance=40;settings.grassDensity=1.25f;settings.grassWind=.5f;
    const auto windy=render(true);Sleep(80);CHECK(windy!=render(true));settings.grassWind=0;
    grass.invalidate();CHECK(!grass.bytes());CHECK(render(true).size()==on.size()&&!GrassFXFixture::failed(grass));
    CHECK(!stateErrors);
    puts("PASS: instanced grass on native buffered terrain with preserved source geometry.");return 0;
}
