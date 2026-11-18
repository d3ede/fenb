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
float Tilt=0;
float Ground(float x,float z){return .45f*std::sin(x*.19f)+.25f*std::cos(z*.23f)+Tilt*x;}
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
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
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
    static UINT bladeVertices(const GrassFX &g){return g.bands_[0].vertices;}
    static bool hasLeafArt(const GrassFX &g){return g.leafArt_.p!=nullptr;}
    static std::array<float,2> center(const GrassFX &g){return {g.map_[0],g.map_[1]};}
    static size_t queued(const GrassFX &g){return g.pending_.size();}
    static float contact(GrassFX &g,float x,float z){
        Com<IDirect3DSurface9> read;
        if(FAILED(g.device_->CreateOffscreenPlainSurface(g.resolution_,g.resolution_,D3DFMT_R32F,D3DPOOL_SYSTEMMEM,read.out(),nullptr))||
            FAILED(g.device_->GetRenderTargetData(g.contactSurface_,read)))return -1;
        const UINT px=UINT(std::clamp((x-g.map_[0])/(2*g.map_[2])+.5f,0.f,.999f)*g.resolution_);
        const UINT py=UINT(std::clamp(.5f-(z-g.map_[1])/(2*g.map_[2]),0.f,.999f)*g.resolution_);
        D3DLOCKED_RECT lock{};float value=-1;
        if(SUCCEEDED(read->LockRect(&lock,nullptr,D3DLOCK_READONLY))){memcpy(&value,(BYTE *)lock.pBits+py*lock.Pitch+px*4,4);read->UnlockRect();}
        return value;
    }
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
        fputs("[General]\nenabled=1\nopen_on_start=0\nunlock_fps=0\n[Bloom]\nbloom=0\n[Color]\ncolor=0\n[WaterFX]\nwater=0\n[SceneFX]\ngrass=1\ngrass_quality=2\ngrass_distance=12\ngrass_wind=0\n",f);fclose(f);
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
    Com<IDirect3DTexture9> art,terrainCloud;
    if(assets){
        auto module=LoadLibraryExW(L"d3dx9_43.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        using Load=HRESULT(WINAPI *)(IDirect3DDevice9 *,LPCWSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,DWORD,DWORD,D3DCOLOR,void *,PALETTEENTRY *,IDirect3DTexture9 **);
        auto load=reinterpret_cast<Load>(GetProcAddress(module,"D3DXCreateTextureFromFileExW"));CHECK(load);
        for(const char *name:{"ground_clay001.dds","ground_clay003.dds","ground_clay006.dds","ground_clay007.dds",
                              "ground_n_clay_lawn001.dds","Upresia_Ground01.dds","Upresia_Ground01_01.dds",
                              "rustia_lawn01.dds","ground_n_ice02.dds","ground_n_ice03.dds"}){
            const std::string file=std::string(assets)+"/World/Texture/"+name;const std::wstring native(file.begin(),file.end());
            for(auto format:{D3DFMT_UNKNOWN,D3DFMT_A8R8G8B8}){Com<IDirect3DTexture9> source;
                CHECK(SUCCEEDED(load(d,native.c_str(),UINT(-1),UINT(-1),0,0,format,D3DPOOL_MANAGED,1,1,0,nullptr,nullptr,source.out())));
                CHECK(GrassFXFixture::kind(grass,source)==(!strcmp(name,"rustia_lawn01.dds")?1u:strstr(name,"ice")?4u:3u));}
        }
        for(const char *name:{"brick_MaFl01_new.dds","5da3e18d.tga","Obj_FLGR01.dds"})for(auto format:{D3DFMT_UNKNOWN,D3DFMT_A8R8G8B8}){
            const std::string file=std::string(assets)+"/World/Texture/"+name;
            const std::wstring native(file.begin(),file.end());Com<IDirect3DTexture9> brick;
            CHECK(SUCCEEDED(load(d,native.c_str(),UINT(-1),UINT(-1),0,0,format,D3DPOOL_MANAGED,1,1,0,nullptr,nullptr,brick.out())));
            CHECK(GrassFXFixture::kind(grass,brick)==6);
        }
        for(const char *name:{"sky_cloud01.dds","sky_cloud02.dds"})for(auto format:{D3DFMT_UNKNOWN,D3DFMT_A8R8G8B8}){
            const std::string file=std::string(assets)+"/World/Texture/"+name;
            const std::wstring native(file.begin(),file.end());Com<IDirect3DTexture9> cloud;
            CHECK(SUCCEEDED(load(d,native.c_str(),UINT(-1),UINT(-1),0,0,format,D3DPOOL_MANAGED,1,1,0,nullptr,nullptr,cloud.out())));
            CHECK(GrassFXFixture::kind(grass,cloud)==8);
            if(!terrainCloud.p)std::swap(terrainCloud.p,cloud.p);
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
    float cameraEye[]={0,2.3f,-5};float player[]={0,0,8};bool followPlayer=false;uint64_t worldKey=0;
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
    bool groundTest=false,roadTest=false,sourceSRGB=false,roadWrite=true,leafProp=false,propBlend=false,doubleProp=false,nativeLeaf=false;float propHeight=0;int lightingCase=0;
    bool distantTiles=false;size_t pendingAfterTiles=0;
    bool translucentLeaf=false,earlyGrass=true,refinementBoundary=false,grassBeforeMist=false;
    bool paving=false,earlyPaving=false,maskOnly=false;
    bool waterMask=false;
    DWORD maskMip=0;
    float waterY=0;
    Com<IDirect3DTexture9> dirt,coverage;
    CHECK(SUCCEEDED(d->CreateTexture(8,8,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,dirt.out(),nullptr)));
    CHECK(SUCCEEDED(d->CreateTexture(8,8,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,coverage.out(),nullptr)));
    // Deliberately different lower mips expose camera-selected mask sampling.
    for(UINT level=1;level<coverage->GetLevelCount();++level){
        D3DLOCKED_RECT lock{};CHECK(SUCCEEDED(coverage->LockRect(level,&lock,nullptr,0)));
        const UINT size=8u>>level;
        for(UINT y=0;y<size;++y)for(UINT x=0;x<size;++x)
            reinterpret_cast<DWORD *>(static_cast<BYTE *>(lock.pBits)+y*lock.Pitch)[x]=0x00ffffff;
        CHECK(SUCCEEDED(coverage->UnlockRect(level)));
    }
    {D3DLOCKED_RECT lock{};dirt->LockRect(0,&lock,nullptr,0);for(int y=0;y<8;++y)for(int x=0;x<8;++x)((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=0xff826a40;dirt->UnlockRect(0);}
    Com<IDirect3DTexture9> nativeCard;
    if(proxy){
        CHECK(SUCCEEDED(d->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,nativeCard.out(),nullptr)));
        D3DLOCKED_RECT lock{};
        CHECK(SUCCEEDED(nativeCard->LockRect(0,&lock,nullptr,0)));
        *static_cast<DWORD *>(lock.pBits)=0xffff00ff;
        CHECK(SUCCEEDED(nativeCard->UnlockRect(0)));
        const DWORD grassCardKind=2;
        CHECK(SUCCEEDED(nativeCard->SetPrivateData(GrassTag,&grassCardKind,sizeof(grassCardKind),0)));
    }
    auto render=[&](bool on){
        StateGuard outer(d);grass.beginFrame(on,followPlayer?player:nullptr,worldKey);
        d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff9aaeb7,1,0);
        d->SetTransform(D3DTS_WORLD,&identity);d->SetTransform(D3DTS_VIEW,&view);d->SetTransform(D3DTS_PROJECTION,&projection);
        d->SetVertexShader(nullptr);d->SetPixelShader(nullptr);d->SetFVF(terrainFVF);
        d->SetRenderState(D3DRS_ZENABLE,TRUE);d->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);d->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
        d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);d->SetRenderState(D3DRS_ALPHATESTENABLE,groundTest);d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
        d->SetRenderState(D3DRS_LIGHTING,TRUE);d->SetRenderState(D3DRS_AMBIENT,lightingCase==1?0xff503458:lightingCase==2?0xff080810:0xff60605a);
        D3DMATERIAL9 material{};material.Diffuse={1,1,1,1};material.Ambient={1,1,1,1};d->SetMaterial(&material);
        D3DLIGHT9 light{};light.Type=D3DLIGHT_DIRECTIONAL;light.Direction={.3f,-.86f,.4f};light.Diffuse={.65f,.65f,.55f,1};d->SetLight(0,&light);d->LightEnable(0,TRUE);
        if(lightingCase){light.Diffuse=lightingCase==1?D3DCOLORVALUE{.7f,.4f,.8f,1}:D3DCOLORVALUE{.02f,.025f,.05f,1};d->SetLight(0,&light);}
        d->SetRenderState(D3DRS_FOGENABLE,FALSE);
        d->SetTexture(0,art);d->SetTexture(1,nullptr);
        d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,sourceSRGB);
        d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);d->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
        for(DWORD stage=0;stage<2;++stage){d->SetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,stage);d->SetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
            d->SetSamplerState(stage,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);d->SetSamplerState(stage,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);d->SetSamplerState(stage,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);}
        d->SetStreamSource(0,vb,0,sizeof(Vertex));d->SetIndices(ib);
        d->BeginScene();const SurfaceDraw geometry{D3DPT_TRIANGLELIST,0,UINT(indices.size()/3),0,UINT(vertices.size()),0,true};
        auto draw=[&](){return d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,UINT(vertices.size()),0,UINT(indices.size()/3));};
        if(on&&!proxy&&earlyPaving){
            StateGuard roadState(d);
            D3DLOCKED_RECT lock{};coverage->LockRect(0,&lock,nullptr,0);
            for(int y=0;y<8;++y)for(int x=0;x<8;++x)((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=x<4?0:0xffffffff;
            coverage->UnlockRect(0);
            const DWORD tag=6;dirt->SetPrivateData(GrassTag,&tag,sizeof(tag),0);
            d->SetTexture(0,dirt);d->SetTexture(1,coverage);
            d->SetSamplerState(1,D3DSAMP_MAXMIPLEVEL,maskMip);
            d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
            d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
            d->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);d->SetTextureStageState(1,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
            const Vertex strip[]={{-3,0,4,0,1,0,0,0,.125f,.5f},{3,0,4,0,1,0,1,0,.875f,.5f},
                                  {-3,0,12,0,1,0,0,1,.125f,.5f},{3,0,12,0,1,0,1,1,.875f,.5f}};
            grass.capture(settings,[&](){return d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,strip,sizeof(Vertex));});
        }
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
            d->SetRenderState(D3DRS_ALPHATESTENABLE,roadTest);
            if(!nativeLeaf){DWORD tag=leafProp?5u:paving?6u:0u;dirt->SetPrivateData(GrassTag,&tag,4,0);}
            d->SetTexture(0,dirt);d->SetTexture(1,coverage);
            d->SetSamplerState(1,D3DSAMP_MAXMIPLEVEL,maskMip);
            d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
            d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_SELECTARG1);d->SetTextureStageState(1,D3DTSS_COLORARG1,D3DTA_CURRENT);
            d->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);d->SetTextureStageState(1,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
            if(roadProp){d->SetFVF(0x112);d->SetRenderState(D3DRS_ALPHABLENDENABLE,propBlend);d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);}
            auto drawRoad=[&](){return d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(strip.size()/3),strip.data(),sizeof(Vertex));};
            if(on&&!proxy){grass.capture(settings,drawRoad);grass.beforeTranslucency(settings);}
            if(!maskOnly)drawRoad();
            if(doubleProp){
                auto shifted=identity;shifted._42=paving?-2.f:5.f;
                d->SetTransform(D3DTS_WORLD,&shifted);
                if(paving){DWORD lowerProp=0;dirt->SetPrivateData(GrassTag,&lowerProp,4,0);}
                if(on&&!proxy)grass.capture(settings,drawRoad);
                drawRoad();d->SetTransform(D3DTS_WORLD,&identity);
            }
        }
        if(waterMask){
            struct WaterVertex {float x,y,z,nx,ny,nz;DWORD color;float u,v;};
            const WaterVertex strip[]={{-3,waterY,0,0,1,0,0,0,0},{3,waterY,0,0,1,0,0,1,0},
                                      {-3,waterY,24,0,1,0,0,0,1},{3,waterY,24,0,1,0,0,1,1}};
            d->SetFVF(0x152);
            d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
            d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
            d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
            d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
            auto drawWater=[&](){return d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,strip,sizeof(WaterVertex));};
            if(on&&!proxy)grass.capture(settings,drawWater,{},true);
        }
        if(refinementBoundary&&on&&!proxy)grassBeforeMist=grass.beforeRefinement(settings);
        if(translucentLeaf){
            d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
            d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
            d->SetFVF(0x102);
            const DWORD leafTag=5;dirt->SetPrivateData(GrassTag,&leafTag,4,0);d->SetTexture(0,dirt);
            if(on&&!proxy&&earlyGrass)grass.beforeTranslucency(settings);
            // Native proximity-faded leaves can still write depth. Their color
            // must blend over grass that has already been submitted.
            struct Leaf {float x,y,z,w;DWORD color;};
            const Leaf leaf[]={{-.5f,-.5f,.01f,1,0x40406020},{Width-.5f,-.5f,.01f,1,0x40406020},
                               {-.5f,Height-.5f,.01f,1,0x40406020},{Width-.5f,Height-.5f,.01f,1,0x40406020}};
            d->SetVertexShader(nullptr);d->SetPixelShader(nullptr);d->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
            d->SetTexture(0,nullptr);d->SetRenderState(D3DRS_LIGHTING,FALSE);d->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
            d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
            d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
            d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
            d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,leaf,sizeof(Leaf));
        }
        if(on&&!proxy){
            // Native software vertex processing has no vertex texture fetch.
            // Probe binding restoration only in the hardware host mode.
            if(!mixed)d->SetTexture(D3DVERTEXTEXTURESAMPLER1,art);
            if(settings.grassStyle==1)d->SetTexture(2,art);
            if(!mixed){d->SetStreamSourceFreq(0,D3DSTREAMSOURCE_INDEXEDDATA|3);d->SetStreamSourceFreq(1,D3DSTREAMSOURCE_INSTANCEDATA|1);}
            grass.draw(settings);
            Com<IDirect3DBaseTexture9> restored;d->GetTexture(D3DVERTEXTEXTURESAMPLER1,restored.out());
            stateErrors+=restored.p!=(mixed?nullptr:art.p);
            if(settings.grassStyle==1){d->GetTexture(2,restored.out());stateErrors+=restored.p!=art.p;}
            stateErrors+=d->GetSoftwareVertexProcessing()!=BOOL(mixed);
            UINT a=0,b=0;d->GetStreamSourceFreq(0,&a);d->GetStreamSourceFreq(1,&b);
            stateErrors+=a!=(mixed?1u:D3DSTREAMSOURCE_INDEXEDDATA|3)||b!=(mixed?1u:D3DSTREAMSOURCE_INSTANCEDATA|1);
            d->SetStreamSourceFreq(0,1);d->SetStreamSourceFreq(1,1);
        }
        if(proxy){
            // The near field is eligible for generated grass. This stock patch
            // at z=23 is beyond the 12-unit radius from the camera at z=-5.
            struct CardVertex {float x,y,z,u,v;};
            const CardVertex card[]={{-2,.1f,23,0,1},{2,.1f,23,1,1},{-2,1.5f,23,0,0},{2,1.5f,23,1,0}};
            d->SetFVF(D3DFVF_XYZ|D3DFVF_TEX1);
            d->SetTexture(0,nativeCard);
            d->SetRenderState(D3DRS_LIGHTING,FALSE);
            d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
            d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
            if(FAILED(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,card,sizeof(CardVertex))))return std::vector<DWORD>{};
            struct Hud {float x,y,z,w;DWORD color;};const Hud marker[]={{8,8,0,1,0xffdc2020},{64,8,0,1,0xffdc2020},{8,30,0,1,0xffdc2020},{64,30,0,1,0xffdc2020}};
            d->SetVertexShader(nullptr);d->SetPixelShader(nullptr);d->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);d->SetRenderState(D3DRS_ZENABLE,FALSE);d->SetRenderState(D3DRS_LIGHTING,FALSE);
            d->SetTexture(0,nullptr);d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,marker,sizeof(Hud));
        }
        d->EndScene();return Read(d);
    };
    if(proxy){
        for(float yaw:{-.2f,0.f,.2f}){
            camera(.16f,yaw);
            const auto result=render(true);CHECK(result.size()==Width*Height);
            CHECK((result[15*Width+15]&0xffffff)==0xdc2020);
            CHECK(std::count_if(result.begin(),result.end(),[](DWORD p){return (p&0xffffff)==0xff00ff;})>100);
            if(yaw==0)CHECK(Save(result,L"artifacts/grass/field-proxy.png"));
            CHECK(SUCCEEDED(d->Present(nullptr,nullptr,nullptr,nullptr)));
        }
        puts("PASS: native grass outside generated coverage and HUD remain visible through the proxy.");
        return 0;
    }
    auto off=render(false),on=render(true);
    refinementBoundary=true;
    const auto atRefinement=render(true);
    CHECK(grassBeforeMist&&atRefinement.size()==on.size()&&Delta(atRefinement,off)>20);
    refinementBoundary=false;
    printf("Grass started=%d failed=%d instances=%u bytes=%zu\n",GrassFXFixture::started(grass),GrassFXFixture::failed(grass),GrassFXFixture::count(grass),grass.bytes());
    CHECK(off.size()==Width*Height&&on.size()==off.size());CHECK(GrassFXFixture::started(grass)&&!GrassFXFixture::failed(grass));
    unsigned changed=0;for(size_t i=0;i<off.size();++i)changed+=off[i]!=on[i];printf("Changed pixels: %u\n",changed);CHECK(changed>10000);
    CreateDirectoryW(L"artifacts/grass",nullptr);CHECK(Save(off,L"artifacts/grass/field-off.png")&&Save(on,L"artifacts/grass/field-on.png"));
    const UINT detailedVertices=GrassFXFixture::bladeVertices(grass);
    settings.grassStyle=1;
    const auto simple=render(true);
    CHECK(GrassFXFixture::hasLeafArt(grass)&&GrassFXFixture::bladeVertices(grass)<detailedVertices&&Delta(on,simple)>20);
    CHECK(Save(simple,L"artifacts/grass/field-simple.png"));
    const float savedSimpleVariation=settings.grassVariation;
    settings.grassVariation=0;
    const auto restrainedSimple=render(true);
    settings.grassVariation=1;
    const auto variedSimple=render(true);
    CHECK(Delta(restrainedSimple,variedSimple)>20&&!GrassFXFixture::failed(grass));
    settings.grassVariation=savedSimpleVariation;
    settings.grassGroundTint=true;
    CHECK(Delta(simple,render(true))<=(msaa?32u:0u));
    settings.grassGroundTint=false;
    followPlayer=true;cameraEye[1]=8;cameraEye[2]=7.3f;camera(1.48f,0);
    const auto aboveBare=render(false),aboveSimple=render(true);
    unsigned aboveChanged=0;
    for(size_t i=0;i<aboveSimple.size();++i)aboveChanged+=aboveSimple[i]!=aboveBare[i];
    CHECK(aboveSimple.size()==on.size()&&aboveChanged>10000&&GrassFXFixture::hasLeafArt(grass));
    followPlayer=false;cameraEye[1]=2.3f;cameraEye[2]=-5;camera(.16f,0);
    settings.grassStyle=0;
    CHECK(Delta(on,render(true))<=(msaa?32u:0u));
    {
        Com<IDirect3DTexture9> soil;
        CHECK(SUCCEEDED(d->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,soil.out(),nullptr)));
        const DWORD kind=1;
        CHECK(SUCCEEDED(soil->SetPrivateData(GrassTag,&kind,sizeof(kind),0)));
        std::swap(art.p,soil.p);
        for(const DWORD pixel:{0xffa8783cu,0xff238f53u}){
            D3DLOCKED_RECT lock{};CHECK(SUCCEEDED(art->LockRect(0,&lock,nullptr,0)));
            *static_cast<DWORD *>(lock.pBits)=pixel;
            CHECK(SUCCEEDED(art->UnlockRect(0)));
            const auto bare=render(false),fixed=render(true);
            settings.grassGroundTint=true;
            const auto adapted=render(true);
            CHECK(adapted.size()==on.size()&&GrassFXFixture::started(grass)&&!GrassFXFixture::failed(grass));
            const bool warm=pixel==0xffa8783cu;
            auto warmth=[](DWORD color){return int((color>>16)&255)-int((color>>8)&255);};
            unsigned grassPixels=0;long shift=0;
            for(size_t i=0;i<adapted.size();++i){
                if(fixed[i]==bare[i]||adapted[i]==bare[i])continue;
                ++grassPixels;shift+=warmth(adapted[i])-warmth(fixed[i]);
            }
            const float mean=grassPixels?float(shift)/grassPixels:0;
            printf("Ground tint %s: %u pixels, mean red-green shift=%.2f\n",warm?"warm":"green",grassPixels,mean);
            CHECK(grassPixels>10000&&(warm?mean>9:mean<-5));
            CHECK(detailedVertices==GrassFXFixture::bladeVertices(grass));
            if(warm)CHECK(Save(fixed,L"artifacts/grass/ground-tint-off.png")&&Save(adapted,L"artifacts/grass/ground-tint-on.png"));
            else CHECK(Save(adapted,L"artifacts/grass/ground-tint-green.png"));
            settings.grassGroundTint=false;
        }
        std::swap(art.p,soil.p);
        CHECK(Delta(on,render(true))<=(msaa?32u:0u));
    }
    translucentLeaf=true;earlyGrass=false;const auto lateLeaf=render(true);
    earlyGrass=true;const auto overGrass=render(true);translucentLeaf=false;
    double blendError=0;
    for(size_t i=0;i<on.size();++i)for(unsigned shift:{0u,8u,16u}){
        const float native=float((on[i]>>shift)&255),leaf=float((0x406020u>>shift)&255);
        const float expected=native*(191.f/255)+leaf*(64.f/255);
        blendError+=std::abs(float((overGrass[i]>>shift)&255)-expected);
    }
    CHECK(blendError/(on.size()*3)<1.1&&Delta(lateLeaf,overGrass)>20);
    CHECK(Save(lateLeaf,L"artifacts/grass/leaf-late-broken.png")&&Save(overGrass,L"artifacts/grass/leaf-early-correct.png"));
    CHECK(!stateErrors);auto repeated=render(true);unsigned large=0;double total=0;
    for(size_t px=0;px<on.size();++px){unsigned delta=0;for(unsigned shift:{0u,8u,16u})
        delta=std::max(delta,unsigned(std::abs(int((on[px]>>shift)&255)-int((repeated[px]>>shift)&255))));
        large+=delta>2;total+=delta;}
    printf("Repeated frame RGB delta=%u large=%u mean=%.6f\n",Delta(repeated,on),large,total/on.size());
    // Native MSAA depth rounding can flip a sub-sample at thin blade intersections.
    // Keep the affected area below 0.1% and the mean below 0.01 of one RGB
    // level even with the contrasting painted tips; whole-field shimmer fails.
    if(msaa)CHECK(Delta(repeated,on)<=32&&large<on.size()/1000&&total<on.size()*.01);
    else CHECK(Delta(repeated,on)==0);
    extraDraws=2800;const auto busy=render(true);
    printf("Busy frame delta=%u\n",Delta(busy,on));
    if(msaa)CHECK(Save(busy,L"artifacts/grass/busy-msaa.png"));
    CHECK(GrassFXFixture::started(grass)&&!GrassFXFixture::failed(grass));
    CHECK(Delta(busy,on)<=(msaa?32u:0u));extraDraws=0;
    extraDraws=2800;distantTiles=true;CHECK(Delta(render(true),on)<=(msaa?32u:0u));
    CHECK(pendingAfterTiles==1);extraDraws=0;distantTiles=false;
    if(!terrainCloud.p){
        CHECK(SUCCEEDED(d->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,terrainCloud.out(),nullptr)));
        D3DLOCKED_RECT lock{};CHECK(SUCCEEDED(terrainCloud->LockRect(0,&lock,nullptr,0)));
        *static_cast<DWORD *>(lock.pBits)=0xffffffff;
        CHECK(SUCCEEDED(terrainCloud->UnlockRect(0)));
        const DWORD terrainCloudKind=8;
        CHECK(SUCCEEDED(terrainCloud->SetPrivateData(GrassTag,&terrainCloudKind,sizeof(terrainCloudKind),0)));
    }
    // Native terrain clouds use the same FVF, depth writes and blend-mask
    // stage as land. Replay them only in the top view and compare the roots.
    std::swap(dirt.p,terrainCloud.p);nativeLeaf=maskOnly=true;followPlayer=true;
    for(float pitch:{.35f,1.48f}){
        cameraEye[1]=13*std::sin(pitch);cameraEye[2]=player[2]-13*std::cos(pitch);camera(pitch,0);
        const auto grassOnly=render(true);
        road=pitch>1.f;
        const auto cloudLayer=render(true);
        CHECK(Delta(grassOnly,cloudLayer)<=(msaa?32u:0u)&&GrassFXFixture::surface(grass,0,8)[1]>.99f);
        road=false;
    }
    followPlayer=maskOnly=nativeLeaf=false;cameraEye[1]=2.3f;cameraEye[2]=-5;camera(.16f,0);
    std::swap(dirt.p,terrainCloud.p);
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
        const auto center=GrassFXFixture::center(grass);
        CHECK(std::abs(center[0])<.083f&&std::abs(center[1]-8)<.083f);
        CHECK(GrassFXFixture::surface(grass,0,8)[1]>.99f);
    }
    followPlayer=false;cameraEye[0]=0;cameraEye[1]=2.3f;camera(.16f,0);
    // Fractional movement over steep terrain must not reseed the field or
    // collapse coverage when the local map advances by one texel.
    settings.grassSlope=60;followPlayer=true;
    for(float tilt:{.8f,1.f,1.5f}){
        Tilt=tilt;for(auto &v:vertices){v.y=Ground(v.x,v.z);const float nx=-.45f*.19f*std::cos(v.x*.19f)-Tilt,nz=.25f*.23f*std::sin(v.z*.23f);
            const float length=std::sqrt(nx*nx+nz*nz+1);v.nx=nx/length;v.ny=1/length;v.nz=nz/length;}
        CHECK(SUCCEEDED(vb->Lock(0,0,&data,0)));memcpy(data,vertices.data(),vertices.size()*sizeof(Vertex));vb->Unlock();
        unsigned minimum=Width*Height,maximum=0;
        for(int frame=0;frame<12;++frame){player[0]=frame*.035f;const auto bare=render(false),blades=render(true);
            unsigned visible=0;for(size_t px=0;px<bare.size();++px)visible+=bare[px]!=blades[px];
            minimum=std::min(minimum,visible);maximum=std::max(maximum,visible);
            CHECK(GrassFXFixture::surface(grass,0,8)[1]>.99f&&!GrassFXFixture::failed(grass));
        }
        printf("Steep terrain tilt=%.2f moving coverage=%u..%u\n",tilt,minimum,maximum);
        CHECK(minimum>10000&&maximum<minimum*1.15f);
    }
    Tilt=0;for(auto &v:vertices){v.y=Ground(v.x,v.z);const float nx=-.45f*.19f*std::cos(v.x*.19f),nz=.25f*.23f*std::sin(v.z*.23f);
        const float length=std::sqrt(nx*nx+nz*nz+1);v.nx=nx/length;v.ny=1/length;v.nz=nz/length;}
    CHECK(SUCCEEDED(vb->Lock(0,0,&data,0)));memcpy(data,vertices.data(),vertices.size()*sizeof(Vertex));vb->Unlock();
    followPlayer=false;player[0]=0;settings.grassSlope=45;
    Com<IDirect3DTexture9> mixedSoil;CHECK(SUCCEEDED(d->CreateTexture(32,32,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,mixedSoil.out(),nullptr)));
    {D3DLOCKED_RECT lock{};CHECK(SUCCEEDED(mixedSoil->LockRect(0,&lock,nullptr,0)));
        for(int y=0;y<32;++y)for(int x=0;x<32;++x)((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=x<16?0xff8c6937:0xff8c845f;
        mixedSoil->UnlockRect(0);DWORD kind=3;CHECK(SUCCEEDED(mixedSoil->SetPrivateData(GrassTag,&kind,4,0)));}
    std::swap(art.p,mixedSoil.p);CHECK(render(true).size()==off.size()&&GrassFXFixture::started(grass)&&!GrassFXFixture::failed(grass));
    CHECK(GrassFXFixture::surface(grass,1,8)[1]<.01f&&GrassFXFixture::surface(grass,4,8)[1]>.8f);
    sourceSRGB=true;CHECK(render(true).size()==on.size());
    CHECK(GrassFXFixture::surface(grass,1,8)[1]<.01f&&GrassFXFixture::surface(grass,4,8)[1]>.8f);sourceSRGB=false;
    std::swap(art.p,mixedSoil.p);
    for(DWORD alpha:{0u,128u,255u}){road=true;roadAlpha=alpha;auto result=render(true);CHECK(result.size()==on.size()&&!GrassFXFixture::failed(grass));
        const auto sample=GrassFXFixture::surface(grass,0,8);printf("Road alpha=%lu coverage=%.3f height=%.3f\n",alpha,sample[1],sample[0]);
        CHECK(std::abs(sample[1]-(1-alpha/255.f))<.01f);CHECK(std::abs(sample[0]-Ground(0,8))<.03f);
        if(alpha==255)CHECK(Save(result,L"artifacts/grass/field-road.png"));}
    roadTest=true;CHECK(render(true).size()==on.size());CHECK(GrassFXFixture::surface(grass,0,8)[1]<.01f);roadTest=false;
    // Turning the camera can change native terrain mask mip limits. The local
    // world map must keep its own footprint and the same planted coverage.
    maskOnly=true;
    const auto fullMask=render(true);
    maskMip=3;
    const auto cameraMask=render(true);
    CHECK(GrassFXFixture::surface(grass,0,8)[1]<.01f);
    CHECK(Delta(fullMask,cameraMask)<=(msaa?32u:0u));
    maskMip=0;maskOnly=false;
    roadGradient=true;CHECK(render(true).size()==on.size()&&!GrassFXFixture::failed(grass));
    const auto left=GrassFXFixture::surface(grass,-1,8),middle=GrassFXFixture::surface(grass,0,8),right=GrassFXFixture::surface(grass,1,8);
    CHECK(left[1]>.8f&&middle[1]>.45f&&middle[1]<.55f&&right[1]<.2f);
    roadGradient=false;roadProp=true;CHECK(render(true).size()==on.size()&&!GrassFXFixture::failed(grass));
    CHECK(GrassFXFixture::surface(grass,0,8)[1]>.99f&&GrassFXFixture::contact(grass,0,8)>0);roadProp=false;
    // A raised roof/road cannot move the ground height, including a partial
    // blend. Native depth-write-disabled terrain overlays still erase blades.
    propHeight=5;roadAlpha=128;CHECK(render(true).size()==on.size());
    CHECK(std::abs(GrassFXFixture::surface(grass,0,8)[0]-Ground(0,8))<.03f);
    roadWrite=false;roadAlpha=255;CHECK(render(true).size()==on.size());CHECK(GrassFXFixture::surface(grass,0,8)[1]<.01f);
    roadWrite=true;roadProp=true;leafProp=true;CHECK(render(true).size()==on.size());
    CHECK(GrassFXFixture::surface(grass,0,8)[1]>.99f&&GrassFXFixture::contact(grass,0,8)==0);
    if(assets){
        auto module=LoadLibraryExW(L"d3dx9_43.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        using Load=HRESULT(WINAPI *)(IDirect3DDevice9 *,LPCWSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,DWORD,DWORD,D3DCOLOR,void *,PALETTEENTRY *,IDirect3DTexture9 **);
        auto load=reinterpret_cast<Load>(GetProcAddress(module,"D3DXCreateTextureFromFileExW"));CHECK(load);
        Com<IDirect3DTexture9> savedDirt;std::swap(savedDirt.p,dirt.p);nativeLeaf=true;leafProp=false;
        for(const char *name:{"Obj_MaCoPrGr07.dds","Obj_MaCoPrGr1.dds","Obj_MaCoPrGr2.dds","Obj_MaCoPrGr3-01.dds",
                             "Obj_MaCoPrGr04.dds","Obj_MaCoPrGr06.dds","Obj_MaCoPrFl02-01.dds","Obj_MaCoPrFl03-2.dds",
                             "Obj_MaSaPuGrass01.DDS","Obj_NewFlbush01.dds","Obj_MaCoPrTr02-01.dds","Obj_AzriaTree01-2.dds"}){
            const std::string file=std::string(assets)+"/Model/Texture/"+name;const std::wstring wide(file.begin(),file.end());
            for(auto format:{D3DFMT_UNKNOWN,D3DFMT_A8R8G8B8}){
                CHECK(SUCCEEDED(load(d,wide.c_str(),UINT(-1),UINT(-1),1,0,format,D3DPOOL_MANAGED,1,1,0,nullptr,nullptr,dirt.out())));
                const unsigned kind=GrassFXFixture::kind(grass,dirt);CHECK(kind==2||kind==5);
                if(!strcmp(name,"Obj_MaCoPrGr04.dds")&&format==D3DFMT_A8R8G8B8){
                    // Even an unclassified faded leaf must leave the receiver intact.
                    propHeight=0;propBlend=true;roadWrite=false;roadTest=false;
                    DWORD unknown=0;CHECK(SUCCEEDED(dirt->SetPrivateData(GrassTag,&unknown,4,0)));
                    CHECK(render(true).size()==on.size()&&GrassFXFixture::contact(grass,0,8)==0);
                    CHECK(SUCCEEDED(dirt->FreePrivateData(GrassTag)));
                    propHeight=5;
                }
                for(bool faded:{false,true}){
                    propBlend=faded;roadWrite=!faded;roadTest=!faded;
                    CHECK(render(true).size()==on.size()&&!GrassFXFixture::failed(grass));
                    CHECK(GrassFXFixture::surface(grass,0,8)[1]>.99f&&GrassFXFixture::contact(grass,0,8)==0);
                }
            }
        }
        nativeLeaf=false;leafProp=true;propBlend=false;roadWrite=true;roadTest=false;
        std::swap(savedDirt.p,dirt.p);FreeLibrary(module);
    }
    leafProp=false;CHECK(render(true).size()==on.size());
    CHECK(GrassFXFixture::surface(grass,0,8)[1]>.99f);
    const float canopy=GrassFXFixture::contact(grass,0,8);
    CHECK(std::abs((2.3f+(.5f-canopy)*1024)-Ground(0,8)-5)<.04f);
    CHECK(Save(render(true),L"artifacts/grass/high-canopy.png"));roadProp=false;propHeight=0;
    roadProp=maskOnly=true;propHeight=.31f;
    const auto beneathCanopy=render(true);
    CHECK(beneathCanopy.size()==off.size());
    unsigned shortened=0;
    for(unsigned y=348;y<385;++y)for(unsigned x=480;x<520;++x)
        shortened+=beneathCanopy[y*Width+x]!=off[y*Width+x];
    CHECK(shortened>100);
    maskOnly=false;propHeight=0;
    // Native proximity fades and surface overlays may appear only from a
    // close or steep camera angle. They must not erase world-space roots.
    followPlayer=true;roadProp=true;
    for(float pitch:{.35f,.9f,1.48f})for(bool writing:{false,true}){
        cameraEye[1]=13*std::sin(pitch);
        cameraEye[2]=player[2]-13*std::cos(pitch);
        camera(pitch,0);roadWrite=writing;propBlend=true;
        const auto bare=render(false),blades=render(true);
        CHECK(Delta(bare,blades)>20);
        CHECK(GrassFXFixture::contact(grass,0,8)==0&&GrassFXFixture::surface(grass,0,8)[1]>.99f);
    }
    followPlayer=false;cameraEye[1]=2.3f;cameraEye[2]=-5;camera(.16f,0);
    roadWrite=true;propBlend=false;
    CHECK(render(true).size()==on.size());
    const float lowContact=GrassFXFixture::contact(grass,0,8);CHECK(lowContact>0);
    doubleProp=true;CHECK(render(true).size()==on.size());
    CHECK(std::abs(GrassFXFixture::contact(grass,0,8)-lowContact)<.00001f);doubleProp=false;
    paving=true;propHeight=.65f;doubleProp=true;
    CHECK(render(true).size()==on.size());
    const float pavingContact=GrassFXFixture::contact(grass,0,8);
    CHECK(pavingContact>1&&std::abs((2.3f+(pavingContact-1.5f)*1024)-Ground(0,8)-.65f)<.04f);
    paving=false;propHeight=0;doubleProp=false;
    roadProp=propBlend=false;roadWrite=true;
    road=false;
    earlyPaving=true;
    CHECK(render(true).size()==on.size());
    CHECK(GrassFXFixture::contact(grass,2,8)>1&&GrassFXFixture::contact(grass,-2,8)==0);
    CHECK(GrassFXFixture::surface(grass,2,8)[1]>.99f&&GrassFXFixture::surface(grass,-2,8)[1]>.99f);
    earlyPaving=false;
    waterMask=true;waterY=.15f;
    const auto flooded=render(true);
    CHECK(GrassFXFixture::contact(grass,0,8)>2);
    CHECK(GrassFXFixture::contact(grass,2,1)==0); // Stepping above the water line stays dry.
    CHECK(GrassFXFixture::contact(grass,5,8)==0);
    CHECK(Delta(on,flooded)>20);
    CHECK(Save(flooded,L"artifacts/grass/field-water-mask.png"));
    waterMask=false;
    Com<IDirect3DTexture9> snowy;CHECK(SUCCEEDED(d->CreateTexture(32,32,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,snowy.out(),nullptr)));
    {D3DLOCKED_RECT lock{};CHECK(SUCCEEDED(snowy->LockRect(0,&lock,nullptr,0)));
        for(int y=0;y<32;++y)for(int x=0;x<32;++x)((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=0xffd8e4ee;
        snowy->UnlockRect(0);DWORD kind=4;snowy->SetPrivateData(GrassTag,&kind,4,0);}
    std::swap(art.p,snowy.p);settings.grassSnow=false;const auto noSnow=render(true);
    CHECK(!GrassFXFixture::started(grass)&&Delta(noSnow,render(false))<=(msaa?2u:0u));
    settings.grassSnow=true;const auto snowGrass=render(true);CHECK(GrassFXFixture::started(grass)&&!GrassFXFixture::failed(grass));
    settings.grassGroundTint=true;
    const auto adaptedSnow=render(true);
    CHECK(adaptedSnow.size()==snowGrass.size()&&Delta(adaptedSnow,noSnow)>20&&GrassFXFixture::surface(grass,0,8)[3]<-1);
    settings.grassGroundTint=false;
    CHECK(GrassFXFixture::surface(grass,0,8)[1]>.99f&&GrassFXFixture::surface(grass,0,8)[3]<0&&Delta(noSnow,snowGrass)>20);
    CHECK(Save(noSnow,L"artifacts/grass/snow-grass-off.png")&&Save(snowGrass,L"artifacts/grass/snow-grass-on.png"));
    lightingCase=1;const auto pinkSnow=render(true);CHECK(Save(pinkSnow,L"artifacts/grass/snow-grass-sunset.png"));
    lightingCase=2;const auto nightSnow=render(true);CHECK(Save(nightSnow,L"artifacts/grass/snow-grass-night.png"));
    auto lightness=[](const std::vector<DWORD> &pixels){double sum=0;
        for(UINT y=400;y<650;++y)for(UINT x=150;x<850;++x){const DWORD p=pixels[y*Width+x];sum+=((p>>16)&255)*.2126+((p>>8)&255)*.7152+(p&255)*.0722;}return sum;};
    CHECK(lightness(nightSnow)<lightness(pinkSnow)*.65);lightingCase=0;
    settings.grassSnow=false;CHECK(Delta(noSnow,render(true))<=(msaa?2u:0u)&&!GrassFXFixture::started(grass));std::swap(art.p,snowy.p);
    settings.grassSnow=true;worldKey=42;std::swap(art.p,snowy.p);CHECK(render(true).size()==on.size());std::swap(art.p,snowy.p);
    const auto winterSoil=render(true);worldKey=43;const auto summerSoil=render(true);
    CHECK(Delta(winterSoil,summerSoil)>20);worldKey=0;settings.grassSnow=false;
    settings.grassHeight=.2f;auto shortGrass=render(true);settings.grassHeight=.95f;auto tallGrass=render(true);CHECK(shortGrass!=tallGrass);
    CHECK(Save(shortGrass,L"artifacts/grass/field-short.png")&&Save(tallGrass,L"artifacts/grass/field-tall.png"));
    settings.grassHeight=.55f;
    for(int q=0;q<3;++q){settings.grassQuality=float(q);auto result=render(true);CHECK(!GrassFXFixture::failed(grass)&&GrassFXFixture::count(grass)>0&&result!=off);}
    settings.grassQuality=2;settings.grassDistance=100;settings.grassDensity=3;
    CHECK(render(true).size()==on.size()&&!GrassFXFixture::failed(grass));CHECK(GrassFXFixture::count(grass)<=80000&&grass.bytes()<24*1024*1024);
    settings.grassDistance=40;settings.grassDensity=1.25f;settings.grassWind=.5f;
    const auto windy=render(true);Sleep(80);CHECK(windy!=render(true));settings.grassWind=0;
    settings.grassStyle=1;grass.invalidate();CHECK(!grass.bytes());
    CHECK(render(true).size()==on.size()&&!GrassFXFixture::failed(grass)&&GrassFXFixture::hasLeafArt(grass));
    CHECK(!stateErrors);
    puts("PASS: instanced grass on native buffered terrain with preserved source geometry.");return 0;
}
