#ifdef FENB_DIAGNOSTICS
#include "pipeline_probe.hpp"
#include <cstdio>
#include <iomanip>

namespace {
constexpr UINT MaxDraws=12000, MaxTextures=192, MaxShaders=96;
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
    BCryptOpenAlgorithmProvider(&hashAlgorithm_,BCRYPT_SHA256_ALGORITHM,nullptr,0);
    D3DCAPS9 caps{};device_->GetDeviceCaps(&caps);
    D3DDEVICE_CREATION_PARAMETERS creation{};device_->GetCreationParameters(&creation);
    Com<IDirect3D9> root;device_->GetDirect3D(root.out());D3DDISPLAYMODE mode{};
    if(root.p)root->GetAdapterDisplayMode(creation.AdapterOrdinal,&mode);
    std::ostringstream out;
    out<<"{\"kind\":\"header\",\"schema\":1,\"pid\":"<<GetCurrentProcessId()<<",\"vertexShaderVersion\":"<<caps.VertexShaderVersion
       <<",\"pixelShaderVersion\":"<<caps.PixelShaderVersion<<",\"maxVSConstants\":"<<caps.MaxVertexShaderConst
       <<",\"maxRTs\":"<<caps.NumSimultaneousRTs<<",\"creationFlags\":"<<creation.BehaviorFlags<<",\"depthTextureFormats\":[";
    DWORD formats[]={MAKEFOURCC('I','N','T','Z'),MAKEFOURCC('D','F','2','4'),MAKEFOURCC('R','A','W','Z')};
    for(int i=0;i<3;++i){if(i)out<<',';HRESULT hr=root.p?root->CheckDeviceFormat(creation.AdapterOrdinal,creation.DeviceType,mode.Format,D3DUSAGE_DEPTHSTENCIL,D3DRTYPE_TEXTURE,D3DFORMAT(formats[i])):E_FAIL;out<<"{\"format\":"<<formats[i]<<",\"hr\":"<<hr<<'}';}
    out<<"],\"note\":\"Driver support is not evidence that the game depth buffer is sampleable. No GPU readback or state mutation.\"}\n";
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
    Log("Pipeline probe: capture complete. See FENB-captures/frame.jsonl in the timestamped folder.");
}
void PipelineProbe::abort(){if(active_)finish(true);}
void PipelineProbe::boundary(){
    if(active_){++frame_;if(frame_>=3)finish(false);}
    if(!active_){
        DWORD now=GetTickCount();
        if(now-checked_>1000){checked_=now;auto marker=ModuleDirectory()+L"FENB-capture.request";if(GetFileAttributesW(marker.c_str())!=INVALID_FILE_ATTRIBUTES){DeleteFileW(marker.c_str());requested_=true;}}
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
void PipelineProbe::draw(const char* api,UINT type,UINT primitives,UINT vertices,UINT stride,const void* up){
    if(!active_)return;++totalDraws_;
    if(draws_>=MaxDraws||lines_.size()>MaxBytes){truncated_=true;return;}
    if(up&&!vertices){switch(type){case D3DPT_POINTLIST:vertices=primitives;break;case D3DPT_LINELIST:vertices=primitives*2;break;case D3DPT_LINESTRIP:vertices=primitives+1;break;case D3DPT_TRIANGLELIST:vertices=primitives*3;break;case D3DPT_TRIANGLESTRIP:case D3DPT_TRIANGLEFAN:vertices=primitives+2;break;}}
    ++draws_;std::ostringstream s;s<<std::setprecision(7);
    DWORD fvf=0;HRESULT fvfHr=device_->GetFVF(&fvf);
    Com<IDirect3DVertexShader9> vs;Com<IDirect3DPixelShader9> ps;device_->GetVertexShader(vs.out());device_->GetPixelShader(ps.out());
    int vsId=shader(vs,false),psId=shader(ps,true);
    int texIds[4]{};for(UINT i=0;i<4;++i){Com<IDirect3DBaseTexture9> tex;if(SUCCEEDED(device_->GetTexture(i,tex.out())))texIds[i]=texture(tex);}
    s<<"{\"kind\":\"draw\",\"frame\":"<<frame_<<",\"index\":"<<draws_<<",\"api\":\""<<api<<"\",\"primitiveType\":"<<type<<",\"primitives\":"<<primitives
     <<",\"vertices\":"<<vertices<<",\"upStride\":"<<stride<<",\"fvf\":"<<fvf<<",\"fvfHR\":"<<fvfHr<<",\"vs\":"<<vsId<<",\"ps\":"<<psId<<",\"textures\":[";
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
