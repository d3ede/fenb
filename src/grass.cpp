#include "grass.hpp"
#include "grass_bounds.hpp"
#include "generated/grass_catalog.hpp"
#include "generated/foliage_catalog.hpp"
#include "generated/material_catalog.hpp"
#include <generated/grass_capture_vs.hpp>
#include <generated/grass_capture_ps.hpp>
#include <generated/grass_blade_vs.hpp>
#include <generated/grass_blade_ps.hpp>
#include <generated/grass_hide_ps.hpp>
#include <cstring>

namespace {
constexpr D3DSAMPLERSTATETYPE Samplers[]={D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_ADDRESSU,
    D3DSAMP_ADDRESSV,D3DSAMP_MIPFILTER,D3DSAMP_MAXANISOTROPY,D3DSAMP_SRGBTEXTURE,
    D3DSAMP_MAXMIPLEVEL,D3DSAMP_MIPMAPLODBIAS};
struct BladeVertex {float side,t,angle,seed,x,z,height;};
struct Cell {float x,z;};
D3DMATRIX Identity(){D3DMATRIX m{};m._11=m._22=m._33=m._44=1;return m;}
D3DMATRIX Multiply(const D3DMATRIX &a,const D3DMATRIX &b){D3DMATRIX m{};
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)for(int k=0;k<4;++k)m.m[r][c]+=a.m[r][k]*b.m[k][c];return m;}
float Random(unsigned &s){s^=s<<13;s^=s>>17;s^=s<<5;return (s&65535)/65535.f;}
void Color(DWORD v,float *out){for(int i=0;i<3;++i)out[i]=float((v>>(16-i*8))&255)/255;out[3]=1;}
struct StreamFrequencies {
    IDirect3DDevice9 *d;UINT values[2]{1,1};
    explicit StreamFrequencies(IDirect3DDevice9 *device):d(device){for(int i=0;i<2;++i)d->GetStreamSourceFreq(i,values+i);}
    ~StreamFrequencies(){for(int i=0;i<2;++i)d->SetStreamSourceFreq(i,values[i]);}
};
struct VertexProcessing {
    IDirect3DDevice9 *d;BOOL software;
    explicit VertexProcessing(IDirect3DDevice9 *device):d(device),software(device->GetSoftwareVertexProcessing()){
        if(software)d->SetSoftwareVertexProcessing(FALSE);
    }
    ~VertexProcessing(){if(software)d->SetSoftwareVertexProcessing(TRUE);}
};
}
GrassFX::~GrassFX(){invalidate();if(hash_)BCryptCloseAlgorithmProvider(hash_,0);}
bool GrassFX::supported(){
    if(checked_)return capable_;
    checked_=true;D3DCAPS9 caps{};D3DDEVICE_CREATION_PARAMETERS cp{};Com<IDirect3D9> root;D3DDISPLAYMODE mode{};
    if(FAILED(device_->GetDeviceCaps(&caps))||caps.VertexShaderVersion<D3DVS_VERSION(3,0)||
        caps.PixelShaderVersion<D3DPS_VERSION(3,0)||FAILED(device_->GetCreationParameters(&cp))||
        (cp.BehaviorFlags&D3DCREATE_SOFTWARE_VERTEXPROCESSING)||FAILED(device_->GetDirect3D(root.out()))||
        FAILED(root->GetAdapterDisplayMode(cp.AdapterOrdinal,&mode)))return false;
    const auto format=D3DFMT_A32B32G32R32F;
    capable_=SUCCEEDED(root->CheckDeviceFormat(cp.AdapterOrdinal,cp.DeviceType,mode.Format,
        D3DUSAGE_RENDERTARGET|D3DUSAGE_QUERY_VERTEXTEXTURE|D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,D3DRTYPE_TEXTURE,format));
    capable_=capable_&&(caps.PrimitiveMiscCaps&D3DPMISCCAPS_BLENDOP)&&SUCCEEDED(root->CheckDeviceFormat(cp.AdapterOrdinal,
        cp.DeviceType,mode.Format,D3DUSAGE_RENDERTARGET|D3DUSAGE_QUERY_VERTEXTEXTURE|
        D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,D3DRTYPE_TEXTURE,D3DFMT_R32F));
    return capable_;
}
void GrassFX::invalidate(){
    endNative();for(auto *c:pending_)c->clear();pending_.clear();groundSurface_.out();ground_.out();worldTarget_.out();worldDepth_.out();
    contactSurface_.out();contacts_.out();
    captureVS_.out();capturePS_.out();bladeVS_.out();bladePS_.out();hidePS_.out();declaration_.out();
    for(auto &b:bands_){b.mesh.out();b.instances.out();b.indices.out();b.count=b.vertices=b.triangles=0;}
    resolution_=0;bufferBytes_=0;builtQuality_=-1;started_=false;clear_=true;failed_=false;checked_=false;
}
void GrassFX::beginFrame(bool enabled,const float *player,uint64_t worldKey){
    endNative();for(auto *c:pending_)c->clear();pending_.clear();worldTarget_.out();worldDepth_.out();started_=false;clear_=true;
    if(!enabled&&enabled_)invalidate();
    enabled_=enabled;
    contactClear_=true;contactTarget_=false;
    if(!worldKey||worldKey!=worldKey_)winter_=false;
    worldKey_=worldKey;
    centered_=player&&std::isfinite(player[0])&&std::isfinite(player[1])&&std::isfinite(player[2]);
    if(centered_)memcpy(center_,player,3*sizeof(float));
}
unsigned GrassFX::classify(IDirect3DTexture9 *texture){
    DWORD kind=0,size=4;
    if(SUCCEEDED(texture->GetPrivateData(GrassTag,&kind,&size))&&size==4)return kind<=5?kind:0;
    D3DSURFACE_DESC desc{};
    if(FAILED(texture->GetLevelDesc(0,&desc))||desc.Pool!=D3DPOOL_MANAGED||desc.Usage)return 0;
    UINT bytes=0,rows=desc.Height;
    if(desc.Format==D3DFMT_A8R8G8B8||desc.Format==D3DFMT_X8R8G8B8)bytes=desc.Width*4;
    else if(desc.Format==D3DFMT_A1R5G5B5||desc.Format==D3DFMT_A4R4G4B4||desc.Format==D3DFMT_R5G6B5)bytes=desc.Width*2;
    else if(desc.Format==D3DFMT_DXT1||desc.Format==D3DFMT_DXT3||desc.Format==D3DFMT_DXT5){rows=(desc.Height+3)/4;bytes=(desc.Width+3)/4*(desc.Format==D3DFMT_DXT1?8:16);}
    else return 0;
    if(!hash_)BCryptOpenAlgorithmProvider(&hash_,BCRYPT_SHA256_ALGORITHM,nullptr,0);
    D3DLOCKED_RECT lock{};if(!hash_||FAILED(texture->LockRect(0,&lock,nullptr,D3DLOCK_READONLY)))return 0;
    BCRYPT_HASH_HANDLE h=nullptr;unsigned char digest[32]{};
    bool ok=BCryptCreateHash(hash_,&h,nullptr,0,nullptr,0,0)>=0;
    for(UINT row=0;ok&&row<rows;++row)ok=BCryptHashData(h,(PUCHAR)lock.pBits+row*lock.Pitch,bytes,0)>=0;
    ok=ok&&BCryptFinishHash(h,digest,32,0)>=0;
    if(h)BCryptDestroyHash(h);texture->UnlockRect(0);if(!ok)return 0;
    size_t lo=0,hi=std::size(GrassIdentities);
    while(lo<hi){size_t mid=(lo+hi)/2;if(memcmp(GrassIdentities[mid].hash,digest,32)<0)lo=mid+1;else hi=mid;}
    kind=lo<std::size(GrassIdentities)&&!memcmp(GrassIdentities[lo].hash,digest,32)?GrassIdentities[lo].kind:0;
    if(!kind)for(const auto &leaf:FoliageHashes)if(!memcmp(leaf,digest,32)){kind=5;break;}
    if(!kind)for(const auto &leaf:MaterialIdentities)if(leaf.kind==7&&!memcmp(leaf.hash,digest,32)){kind=5;break;}
    texture->SetPrivateData(GrassTag,&kind,4,0);return kind;
}
bool GrassFX::programs(){
    if(!captureVS_.p&&FAILED(device_->CreateVertexShader(ShaderGrassCaptureVS,captureVS_.out())))return false;
    if(!capturePS_.p&&FAILED(device_->CreatePixelShader(ShaderGrassCapturePS,capturePS_.out())))return false;
    if(!bladeVS_.p&&FAILED(device_->CreateVertexShader(ShaderGrassBladeVS,bladeVS_.out())))return false;
    if(!bladePS_.p&&FAILED(device_->CreatePixelShader(ShaderGrassBladePS,bladePS_.out())))return false;
    if(!hidePS_.p&&FAILED(device_->CreatePixelShader(ShaderGrassHidePS,hidePS_.out())))return false;
    if(!declaration_.p){
        const D3DVERTEXELEMENT9 elements[]={
            {0,0,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
            {0,16,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
            {0,24,D3DDECLTYPE_FLOAT1,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},
            {1,0,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,2},D3DDECL_END()};
        if(FAILED(device_->CreateVertexDeclaration(elements,declaration_.out())))return false;
    }
    return true;
}
bool GrassFX::geometry(){
    const int quality=int(std::round(frame_.grassQuality));
    if(builtQuality_==quality&&builtDensity_==frame_.grassDensity&&builtDistance_==frame_.grassDistance)return true;
    bufferBytes_=0;
    const float distance=frame_.grassDistance;
    const float cuts[]={0,std::min(15.f,distance*.34f),std::min(42.f,distance*.7f),distance};
    const unsigned budgets[]={16000,42000,80000};
    const float weightedArea=3.14159265f*(cuts[1]*cuts[1]+(cuts[2]*cuts[2]-cuts[1]*cuts[1])*.25f+
        (cuts[3]*cuts[3]-cuts[2]*cuts[2])*.0625f);
    const float baseSpacing=std::max(.28f/std::sqrt(frame_.grassDensity),std::sqrt(weightedArea/(budgets[quality]*.94f)));
    for(int lod=0;lod<3;++lod){
        auto &b=bands_[lod];b.mesh.out();b.instances.out();b.indices.out();b.count=0;
        const int blades=quality==0?4:quality==1?7:10;
        const int segments=lod==0?quality+2:lod==1?2:1;
        std::vector<BladeVertex> mesh;std::vector<WORD> indices;unsigned seed=0x39c1a265u;
        for(int blade=0;blade<blades;++blade){
            const float angle=blade*6.2831853f/blades+Random(seed)*.3f;
            const float radius=.08f+Random(seed)*.16f,h=.75f+Random(seed)*.5f;
            const unsigned first=unsigned(mesh.size());
            for(int segment=0;segment<=segments;++segment)for(int side:{-1,1})
                mesh.push_back({float(side),float(segment)/segments,angle,float(blade),radius,0,h});
            for(int segment=0;segment<segments;++segment){WORD a=WORD(first+segment*2),c=a+2;
                indices.insert(indices.end(),{a,WORD(a+1),c,c,WORD(a+1),WORD(c+1)});}
        }
        b.inner=cuts[lod];b.outer=cuts[lod+1];b.factor=float(1<<lod);
        b.spacing=baseSpacing*b.factor;
        const int cells=int(std::ceil(b.outer/b.spacing))+1;
        std::vector<Cell> roots;
        for(int z=-cells;z<=cells;++z)for(int x=-cells;x<=cells;++x){
            const float r=std::sqrt(float(x*x+z*z))*b.spacing;
            if(r>b.outer+b.spacing*2||r<std::max(0.f,b.inner-b.spacing*2))continue;
            roots.push_back({float(x),float(z)});
        }
        if(roots.empty())continue;
        if(FAILED(device_->CreateVertexBuffer(UINT(mesh.size()*sizeof(BladeVertex)),D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,b.mesh.out(),nullptr))||
           FAILED(device_->CreateIndexBuffer(UINT(indices.size()*2),D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_DEFAULT,b.indices.out(),nullptr))||
           FAILED(device_->CreateVertexBuffer(UINT(roots.size()*sizeof(Cell)),D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,b.instances.out(),nullptr)))return false;
        void *data=nullptr;
        if(FAILED(b.mesh->Lock(0,0,&data,0)))return false;memcpy(data,mesh.data(),mesh.size()*sizeof(BladeVertex));b.mesh->Unlock();
        if(FAILED(b.indices->Lock(0,0,&data,0)))return false;memcpy(data,indices.data(),indices.size()*2);b.indices->Unlock();
        if(FAILED(b.instances->Lock(0,0,&data,0)))return false;memcpy(data,roots.data(),roots.size()*sizeof(Cell));b.instances->Unlock();
        b.vertices=UINT(mesh.size());b.triangles=UINT(indices.size()/3);b.count=UINT(roots.size());
        bufferBytes_+=mesh.size()*sizeof(BladeVertex)+indices.size()*2+roots.size()*sizeof(Cell);
    }
    builtQuality_=quality;builtDensity_=frame_.grassDensity;builtDistance_=frame_.grassDistance;return true;
}
bool GrassFX::initialize(const Settings &s){
    if(failed_||!supported())return false;
    VertexProcessing hardware(device_);
    frame_=s;
    if(FAILED(device_->GetTransform(D3DTS_VIEW,&view_))||FAILED(device_->GetTransform(D3DTS_PROJECTION,&projection_))||
        std::abs(projection_._34-1)>.001f||std::abs(projection_._44)>.001f||
        FAILED(device_->GetViewport(&viewport_))||FAILED(device_->GetRenderTarget(0,worldTarget_.out()))||
        FAILED(device_->GetDepthStencilSurface(worldDepth_.out()))||!worldDepth_.p)return false;
    for(int axis=0;axis<3;++axis)eye_[axis]=-view_._41*view_.m[axis][0]-view_._42*view_.m[axis][1]-view_._43*view_.m[axis][2];
    eye_[3]=1;
    if(!centered_)memcpy(center_,eye_,sizeof(center_));
    device_->GetRenderState(D3DRS_SRGBWRITEENABLE,&worldSrgb_);
    const unsigned resolution=int(std::round(s.grassQuality))==0?256:int(std::round(s.grassQuality))==1?512:1024;
    if(!ground_.p||resolution!=resolution_){
        groundSurface_.out();ground_.out();contactSurface_.out();contacts_.out();resolution_=resolution;
        if(FAILED(device_->CreateTexture(resolution,resolution,1,D3DUSAGE_RENDERTARGET,D3DFMT_A32B32G32R32F,D3DPOOL_DEFAULT,ground_.out(),nullptr))||
           FAILED(ground_->GetSurfaceLevel(0,groundSurface_.out()))){failed_=true;return false;}
        if(FAILED(device_->CreateTexture(resolution,resolution,1,D3DUSAGE_RENDERTARGET,D3DFMT_R32F,D3DPOOL_DEFAULT,contacts_.out(),nullptr))||
           FAILED(contacts_->GetSurfaceLevel(0,contactSurface_.out()))){failed_=true;return false;}
    }
    if(!programs()||!geometry()){failed_=true;return false;}
    map_[2]=s.grassDistance+2;map_[3]=1.f/resolution_;
    const float texel=2*map_[2]/resolution_;
    map_[0]=std::floor(center_[0]/texel)*texel;map_[1]=std::floor(center_[2]/texel)*texel;
    DWORD value=0;device_->GetRenderState(D3DRS_AMBIENT,&value);Color(value,ambient_);
    for(int axis=0;axis<3;++axis)ambient_[axis]=std::max(.12f,ambient_[axis]);
    memset(sunColor_,0,sizeof(sunColor_));sunDirection_[0]=0;sunDirection_[1]=1;sunDirection_[2]=0;
    for(DWORD light=0;light<8;++light){BOOL enabled=FALSE;D3DLIGHT9 data{};
        if(SUCCEEDED(device_->GetLightEnable(light,&enabled))&&enabled&&SUCCEEDED(device_->GetLight(light,&data))&&data.Type==D3DLIGHT_DIRECTIONAL){
            sunDirection_[0]=-data.Direction.x;sunDirection_[1]=-data.Direction.y;sunDirection_[2]=-data.Direction.z;
            memcpy(sunColor_,&data.Diffuse,sizeof(sunColor_));break;}
    }
    DWORD fog=0,mode=0,table=0,range=0;
    device_->GetRenderState(D3DRS_FOGENABLE,&fog);device_->GetRenderState(D3DRS_FOGVERTEXMODE,&mode);
    device_->GetRenderState(D3DRS_FOGTABLEMODE,&table);device_->GetRenderState(D3DRS_RANGEFOGENABLE,&range);
    fog_[0]=fog?float(table?table:mode)*(range&&!table?-1.f:1.f):0;
    const D3DRENDERSTATETYPE keys[]={D3DRS_FOGDENSITY,D3DRS_FOGSTART,D3DRS_FOGEND};
    for(int i=0;i<3;++i){device_->GetRenderState(keys[i],&value);memcpy(fog_+i+1,&value,4);}
    device_->GetRenderState(D3DRS_FOGCOLOR,&value);Color(value,fogColor_);
    started_=true;return true;
}
void GrassFX::capture(const Settings &s,DrawCall draw,SurfaceDraw geometry){
    if(!enabled_||failed_||!s.enabled||!s.grass)return;
    DWORD fvf=0,z=0,write=0,test=0,blend=0,src=0,dst=0;
    if(FAILED(device_->GetFVF(&fvf))||(fvf!=0x212&&fvf!=0x112))return;
    D3DMATRIX world{};device_->GetTransform(D3DTS_WORLD,&world);
    Com<IDirect3DVertexShader9> vs;Com<IDirect3DPixelShader9> ps;
    device_->GetVertexShader(vs.out());device_->GetPixelShader(ps.out());if(vs.p||ps.p)return;
    device_->GetRenderState(D3DRS_ZENABLE,&z);device_->GetRenderState(D3DRS_ZWRITEENABLE,&write);
    device_->GetRenderState(D3DRS_ALPHATESTENABLE,&test);device_->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend);
    device_->GetRenderState(D3DRS_SRCBLEND,&src);device_->GetRenderState(D3DRS_DESTBLEND,&dst);
    if(!z||(blend&&(src!=D3DBLEND_SRCALPHA||dst!=D3DBLEND_INVSRCALPHA)))return;
    DWORD clip=0;device_->GetRenderState(D3DRS_CLIPPLANEENABLE,&clip);if(clip)return;
    Com<IDirect3DBaseTexture9> art;device_->GetTexture(0,art.out());
    if(!art.p||art->GetType()!=D3DRTYPE_TEXTURE)return;
    const unsigned kind=classify(static_cast<IDirect3DTexture9 *>(art.p));
    if(kind==2||kind==5)return; // Vegetation cards never erase the ground below them.
    if(kind==4)winter_=true;
    const bool ground=kind==1||kind==3||(kind==4&&s.grassSnow);
    const bool prop=fvf==0x112&&!ground;
    // Exact ground atlases are opaque. A preceding cutout leaf can leave the
    // native alpha-test enabled; it must not disable the next terrain receiver.
    if(!write&&!blend&&!prop)return;
    if(!started_&&(!ground||!initialize(s)))return;
    Com<IDirect3DVertexBuffer9> source;UINT offset=0,stride=0;
    if(geometry.primitives){
        device_->GetStreamSource(0,source.out(),&offset,&stride);
        const int64_t first=geometry.indexed?int64_t(geometry.baseVertex)+geometry.minVertex:geometry.first;
        if(fvf==0x212&&source.p&&grass_bounds::outside(source,offset,stride,first,geometry.vertices,world,map_))return;
    }
    if(pending_.size()>=commands_.size())flush();
    auto release=[](Command *c){c->clear();};
    std::unique_ptr<Command,decltype(release)> command(&commands_[pending_.size()],release);
    command->draw=geometry;command->fvf=fvf;command->blend=blend!=0&&!prop;command->prop=prop;
    command->matrices[0]=world;
    D3DMATRIX view{},projection{};device_->GetTransform(D3DTS_VIEW,&view);device_->GetTransform(D3DTS_PROJECTION,&projection);
    if(memcmp(&view,&view_,sizeof(view))||memcmp(&projection,&projection_,sizeof(projection)))return;
    for(DWORD stage=0;stage<2;++stage){
        if(stage==1&&prop){command->matrices[2]=Identity();continue;}
        DWORD coordinate=0,flags=0;device_->GetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,&coordinate);
        device_->GetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,&flags);
        if((stage==0&&coordinate!=0)||(stage==1&&blend&&coordinate!=1)||
            (flags!=D3DTTFF_DISABLE&&flags!=D3DTTFF_COUNT2))return;
        if(flags)device_->GetTransform(D3DTRANSFORMSTATETYPE(D3DTS_TEXTURE0+stage),&command->matrices[stage+1]);
        else command->matrices[stage+1]=Identity();
        for(unsigned i=0;i<std::size(Samplers);++i)device_->GetSamplerState(stage,Samplers[i],&command->samplers[stage][i]);
    }
    bool blendMap=false;
    if(command->blend){DWORD op=0,arg=0;device_->GetTextureStageState(1,D3DTSS_ALPHAOP,&op);
        if(op!=D3DTOP_DISABLE){
            if(op!=D3DTOP_SELECTARG1&&op!=D3DTOP_SELECTARG2)return;
            device_->GetTextureStageState(1,op==D3DTOP_SELECTARG1?D3DTSS_ALPHAARG1:D3DTSS_ALPHAARG2,&arg);
            if(arg!=D3DTA_TEXTURE&&arg!=D3DTA_CURRENT)return;
            blendMap=arg==D3DTA_TEXTURE;
        }
    }
    command->material[0]=kind==3?2.f:kind==1?1.f:kind==4&&s.grassSnow?-1.f:0.f;command->material[1]=blendMap?1.f:0.f;
    command->material[2]=-1;
    if(command->blend){command->material[2]=1;
        if(!blendMap){DWORD lighting=0;device_->GetRenderState(D3DRS_LIGHTING,&lighting);
            if(lighting){D3DMATERIAL9 material{};device_->GetMaterial(&material);command->material[2]=material.Diffuse.a;}}}
    command->art.p=art.p;art->AddRef();if(blendMap)device_->GetTexture(1,command->blendMap.out());
    if(blendMap&&!command->blendMap.p)return;
    command->vertices.p=source.p;source.p=nullptr;command->offset=offset;command->stride=stride;
    if(geometry.primitives&&command->vertices.p&&(!geometry.indexed||SUCCEEDED(device_->GetIndices(command->indices.out())))){
        pending_.push_back(command.release());return;
    }
    flush();VertexProcessing hardware(device_);StateGuard guard(device_);StreamFrequencies frequencies(device_);if(!guard.valid())return;guard.isolateTargets();if(!target())return;
    apply(*command);if(FAILED(draw())){failed_=true;return;}
    if(command->blend&&command->material[0]!=0){apply(*command,true);if(FAILED(draw()))failed_=true;}
}
bool GrassFX::target(bool contacts){
    device_->SetStreamSourceFreq(0,1);device_->SetStreamSourceFreq(1,1);
    device_->SetTexture(D3DVERTEXTEXTURESAMPLER0,nullptr);
    device_->SetTexture(D3DVERTEXTEXTURESAMPLER1,nullptr);
    if(FAILED(device_->SetRenderTarget(0,contacts?contactSurface_:groundSurface_)))return false;
    contactTarget_=contacts;
    D3DVIEWPORT9 viewport{0,0,resolution_,resolution_,0,1};device_->SetViewport(&viewport);
    auto &clear=contacts?contactClear_:clear_;
    if(clear){if(FAILED(device_->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0)))return false;clear=false;}
    device_->SetVertexShader(captureVS_);device_->SetPixelShader(capturePS_);
    device_->SetVertexShaderConstantF(12,map_,1);
    for(auto key:{D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ALPHATESTENABLE,D3DRS_FOGENABLE,D3DRS_STENCILENABLE,
        D3DRS_SCISSORTESTENABLE,D3DRS_SRGBWRITEENABLE,D3DRS_CLIPPLANEENABLE,D3DRS_SEPARATEALPHABLENDENABLE})device_->SetRenderState(key,FALSE);
    device_->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device_->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
    device_->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);device_->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    device_->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);device_->SetRenderState(D3DRS_MULTISAMPLEMASK,0xffffffff);
    for(int i=0;i<8;++i)device_->SetRenderState(D3DRENDERSTATETYPE(D3DRS_WRAP0+i),0);
    return true;
}
void GrassFX::apply(const Command &c,bool heightOnly){
    if(c.prop!=contactTarget_&&!target(c.prop)){failed_=true;return;}
    device_->SetFVF(c.fvf);device_->SetTexture(0,c.art);device_->SetTexture(1,c.blendMap);
    device_->SetVertexShaderConstantF(0,&c.matrices[0].m[0][0],12);
    float material[]={c.material[0],c.material[1],c.material[2],!c.blend||heightOnly?1.f:0.f};
    device_->SetPixelShaderConstantF(0,material,1);
    const float contact[]={c.prop?1.f:0.f,center_[1],0,0};device_->SetPixelShaderConstantF(1,contact,1);
    device_->SetRenderState(D3DRS_ALPHABLENDENABLE,c.prop||(c.blend&&!heightOnly));
    device_->SetRenderState(D3DRS_BLENDOP,c.prop?D3DBLENDOP_MAX:D3DBLENDOP_ADD);
    device_->SetRenderState(D3DRS_SRCBLEND,c.prop?D3DBLEND_ONE:D3DBLEND_SRCALPHA);
    device_->SetRenderState(D3DRS_DESTBLEND,c.prop?D3DBLEND_ONE:D3DBLEND_INVSRCALPHA);
    // Props and roads erase coverage, never replace the receiver height. Otherwise
    // bilinear samples at a roof edge lift surviving ground blades into the air.
    device_->SetRenderState(D3DRS_COLORWRITEENABLE,c.prop?D3DCOLORWRITEENABLE_RED:c.material[0]==0?
        D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE:!c.blend?15:heightOnly?
        D3DCOLORWRITEENABLE_RED|D3DCOLORWRITEENABLE_ALPHA:D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE);
    for(DWORD stage=0;stage<(c.prop?1u:2u);++stage)for(unsigned i=0;i<std::size(Samplers);++i)device_->SetSamplerState(stage,Samplers[i],c.samplers[stage][i]);
    // Classification is calibrated in artist RGB, not camera-selected mip/sRGB
    // state. Its footprint comes from the fixed world map, independent of view.
    device_->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);
    device_->SetSamplerState(0,D3DSAMP_MAXMIPLEVEL,0);device_->SetSamplerState(0,D3DSAMP_MIPMAPLODBIAS,0);
    device_->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);device_->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
    device_->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);device_->SetSamplerState(0,D3DSAMP_MAXANISOTROPY,1);
}
void GrassFX::replay(const Command &c){
    device_->SetStreamSource(0,c.vertices,c.offset,c.stride);device_->SetIndices(c.indices);
    auto draw=[&](){return c.draw.indexed?device_->DrawIndexedPrimitive(c.draw.type,c.draw.baseVertex,c.draw.minVertex,c.draw.vertices,c.draw.first,c.draw.primitives):
        device_->DrawPrimitive(c.draw.type,c.draw.first,c.draw.primitives);};
    apply(c);if(FAILED(draw()))failed_=true;
    if(c.blend&&c.material[0]!=0){apply(c,true);if(FAILED(draw()))failed_=true;}
}
void GrassFX::flush(){
    if(pending_.empty())return;
    VertexProcessing hardware(device_);StateGuard guard(device_);StreamFrequencies frequencies(device_);
    if(guard.valid()){guard.isolateTargets();if(target())for(const auto &command:pending_)replay(*command);else failed_=true;}
    else failed_=true;
    for(auto *c:pending_)c->clear();pending_.clear();
}
void GrassFX::draw(const Settings &s,bool ownScene){
    if(!enabled_||!s.enabled||!s.grass||!started_||failed_||(winter_&&!s.grassSnow))return;
    if(ownScene&&FAILED(device_->BeginScene()))return;
    {
        flush();VertexProcessing hardware(device_);StateGuard guard(device_);StreamFrequencies frequencies(device_);
        if(guard.valid()&&!failed_&&!clear_){
            if(contactClear_&&!target(true))failed_=true;
            guard.isolateTargets();device_->SetRenderTarget(0,worldTarget_);device_->SetDepthStencilSurface(worldDepth_);device_->SetViewport(&viewport_);
            device_->SetVertexDeclaration(declaration_);device_->SetVertexShader(bladeVS_);device_->SetPixelShader(bladePS_);
            device_->SetTexture(D3DVERTEXTEXTURESAMPLER0,ground_);
            device_->SetTexture(D3DVERTEXTEXTURESAMPLER1,contacts_);
            for(DWORD slot:{DWORD(D3DVERTEXTEXTURESAMPLER0),DWORD(D3DVERTEXTEXTURESAMPLER1)}){
                for(auto key:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER})device_->SetSamplerState(slot,key,key==D3DSAMP_MIPFILTER?D3DTEXF_NONE:D3DTEXF_POINT);
                device_->SetSamplerState(slot,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);device_->SetSamplerState(slot,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
                device_->SetSamplerState(slot,D3DSAMP_MAXMIPLEVEL,0);device_->SetSamplerState(slot,D3DSAMP_MIPMAPLODBIAS,0);device_->SetSamplerState(slot,D3DSAMP_SRGBTEXTURE,FALSE);
            }
            device_->SetRenderState(D3DRS_ZENABLE,TRUE);device_->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
            device_->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);device_->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
            for(auto key:{D3DRS_ALPHABLENDENABLE,D3DRS_ALPHATESTENABLE,D3DRS_FOGENABLE,D3DRS_STENCILENABLE,
                D3DRS_SCISSORTESTENABLE,D3DRS_SRGBWRITEENABLE,D3DRS_CLIPPLANEENABLE})device_->SetRenderState(key,FALSE);
            device_->SetRenderState(D3DRS_COLORWRITEENABLE,15);device_->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
            device_->SetRenderState(D3DRS_SRGBWRITEENABLE,worldSrgb_);
            device_->SetRenderState(D3DRS_DEPTHBIAS,0);device_->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,0);
            device_->SetRenderState(D3DRS_MULTISAMPLEMASK,0xffffffff);
            for(int i=0;i<8;++i)device_->SetRenderState(D3DRENDERSTATETYPE(D3DRS_WRAP0+i),0);
            float p[17][4]{};const auto vp=Multiply(view_,projection_);memcpy(p,&vp,sizeof(vp));
            memcpy(p[4],eye_,16);memcpy(p[5],map_,16);p[5][3]=float(resolution_);
            p[6][0]=frame_.grassHeight;p[6][1]=frame_.grassWidth;p[6][2]=frame_.grassVariation;p[6][3]=std::cos(frame_.grassSlope*.0174532925f);
            const float angle=frame_.grassWindDirection*.0174532925f;
            p[8][0]=std::cos(angle);p[8][1]=std::sin(angle);p[8][2]=frame_.grassWind;p[8][3]=frame_.grassWind>0&&frame_.grassWindSpeed>0?float(GetTickCount64()%3600000)*.001f:0;
            memcpy(p[9],sunDirection_,16);memcpy(p[10],sunColor_,16);memcpy(p[11],ambient_,16);
            p[12][0]=frame_.grassBrightness;p[12][1]=frame_.grassSaturation;p[12][2]=frame_.grassDryness;p[12][3]=frame_.grassShading;
            memcpy(p[13],fog_,16);memcpy(p[14],fogColor_,16);p[15][0]=frame_.grassWindSpeed;p[15][3]=frame_.grassSnow?(winter_?2.f:1.f):0.f;
            device_->SetPixelShaderConstantF(14,fogColor_,1);
            for(int axis=0;axis<3;++axis)p[16][axis]=view_.m[axis][2];
            for(auto &b:bands_){if(!b.count)continue;
                p[7][0]=b.spacing;p[7][1]=b.inner;p[7][2]=b.outer;p[7][3]=std::max(b.spacing*2,b.outer*frame_.grassFade);
                p[15][1]=std::floor(center_[0]/b.spacing);p[15][2]=std::floor(center_[2]/b.spacing);
                device_->SetVertexShaderConstantF(0,p[0],17);
                device_->SetVertexShaderConstantF(17,center_,1);
                const float lod[]={b.factor,b.spacing/b.factor,frame_.grassDistance,frame_.grassFade};
                device_->SetVertexShaderConstantF(18,lod,1);
                device_->SetStreamSource(0,b.mesh,0,sizeof(BladeVertex));device_->SetStreamSource(1,b.instances,0,sizeof(Cell));device_->SetIndices(b.indices);
                device_->SetStreamSourceFreq(0,D3DSTREAMSOURCE_INDEXEDDATA|b.count);device_->SetStreamSourceFreq(1,D3DSTREAMSOURCE_INSTANCEDATA|1);
                if(FAILED(device_->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,b.vertices,0,b.triangles)))failed_=true;
            }
        }
    }
    if(ownScene)device_->EndScene();
}
void GrassFX::beginNative(const Settings &s){
    if(hidden_||!started_||failed_||!s.enabled||!s.grass)return;
    Com<IDirect3DBaseTexture9> texture;device_->GetTexture(0,texture.out());
    if(!texture.p||texture->GetType()!=D3DRTYPE_TEXTURE)return;
    D3DSURFACE_DESC desc{};auto *art=static_cast<IDirect3DTexture9 *>(texture.p);art->GetLevelDesc(0,&desc);
    if(desc.Width>256||desc.Height>256||classify(art)!=2)return;
    Com<IDirect3DVertexShader9> vs;Com<IDirect3DPixelShader9> ps;device_->GetVertexShader(vs.out());device_->GetPixelShader(ps.out());
    if(vs.p||ps.p)return;
    hidden_=SUCCEEDED(device_->SetPixelShader(hidePS_));
}
void GrassFX::endNative(){if(hidden_){device_->SetPixelShader(nullptr);hidden_=false;}}
