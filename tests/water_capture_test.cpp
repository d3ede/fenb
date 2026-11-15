#include "pipeline_probe.hpp"
#include "water.hpp"
#include <cstdio>
#include <cstring>
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
std::wstring ModuleDirectory(){return L"build\\water-capture-test\\";}
void Log(const char* text){puts(text);}
struct PipelineProbeFixture {
    static auto& blobs(PipelineProbe& p){return p.blobs_;}
    static void depth(PipelineProbe& p,IDirect3DTexture9* d){p.readDepth(d);}
    static bool vertices(PipelineProbe& p,UINT count){return p.readVertices("test-positions.fp32",count);}
    static bool payload(PipelineProbe& p,size_t bytes){unsigned char zero=0;return p.addBlob("rejected.bin",&zero,bytes);}
    static void exhaustDraws(PipelineProbe& p){p.draws_=12000;}
    static bool crop(PipelineProbe& p,IDirect3DSurface9* s,const RECT& r){return p.readSurface(s,"surface-crop.rgba16f",&r);}
    static void allowPoll(PipelineProbe& p){p.checked_=GetTickCount()-1001;}
    static bool surfaceMode(PipelineProbe& p){return p.active_&&p.surfaceOnly_;}
};
int main(){
    CreateDirectoryW(L"build\\water-capture-test",nullptr);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FENBWaterCaptureTest";RegisterClassW(&wc);
    HWND window=CreateWindowW(wc.lpszClassName,L"Water capture fixture",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,wc.hInstance,nullptr);
    Com<IDirect3D9> root;root.p=Direct3DCreate9(D3D_SDK_VERSION);CHECK(root.p);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
    pp.BackBufferWidth=320;pp.BackBufferHeight=240;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24X8;
    Com<IDirect3DDevice9> device;CHECK(SUCCEEDED(root->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,device.out())));
    struct V{float x,y,z,nx,ny,nz;DWORD color;float u,v;};
    V data[7]{};for(int i=0;i<7;++i){data[i].x=7000.f+i*.125f;data[i].y=100.f-i*.25f;data[i].z=3300.f+i*.5f;}
    data[3]={6998,100,3301};data[4]={6998,100,3305};data[5]={7002,100,3301};data[6]={7002,100,3305};
    Com<IDirect3DVertexBuffer9> vb;CHECK(SUCCEEDED(device->CreateVertexBuffer(sizeof(data),D3DUSAGE_WRITEONLY,0x152,D3DPOOL_MANAGED,vb.out(),nullptr)));
    void* write=nullptr;CHECK(SUCCEEDED(vb->Lock(0,0,&write,0)));memcpy(write,data,sizeof(data));vb->Unlock();
    DepthCopy depth(device);device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff205080,.37f,0);
    if(!depth.capture(320,240)){puts("SKIP: optional NVIDIA depth backend unavailable");DestroyWindow(window);return 0;}
    PipelineProbe probe(device);probe.request();probe.boundary();
    CHECK(SUCCEEDED(device->BeginScene()));
    device->SetStreamSource(0,vb,sizeof(V),sizeof(V));device->SetFVF(0x152);
    float constants[4]={9,8,7,6};device->SetPixelShaderConstantF(0,constants,1);
    probe.draw("DrawPrimitive",D3DPT_TRIANGLESTRIP,2,0,0,nullptr,2);
    CHECK(PipelineProbeFixture::vertices(probe,4));
    PipelineProbeFixture::depth(probe,depth.texture());
    Com<IDirect3DVertexBuffer9> restored;UINT offset=0,stride=0;device->GetStreamSource(0,restored.out(),&offset,&stride);
    CHECK(restored.p==vb.p&&offset==sizeof(V)&&stride==sizeof(V));
    DWORD fvf=0;device->GetFVF(&fvf);CHECK(fvf==0x152);
    float values[4]{};device->GetPixelShaderConstantF(0,values,1);CHECK(!memcmp(values,constants,16));
    CHECK(SUCCEEDED(device->EndScene()));
    // Exercise the full bundle hook with an already identified water material.
    Com<IDirect3DTexture9> material;CHECK(SUCCEEDED(device->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,material.out(),nullptr)));
    D3DLOCKED_RECT lock{};CHECK(SUCCEEDED(material->LockRect(0,&lock,nullptr,0)));*(DWORD*)lock.pBits=0xffffffff;material->UnlockRect(0);
    DWORD tag=1;material->SetPrivateData(WaterTag,&tag,4,0);
    WaterFX water(device);Settings settings;settings.waterSSR=1;
    D3DMATRIX id{};id._11=id._22=id._33=id._44=1;
    auto projection=id;projection._33=100.f/99.9f;projection._34=1;projection._43=-.1f*projection._33;projection._44=0;
    auto view=id;view._41=-7000;view._42=-102;view._43=-3297;
    device->SetTransform(D3DTS_WORLD,&id);device->SetTransform(D3DTS_VIEW,&view);device->SetTransform(D3DTS_PROJECTION,&projection);
    device->Clear(0,nullptr,D3DCLEAR_ZBUFFER,0,projection._33+projection._43/8,0);
    device->SetRenderState(D3DRS_ZENABLE,TRUE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    device->SetTexture(0,material);CHECK(SUCCEEDED(device->BeginScene()));
    probe.draw("DrawPrimitive",D3DPT_TRIANGLESTRIP,2,0,0,nullptr,2);
    CHECK(water.begin(settings,sizeof(V)));water.reflect([&](){return device->DrawPrimitive(D3DPT_TRIANGLESTRIP,2,2);});
    probe.water(water);water.end();probe.waterEnd(water);
    CHECK(SUCCEEDED(device->EndScene()));
    bool positions=false,depthPixels=false;
    bool colorPixels=false,reflectionPixels=false,finalPixels=false,uniforms=false,mesh=false;
    for(auto& blob:PipelineProbeFixture::blobs(probe)){
        if(blob.name=="test-positions.fp32"){
            CHECK(blob.bytes.size()>=64);float read[16]{};memcpy(read,blob.bytes.data(),64);
            for(int i=0;i<4;++i){CHECK(read[i*4]==data[i+3].x&&read[i*4+1]==data[i+3].y&&read[i*4+2]==data[i+3].z&&read[i*4+3]==1);}
            positions=true;
        }
        if(blob.name=="water-depth.r32f"&&!depthPixels){
            float z=0;CHECK(blob.bytes.size()==320*240*4);memcpy(&z,blob.bytes.data()+123*4,4);CHECK(std::abs(z-.37f)<.000001f);depthPixels=true;
        }
        colorPixels|=blob.name=="water-color.argb";reflectionPixels|=blob.name=="water-reflection.argb";
        finalPixels|=blob.name=="water-final.argb";uniforms|=blob.name=="water-0-vs.fp32";
        mesh|=blob.name=="water-0-positions.fp32";
        if(blob.name=="water-0-ps.fp32") CHECK(blob.bytes.size()==23*16);
        if(blob.name=="water-reflection.argb"){
            unsigned hits=0;for(size_t i=3;i<blob.bytes.size();i+=4)hits+=blob.bytes[i]>20;
            CHECK(hits>50);
        }
    }
    CHECK(positions&&depthPixels&&colorPixels&&reflectionPixels&&finalPixels&&uniforms&&mesh);
    auto blobCount=PipelineProbeFixture::blobs(probe).size();PipelineProbeFixture::exhaustDraws(probe);
    probe.draw("DrawPrimitive",D3DPT_TRIANGLELIST,1000000,0,0,nullptr,1000000);probe.water(water);
    CHECK(PipelineProbeFixture::blobs(probe).size()==blobCount);
    CHECK(!PipelineProbeFixture::payload(probe,65*1024*1024));
    probe.boundary();probe.boundary();probe.boundary();
    CHECK(PipelineProbeFixture::blobs(probe).empty());
    Com<IDirect3DTexture9> surfaceColor,surfaceMask,preciseMask;
    Com<IDirect3DSurface9> colorTarget,maskTarget,preciseTarget,originalTarget;
    CHECK(SUCCEEDED(device->CreateTexture(320,240,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,surfaceColor.out(),nullptr)));
    CHECK(SUCCEEDED(device->CreateTexture(320,240,1,D3DUSAGE_RENDERTARGET,D3DFMT_A16B16G16R16F,D3DPOOL_DEFAULT,surfaceMask.out(),nullptr)));
    CHECK(SUCCEEDED(device->CreateTexture(320,240,1,D3DUSAGE_RENDERTARGET,D3DFMT_A32B32G32R32F,D3DPOOL_DEFAULT,preciseMask.out(),nullptr)));
    CHECK(SUCCEEDED(preciseMask->GetSurfaceLevel(0,preciseTarget.out())));
    CHECK(SUCCEEDED(surfaceColor->GetSurfaceLevel(0,colorTarget.out()))&&SUCCEEDED(surfaceMask->GetSurfaceLevel(0,maskTarget.out())));
    device->GetRenderTarget(0,originalTarget.out());
    device->SetDepthStencilSurface(nullptr);device->SetRenderTarget(0,colorTarget);
    CHECK(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff205080,1,0)));
    device->SetRenderTarget(0,maskTarget);CHECK(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff102030,1,0)));
    device->SetRenderTarget(0,preciseTarget);CHECK(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff102030,1,0)));
    device->SetRenderTarget(0,originalTarget);
    const auto marker=ModuleDirectory()+L"FENB-surface.request";
    FILE* request=_wfopen(marker.c_str(),L"wb");CHECK(request);fclose(request);
    PipelineProbeFixture::allowPoll(probe);probe.boundary();
    CHECK(PipelineProbeFixture::surfaceMode(probe)&&GetFileAttributesW(marker.c_str())==INVALID_FILE_ATTRIBUTES);
    CHECK(PipelineProbeFixture::crop(probe,maskTarget,RECT{5,7,15,17}));
    CHECK(PipelineProbeFixture::blobs(probe).back().bytes.size()==10*10*8);
    CHECK(!PipelineProbeFixture::crop(probe,maskTarget,RECT{-1,0,15,17}));
    for(int frame=0;frame<3;++frame) {
        const auto previous=PipelineProbeFixture::blobs(probe).size();
        CHECK(SUCCEEDED(device->BeginScene()));
        device->SetPixelShaderConstantF(0,constants,1);
        const float precision[]={1.f/65535.f,1,.00015f,.025f};
        device->SetPixelShaderConstantF(60,precision,1);
        auto *frameMask=frame==2?preciseMask.p:surfaceMask.p;
        probe.surface(surfaceColor,frameMask,depth.texture(),colorTarget);
        CHECK(PipelineProbeFixture::blobs(probe).size()==previous+5);
        const auto &uniforms=PipelineProbeFixture::blobs(probe)[previous];
        CHECK(uniforms.name=="surface-"+std::to_string(frame)+"-ps.fp32" && uniforms.bytes.size()==61*16);
        CHECK(!memcmp(uniforms.bytes.data()+60*16,precision,16));
        probe.surface(surfaceColor,frameMask,depth.texture(),colorTarget);
        CHECK(PipelineProbeFixture::blobs(probe).size()==previous+5);
        device->GetPixelShaderConstantF(0,values,1);CHECK(!memcmp(values,constants,16));
        device->GetPixelShaderConstantF(60,values,1);CHECK(!memcmp(values,precision,16));
        const auto &mask=PipelineProbeFixture::blobs(probe)[previous+2];
        CHECK(mask.bytes.size()==320*240*(frame==2?16:8));
        if(frame==2){float alpha=0;memcpy(&alpha,mask.bytes.data()+12,4);CHECK(alpha==1);}
        else {unsigned short alpha=0;memcpy(&alpha,mask.bytes.data()+6,2);CHECK(alpha==0x3c00);}
        CHECK(SUCCEEDED(device->EndScene()));probe.boundary();
    }
    CHECK(PipelineProbeFixture::blobs(probe).empty());
    DestroyWindow(window);puts("PASS: write-only GPU vertex extraction with native offsets, exact R32F depth, state restoration, bounds and cleanup");
    return 0;
}
