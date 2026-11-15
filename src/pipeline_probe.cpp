#ifdef FENB_DIAGNOSTICS
#include "pipeline_probe.hpp"
#include "water.hpp"
#include <generated/probe_depth.hpp>
#include <generated/probe_vertices_vs.hpp>
#include <generated/probe_vertices_ps.hpp>
#include <cstdio>
#include <iomanip>

namespace {
constexpr UINT MaxDraws=12000, MaxTextures=768, MaxShaders=96;
constexpr size_t MaxBytes=24*1024*1024;
void Matrix(std::ostream& s,const D3DMATRIX& m) {
    s<<'[';for(int i=0;i<16;++i){if(i)s<<',';float v=reinterpret_cast<const float*>(&m)[i];s<<(std::isfinite(v)?v:0.f);}s<<']';
}
void Save(const std::wstring& path,const void* data,size_t bytes){FILE* f=_wfopen(path.c_str(),L"wb");if(f){fwrite(data,1,bytes,f);fclose(f);}}
}
PipelineProbe::~PipelineProbe(){if(active_)finish(true);}
std::string PipelineProbe::digest(const void* data,size_t rowBytes,UINT rows,UINT pitch){
    if(!hashAlgorithm_)return "";
    BCRYPT_HASH_HANDLE hash=nullptr;
    if(BCryptCreateHash(hashAlgorithm_,&hash,nullptr,0,nullptr,0,0)<0)return "";
    bool ok=true;
    for(UINT y=0;y<rows;++y)if(BCryptHashData(hash,(PUCHAR)data+size_t(y)*pitch,ULONG(rowBytes),0)<0){ok=false;break;}
    unsigned char bytes[32]{};if(ok)ok=BCryptFinishHash(hash,bytes,32,0)>=0;
    BCryptDestroyHash(hash);if(!ok)return "";
    char hex[65]{};for(int i=0;i<32;++i)sprintf(hex+i*2,"%02x",bytes[i]);return hex;
}
void PipelineProbe::begin(){
    active_=true;requested_=false;frame_=draws_=totalDraws_=0;truncated_=false;started_=GetTickCount();
    lines_.clear();lines_.reserve(1024*1024);textures_.clear();shaders_.clear();blobs_.clear();
    replayBytes_=0;waterDraws_=0;waterSource_=waterFinished_=false;surfaceFrames_=0;lastUP_=nullptr;lastApi_.clear();
    BCryptOpenAlgorithmProvider(&hashAlgorithm_,BCRYPT_SHA256_ALGORITHM,nullptr,0);
    D3DCAPS9 caps{};device_->GetDeviceCaps(&caps);
    D3DDEVICE_CREATION_PARAMETERS creation{};device_->GetCreationParameters(&creation);
    Com<IDirect3D9> root;device_->GetDirect3D(root.out());D3DDISPLAYMODE mode{};
    if(root.p)root->GetAdapterDisplayMode(creation.AdapterOrdinal,&mode);
    std::ostringstream out;
    out<<"{\"kind\":\"header\",\"schema\":2,\"mode\":\""<<(surfaceOnly_?"surface":"pipeline")<<"\",\"pid\":"<<GetCurrentProcessId()<<",\"vertexShaderVersion\":"<<caps.VertexShaderVersion
       <<",\"pixelShaderVersion\":"<<caps.PixelShaderVersion<<",\"maxVSConstants\":"<<caps.MaxVertexShaderConst
       <<",\"maxRTs\":"<<caps.NumSimultaneousRTs<<",\"creationFlags\":"<<creation.BehaviorFlags<<",\"depthTextureFormats\":[";
    DWORD formats[]={MAKEFOURCC('I','N','T','Z'),MAKEFOURCC('D','F','2','4'),MAKEFOURCC('R','A','W','Z')};
    for(int i=0;i<3;++i){if(i)out<<',';HRESULT hr=root.p?root->CheckDeviceFormat(creation.AdapterOrdinal,creation.DeviceType,mode.Format,D3DUSAGE_DEPTHSTENCIL,D3DRTYPE_TEXTURE,D3DFORMAT(formats[i])):E_FAIL;out<<"{\"format\":"<<formats[i]<<",\"hr\":"<<hr<<'}';}
    out<<"],\"note\":\"Driver support alone does not establish readable depth. Opt-in water replay records one GPU frame, at most 32 nonindexed water draws and 64 MiB of replay payload. No process memory or packets.\"}\n";
    lines_=out.str();Log("Pipeline probe: capture started (3 frames, bounded). ");
}
void PipelineProbe::finish(bool aborted){
    if(!active_)return;
    std::ostringstream out;out<<"{\"kind\":\"footer\",\"frames\":"<<frame_<<",\"drawsRecorded\":"<<draws_<<",\"drawsSeen\":"<<totalDraws_
        <<",\"truncated\":"<<(truncated_?"true":"false")<<",\"aborted\":"<<(aborted?"true":"false")<<",\"elapsedMs\":"<<GetTickCount()-started_<<"}\n";
    lines_+=out.str();
    auto base=ModuleDirectory()+L"FENB-captures";CreateDirectoryW(base.c_str(),nullptr);
    SYSTEMTIME time{};GetLocalTime(&time);wchar_t name[128]{};
    swprintf(name,128,L"\\capture-%04u%02u%02u-%02u%02u%02u-%lu-%lu",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,GetCurrentProcessId(),started_);
    auto dir=base+name;CreateDirectoryW(dir.c_str(),nullptr);
    Save(dir+L"\\frame.jsonl",lines_.data(),lines_.size());
    for(auto& b:blobs_){std::wstring file(b.name.begin(),b.name.end());Save(dir+L"\\"+file,b.bytes.data(),b.bytes.size());}
    for(auto* resource:retained_)resource->Release();retained_.clear();
    if(hashAlgorithm_){BCryptCloseAlgorithmProvider(hashAlgorithm_,0);hashAlgorithm_=nullptr;}
    active_=false;std::string().swap(lines_);blobs_.clear();textures_.clear();shaders_.clear();
    lastUP_=nullptr;lastApi_.clear();replayBytes_=0;
    Log("Pipeline probe: capture complete. See FENB-captures/frame.jsonl in the timestamped folder.");
}
void PipelineProbe::abort(){if(active_)finish(true);}
void PipelineProbe::boundary(){
    if(active_){++frame_;if(frame_>=3)finish(false);}
    if(!active_){
        DWORD now=GetTickCount();
        if(now-checked_>1000){
            checked_=now;auto marker=ModuleDirectory()+L"FENB-capture.request";
            if(GetFileAttributesW(marker.c_str())!=INVALID_FILE_ATTRIBUTES){DeleteFileW(marker.c_str());request();}
            marker=ModuleDirectory()+L"FENB-surface.request";
            if(GetFileAttributesW(marker.c_str())!=INVALID_FILE_ATTRIBUTES){DeleteFileW(marker.c_str());requestSurface();}
        }
        if(requested_)begin();
    }
}
int PipelineProbe::shader(IUnknown* object,bool pixel){
    if(!object)return 0;
    auto found=shaders_.find(object);if(found!=shaders_.end())return found->second;
    if(shaders_.size()>=MaxShaders){truncated_=true;return -1;}
    int id=int(shaders_.size())+1;shaders_[object]=id;object->AddRef();retained_.push_back(object);
    UINT size=0;HRESULT hr=pixel?static_cast<IDirect3DPixelShader9*>(object)->GetFunction(nullptr,&size):static_cast<IDirect3DVertexShader9*>(object)->GetFunction(nullptr,&size);
    Blob blob;blob.name=std::string(pixel?"ps-":"vs-")+std::to_string(id)+".bin";
    if(SUCCEEDED(hr)&&size<=65536){blob.bytes.resize(size);hr=pixel?static_cast<IDirect3DPixelShader9*>(object)->GetFunction(blob.bytes.data(),&size):static_cast<IDirect3DVertexShader9*>(object)->GetFunction(blob.bytes.data(),&size);}
    std::ostringstream out;out<<"{\"kind\":\"shader\",\"id\":"<<id<<",\"stage\":\""<<(pixel?"ps":"vs")<<"\",\"bytes\":"<<size<<",\"hr\":"<<hr<<",\"sha256\":\""<<digest(blob.bytes.data(),blob.bytes.size())<<"\",\"file\":\""<<blob.name<<"\"}\n";
    lines_+=out.str();if(!blob.bytes.empty())blobs_.push_back(std::move(blob));return id;
}
int PipelineProbe::texture(IDirect3DBaseTexture9* object){
    if(!object)return 0;
    auto found=textures_.find(object);if(found!=textures_.end())return found->second;
    if(textures_.size()>=MaxTextures){truncated_=true;return -1;}
    int id=int(textures_.size())+1;textures_[object]=id;object->AddRef();retained_.push_back(object);
    D3DSURFACE_DESC d{};HRESULT hr=E_NOINTERFACE;std::string hash;size_t row=0;UINT rows=0;
    if(object->GetType()==D3DRTYPE_TEXTURE){
        auto* tex=static_cast<IDirect3DTexture9*>(object);hr=tex->GetLevelDesc(0,&d);
        if(SUCCEEDED(hr)&&(d.Pool==D3DPOOL_MANAGED||d.Pool==D3DPOOL_SYSTEMMEM)&&!(d.Usage&D3DUSAGE_RENDERTARGET)){
            DWORD f=DWORD(d.Format);
            if(f==MAKEFOURCC('D','X','T','1')){row=((d.Width+3)/4)*8;rows=(d.Height+3)/4;}
            else if(f==MAKEFOURCC('D','X','T','3')||f==MAKEFOURCC('D','X','T','5')){row=((d.Width+3)/4)*16;rows=(d.Height+3)/4;}
            else if(d.Format==D3DFMT_A8R8G8B8||d.Format==D3DFMT_X8R8G8B8){row=size_t(d.Width)*4;rows=d.Height;}
            else if(d.Format==D3DFMT_R5G6B5||d.Format==D3DFMT_A1R5G5B5||d.Format==D3DFMT_A4R4G4B4){row=size_t(d.Width)*2;rows=d.Height;}
            D3DLOCKED_RECT lock{};
            if(row&&row*rows<=1024*1024&&SUCCEEDED(tex->LockRect(0,&lock,nullptr,D3DLOCK_READONLY))){if(lock.Pitch>0&&UINT(lock.Pitch)>=row)hash=digest(lock.pBits,row,rows,UINT(lock.Pitch));tex->UnlockRect(0);}
        }
    }
    std::ostringstream out;out<<"{\"kind\":\"texture\",\"id\":"<<id<<",\"type\":"<<object->GetType()<<",\"width\":"<<d.Width<<",\"height\":"<<d.Height
        <<",\"format\":"<<DWORD(d.Format)<<",\"pool\":"<<d.Pool<<",\"usage\":"<<d.Usage<<",\"hr\":"<<hr<<",\"firstMipSHA256\":\""<<hash<<"\"}\n";
    lines_+=out.str();return id;
}
bool PipelineProbe::addBlob(const std::string& name,const void* data,size_t bytes){
    constexpr size_t budget=64*1024*1024;
    if(bytes>budget-replayBytes_){truncated_=true;return false;}
    Blob blob;blob.name=name;blob.bytes.resize(bytes);memcpy(blob.bytes.data(),data,bytes);
    replayBytes_+=bytes;blobs_.push_back(std::move(blob));return true;
}
bool PipelineProbe::readSurface(IDirect3DSurface9* source,const std::string& name,const RECT* crop){
    D3DSURFACE_DESC d{};
    if(!source||FAILED(source->GetDesc(&d)))return false;
    UINT pixelBytes=d.Format==D3DFMT_A32B32G32R32F?16:d.Format==D3DFMT_A16B16G16R16F?8:4;
    if(d.Format!=D3DFMT_R32F&&d.Format!=D3DFMT_A32B32G32R32F&&d.Format!=D3DFMT_A16B16G16R16F&&d.Format!=D3DFMT_A8R8G8B8&&d.Format!=D3DFMT_X8R8G8B8)return false;
    RECT area=crop?*crop:RECT{0,0,LONG(d.Width),LONG(d.Height)};
    if(area.left<0 || area.top<0 || area.right>LONG(d.Width) || area.bottom>LONG(d.Height) || area.right<=area.left || area.bottom<=area.top)return false;
    UINT width=UINT(area.right-area.left),height=UINT(area.bottom-area.top);
    size_t bytes=size_t(width)*height*pixelBytes;
    if(bytes>64*1024*1024-replayBytes_){truncated_=true;return false;}
    Com<IDirect3DSurface9> resolve,read;
    if(d.MultiSampleType!=D3DMULTISAMPLE_NONE){
        if(FAILED(device_->CreateRenderTarget(d.Width,d.Height,d.Format,D3DMULTISAMPLE_NONE,0,FALSE,resolve.out(),nullptr))||
           FAILED(device_->StretchRect(source,nullptr,resolve,nullptr,D3DTEXF_NONE)))return false;
        source=resolve;
    }
    if(FAILED(device_->CreateOffscreenPlainSurface(d.Width,d.Height,d.Format,D3DPOOL_SYSTEMMEM,read.out(),nullptr))||
       FAILED(device_->GetRenderTargetData(source,read)))return false;
    D3DLOCKED_RECT lock{};if(FAILED(read->LockRect(&lock,nullptr,D3DLOCK_READONLY)))return false;
    std::vector<unsigned char> pixels(bytes);bool ok=lock.Pitch>=int(d.Width*pixelBytes);
    if(ok)for(UINT y=0;y<height;++y)memcpy(pixels.data()+size_t(y)*width*pixelBytes,
        (BYTE*)lock.pBits+size_t(y+area.top)*lock.Pitch+size_t(area.left)*pixelBytes,width*pixelBytes);
    read->UnlockRect();if(!ok||!addBlob(name,pixels.data(),pixels.size()))return false;
    std::ostringstream out;out<<"{\"kind\":\""<<(name.starts_with("surface-")?"surface-image":"water-image")<<"\",\"file\":\""<<name<<"\",\"width\":"<<width<<",\"height\":"<<height<<",\"format\":"<<DWORD(d.Format)<<",\"pixelBytes\":"<<pixelBytes
        <<",\"x\":"<<area.left<<",\"y\":"<<area.top<<",\"sourceWidth\":"<<d.Width<<",\"sourceHeight\":"<<d.Height<<"}\n";lines_+=out.str();return true;
}
namespace {
void ProbeTarget(IDirect3DDevice9* d,IDirect3DSurface9* target,UINT w,UINT h){
    d->SetRenderTarget(0,target);D3DVIEWPORT9 viewport{0,0,w,h,0,1};d->SetViewport(&viewport);
    for(auto key:{D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_ALPHATESTENABLE,
                  D3DRS_FOGENABLE,D3DRS_STENCILENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_SRGBWRITEENABLE,
                  D3DRS_CLIPPLANEENABLE,D3DRS_WRAP0,D3DRS_LIGHTING,D3DRS_POINTSPRITEENABLE,D3DRS_POINTSCALEENABLE})d->SetRenderState(key,0);
    d->SetRenderState(D3DRS_COLORWRITEENABLE,15);d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    d->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);d->SetRenderState(D3DRS_MULTISAMPLEMASK,0xffffffff);
    float point=1;DWORD pointBits=0;memcpy(&pointBits,&point,4);d->SetRenderState(D3DRS_POINTSIZE,pointBits);
    d->SetTexture(0,nullptr);
}
}
void PipelineProbe::readDepth(IDirect3DTexture9* source,const std::string& name,const RECT* crop){
    D3DSURFACE_DESC desc{};if(!source||FAILED(source->GetLevelDesc(0,&desc)))return;
    if(size_t(desc.Width)*desc.Height*4>64*1024*1024-replayBytes_){truncated_=true;return;}
    StateGuard guard(device_);Com<IDirect3DTexture9> target;Com<IDirect3DSurface9> surface;Com<IDirect3DPixelShader9> ps;
    if(!guard.valid()||FAILED(device_->CreateTexture(desc.Width,desc.Height,1,D3DUSAGE_RENDERTARGET,D3DFMT_R32F,D3DPOOL_DEFAULT,target.out(),nullptr))||
       FAILED(target->GetSurfaceLevel(0,surface.out()))||FAILED(device_->CreatePixelShader(ShaderProbeDepth,ps.out())))return;
    guard.isolateTargets();ProbeTarget(device_,surface,desc.Width,desc.Height);
    device_->SetVertexShader(nullptr);device_->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);device_->SetPixelShader(ps);device_->SetTexture(0,source);
    for(auto key:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER})device_->SetSamplerState(0,key,D3DTEXF_POINT);
    device_->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);device_->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);
    device_->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);device_->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
    struct V{float x,y,z,w,u,v;};float w=float(desc.Width),h=float(desc.Height);
    V quad[]={{-.5f,-.5f,0,1,0,0},{w-.5f,-.5f,0,1,1,0},{-.5f,h-.5f,0,1,0,1},{w-.5f,h-.5f,0,1,1,1}};
    if(SUCCEEDED(device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(V))))readSurface(surface,name,crop);
}
void PipelineProbe::surface(IDirect3DTexture9* before,IDirect3DTexture9* mask,IDirect3DTexture9* depth,IDirect3DSurface9* after){
    if(!active_ || !surfaceOnly_ || frame_>=3 || (surfaceFrames_&(1u<<frame_)) || !before || !mask || !depth || !after)return;
    surfaceFrames_|=1u<<frame_;
    D3DSURFACE_DESC desc{};if(FAILED(after->GetDesc(&desc)))return;
    // Three bounded lower-center crops retain native pixel coordinates and precision.
    UINT width=std::min(960u,desc.Width),height=std::min(640u,desc.Height);
    LONG x=LONG((desc.Width-width)/2),y=LONG((desc.Height-height)*2/3);
    RECT crop{x,y,x+LONG(width),y+LONG(height)};
    auto prefix="surface-"+std::to_string(frame_);
    constexpr UINT uniformRegisters=61; // Includes c60 native depth quantization controls.
    float constants[uniformRegisters*4]{};
    bool uniforms=SUCCEEDED(device_->GetPixelShaderConstantF(0,constants,uniformRegisters)) &&
        addBlob(prefix+"-ps.fp32",constants,sizeof(constants));
    Com<IDirect3DSurface9> input,material;
    bool color=SUCCEEDED(before->GetSurfaceLevel(0,input.out()))&&readSurface(input,prefix+"-before.argb",&crop);
    bool receiver=SUCCEEDED(mask->GetSurfaceLevel(0,material.out()))&&readSurface(material,prefix+"-mask.rgba16f",&crop);
    readDepth(depth,prefix+"-depth.r32f",&crop);
    bool result=readSurface(after,prefix+"-after.argb",&crop);
    std::ostringstream out;out<<"{\"kind\":\"surface-frame\",\"frame\":"<<frame_<<",\"prefix\":\""<<prefix
        <<"\",\"uniforms\":"<<(uniforms?"true":"false")<<",\"uniformRegisters\":"<<uniformRegisters<<",\"color\":"<<(color?"true":"false")
        <<",\"mask\":"<<(receiver?"true":"false")<<",\"result\":"<<(result?"true":"false")<<"}\n";
    lines_+=out.str();
}
bool PipelineProbe::readVertices(const std::string& name,UINT count){
    if(lastUP_){
        if(lastStride_<12)return false;
        std::vector<float> positions(size_t(count)*4);
        for(UINT i=0;i<count;++i){memcpy(&positions[i*4],(const BYTE*)lastUP_+size_t(i)*lastStride_,12);positions[i*4+3]=1;}
        return addBlob(name,positions.data(),positions.size()*4);
    }
    Com<IDirect3DVertexBuffer9> native,indices;UINT offset=0,stride=0;D3DVERTEXBUFFER_DESC buffer{};
    if(FAILED(device_->GetStreamSource(0,native.out(),&offset,&stride))||!native.p||stride!=36||FAILED(native->GetDesc(&buffer))||
       uint64_t(offset)+uint64_t(lastFirst_+uint64_t(count))*stride>buffer.Size)return false;
    // Capture write-only native buffers through the GPU, never through a READONLY lock.
    UINT width=std::min(256u,count),height=(count+width-1)/width;
    StateGuard guard(device_);Com<IDirect3DTexture9> target;Com<IDirect3DSurface9> surface;
    Com<IDirect3DVertexShader9> vs;Com<IDirect3DPixelShader9> ps;Com<IDirect3DVertexDeclaration9> decl;
    if(!guard.valid()||FAILED(device_->CreateTexture(width,height,1,D3DUSAGE_RENDERTARGET,D3DFMT_A32B32G32R32F,D3DPOOL_DEFAULT,target.out(),nullptr))||
       FAILED(target->GetSurfaceLevel(0,surface.out()))||FAILED(device_->CreateVertexShader(ShaderProbeVerticesVS,vs.out()))||
       FAILED(device_->CreatePixelShader(ShaderProbeVerticesPS,ps.out()))||
       FAILED(device_->CreateVertexBuffer(count*8,0,0,D3DPOOL_MANAGED,indices.out(),nullptr)))return false;
    D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
                               {1,0,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},D3DDECL_END()};
    if(FAILED(device_->CreateVertexDeclaration(elements,decl.out())))return false;
    float* destinations=nullptr;if(FAILED(indices->Lock(0,0,(void**)&destinations,0)))return false;
    for(UINT i=0;i<count;++i){destinations[i*2]=float(i%width)*2/width-1;destinations[i*2+1]=1-float(i/width)*2/height;}
    indices->Unlock();guard.isolateTargets();ProbeTarget(device_,surface,width,height);
    device_->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);device_->SetVertexDeclaration(decl);
    device_->SetVertexShader(vs);device_->SetPixelShader(ps);
    device_->SetStreamSource(0,native,offset+lastFirst_*stride,stride);device_->SetStreamSource(1,indices,0,8);
    UINT freq[2]={1,1};device_->GetStreamSourceFreq(0,&freq[0]);device_->GetStreamSourceFreq(1,&freq[1]);
    device_->SetStreamSourceFreq(0,1);device_->SetStreamSourceFreq(1,1);
    bool ok=SUCCEEDED(device_->DrawPrimitive(D3DPT_POINTLIST,0,count))&&readSurface(surface,name);
    device_->SetStreamSourceFreq(0,freq[0]);device_->SetStreamSourceFreq(1,freq[1]);return ok;
}
void PipelineProbe::water(WaterFX& water){
    if(!active_||surfaceOnly_||frame_!=0||!water.state_||lastApi_.empty())return;
    if(waterDraws_>=32){truncated_=true;return;}
    if(!waterSource_){
        waterSource_=true;readSurface(water.sceneSurface_,"water-color.argb");readDepth(water.depth_.texture());
    }
    if(lastApi_!="DrawPrimitive"&&lastApi_!="DrawPrimitiveUP")return;
    UINT count=0;
    switch(lastType_){case D3DPT_TRIANGLELIST:if(lastPrimitives_>21845)return;count=lastPrimitives_*3;break;
      case D3DPT_TRIANGLESTRIP:case D3DPT_TRIANGLEFAN:if(lastPrimitives_>65534)return;count=lastPrimitives_+2;break;default:return;}
    if(!count)return;
    auto prefix="water-"+std::to_string(waterDraws_++);
    float vs[18*4]{},ps[23*4]{};D3DVIEWPORT9 viewport{};
    if(FAILED(device_->GetVertexShaderConstantF(0,vs,18))||FAILED(device_->GetPixelShaderConstantF(0,ps,23))||FAILED(device_->GetViewport(&viewport)))return;
    bool mesh=readVertices(prefix+"-positions.fp32",count);
    bool constants=addBlob(prefix+"-vs.fp32",vs,sizeof(vs))&&addBlob(prefix+"-ps.fp32",ps,sizeof(ps));
    Com<IDirect3DSurface9> nativeDepth;D3DSURFACE_DESC depthDescription{};
    if(SUCCEEDED(device_->GetDepthStencilSurface(nativeDepth.out())) && nativeDepth.p)
        nativeDepth->GetDesc(&depthDescription);
    std::ostringstream out;out<<"{\"kind\":\"water-replay\",\"prefix\":\""<<prefix<<"\",\"draw\":"<<draws_<<",\"type\":"<<lastType_<<",\"primitives\":"<<lastPrimitives_
       <<",\"vertices\":"<<count<<",\"mesh\":"<<(mesh?"true":"false")<<",\"constants\":"<<(constants?"true":"false")
       <<",\"pixelConstants\":23,\"reflectionEncoding\":\"premultiplied-rgba\",\"nativeDepthFormat\":"<<DWORD(depthDescription.Format)
       <<",\"viewport\":["<<viewport.X<<','<<viewport.Y<<','<<viewport.Width<<','<<viewport.Height<<','<<viewport.MinZ<<','<<viewport.MaxZ<<"]}\n";
    lines_+=out.str();lastUP_=nullptr;
}
void PipelineProbe::waterEnd(WaterFX& water){
    if(!active_||surfaceOnly_||frame_!=0||!waterSource_||waterFinished_)return;waterFinished_=true;
    StateGuard guard(device_);if(!guard.valid())return;
    readSurface(water.reflectionSurface_,"water-reflection.argb");
    Com<IDirect3DSurface9> bb;if(SUCCEEDED(device_->GetRenderTarget(0,bb.out())))readSurface(bb,"water-final.argb");
}
void PipelineProbe::draw(const char* api,UINT type,UINT primitives,UINT vertices,UINT stride,const void* up,UINT first){
    if(!active_)return;++totalDraws_;
    if(draws_>=MaxDraws||lines_.size()>MaxBytes){lastApi_.clear();lastUP_=nullptr;truncated_=true;return;}
    if(up&&!vertices){switch(type){case D3DPT_POINTLIST:vertices=primitives;break;case D3DPT_LINELIST:vertices=primitives*2;break;case D3DPT_LINESTRIP:vertices=primitives+1;break;case D3DPT_TRIANGLELIST:vertices=primitives*3;break;case D3DPT_TRIANGLESTRIP:case D3DPT_TRIANGLEFAN:vertices=primitives+2;break;}}
    lastApi_=api;lastType_=type;lastPrimitives_=primitives;lastVertices_=vertices;lastStride_=stride;lastFirst_=first;lastUP_=up;
    ++draws_;std::ostringstream s;s<<std::setprecision(7);
    DWORD fvf=0;HRESULT fvfHr=device_->GetFVF(&fvf);
    Com<IDirect3DVertexShader9> vs;Com<IDirect3DPixelShader9> ps;device_->GetVertexShader(vs.out());device_->GetPixelShader(ps.out());
    int vsId=shader(vs,false),psId=shader(ps,true);
    int texIds[4]{};for(UINT i=0;i<4;++i){Com<IDirect3DBaseTexture9> tex;if(SUCCEEDED(device_->GetTexture(i,tex.out())))texIds[i]=texture(tex);}
    s<<"{\"kind\":\"draw\",\"frame\":"<<frame_<<",\"index\":"<<draws_<<",\"api\":\""<<api<<"\",\"primitiveType\":"<<type<<",\"primitives\":"<<primitives
     <<",\"vertices\":"<<vertices<<",\"firstVertex\":"<<first<<",\"upStride\":"<<stride<<",\"fvf\":"<<fvf<<",\"fvfHR\":"<<fvfHr<<",\"vs\":"<<vsId<<",\"ps\":"<<psId<<",\"textures\":[";
    for(int i=0;i<4;++i){if(i)s<<',';s<<texIds[i];}s<<"],\"transforms\":{";
    D3DTRANSFORMSTATETYPE transforms[]={D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION,D3DTS_TEXTURE0};const char* labels[]={"world","view","projection","texture0"};
    for(int i=0;i<4;++i){D3DMATRIX m{};HRESULT hr=device_->GetTransform(transforms[i],&m);if(i)s<<',';s<<'"'<<labels[i]<<"\":{\"hr\":"<<hr<<",\"m\":";Matrix(s,m);s<<'}';}s<<"},\"states\":{";
    D3DRENDERSTATETYPE states[]={D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ZFUNC,D3DRS_ALPHABLENDENABLE,D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_ALPHATESTENABLE,D3DRS_ALPHAREF,D3DRS_CULLMODE,D3DRS_LIGHTING,D3DRS_AMBIENT,D3DRS_FOGENABLE,D3DRS_FOGCOLOR,D3DRS_FOGTABLEMODE,D3DRS_FOGVERTEXMODE,D3DRS_FOGSTART,D3DRS_FOGEND,D3DRS_FOGDENSITY,D3DRS_STENCILENABLE,D3DRS_SRGBWRITEENABLE};
    const char* names[]={"zEnable","zWrite","zFunc","blend","srcBlend","dstBlend","alphaTest","alphaRef","cull","lighting","ambient","fogEnable","fogColor","fogTable","fogVertex","fogStartBits","fogEndBits","fogDensityBits","stencil","sRGB"};
    for(int i=0;i<20;++i){DWORD value=0;HRESULT hr=device_->GetRenderState(states[i],&value);if(i)s<<',';s<<'"'<<names[i]<<"\":["<<hr<<','<<value<<']';}s<<"},\"stages\":[";
    for(DWORD stage=0;stage<2;++stage){if(stage)s<<',';s<<'[';D3DTEXTURESTAGESTATETYPE st[]={D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,D3DTSS_ALPHAOP,D3DTSS_TEXCOORDINDEX,D3DTSS_TEXTURETRANSFORMFLAGS};for(int j=0;j<6;++j){DWORD v=0;device_->GetTextureStageState(stage,st[j],&v);if(j)s<<',';s<<v;}s<<']';}s<<']';
    Com<IDirect3DVertexBuffer9> vb;UINT offset=0,streamStride=0;D3DVERTEXBUFFER_DESC vbDesc{};
    HRESULT streamHr=device_->GetStreamSource(0,vb.out(),&offset,&streamStride);if(vb.p)vb->GetDesc(&vbDesc);
    s<<",\"stream0\":{\"hr\":"<<streamHr<<",\"stride\":"<<streamStride<<",\"offset\":"<<offset<<",\"bytes\":"<<vbDesc.Size<<",\"usage\":"<<vbDesc.Usage<<",\"pool\":"<<vbDesc.Pool<<'}';
    Com<IDirect3DVertexDeclaration9> decl;UINT count=MAXD3DDECLLENGTH+1;D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH+1]{};
    s<<",\"declaration\":[";
    if(SUCCEEDED(device_->GetVertexDeclaration(decl.out()))&&decl.p&&SUCCEEDED(decl->GetDeclaration(elements,&count))){for(UINT i=0;i<count&&i<MAXD3DDECLLENGTH;++i){if(elements[i].Stream==0xff)break;if(i)s<<',';auto& e=elements[i];s<<'['<<e.Stream<<','<<e.Offset<<','<<UINT(e.Type)<<','<<UINT(e.Usage)<<','<<UINT(e.UsageIndex)<<']';}}s<<']';
    Com<IDirect3DSurface9> rt,depth;D3DSURFACE_DESC rtDesc{},depthDesc{};device_->GetRenderTarget(0,rt.out());HRESULT depthHr=device_->GetDepthStencilSurface(depth.out());if(rt.p)rt->GetDesc(&rtDesc);if(depth.p)depth->GetDesc(&depthDesc);
    s<<",\"target\":["<<rtDesc.Width<<','<<rtDesc.Height<<','<<DWORD(rtDesc.Format)<<','<<rtDesc.MultiSampleType<<"],\"depth\":{\"hr\":"<<depthHr<<",\"format\":"<<DWORD(depthDesc.Format)<<",\"msaa\":"<<depthDesc.MultiSampleType<<'}';
    D3DLIGHT9 light{};BOOL enabled=FALSE;HRESULT lightHr=device_->GetLight(0,&light);device_->GetLightEnable(0,&enabled);
    s<<",\"light0\":{\"hr\":"<<lightHr<<",\"enabled\":"<<(enabled?"true":"false")<<",\"type\":"<<light.Type<<",\"direction\":["<<light.Direction.x<<','<<light.Direction.y<<','<<light.Direction.z<<"]}";
    D3DMATERIAL9 material{};HRESULT matHr=device_->GetMaterial(&material);s<<",\"material\":{\"hr\":"<<matHr<<",\"diffuse\":["<<material.Diffuse.r<<','<<material.Diffuse.g<<','<<material.Diffuse.b<<','<<material.Diffuse.a<<"]}";
    if(up&&stride>=12&&vertices&&(fvf&D3DFVF_POSITION_MASK)==D3DFVF_XYZ){float xyz[3]{};memcpy(xyz,up,12);s<<",\"firstUPPosition\":["<<xyz[0]<<','<<xyz[1]<<','<<xyz[2]<<']';}
    s<<"}\n";lines_+=s.str();
}
#endif
