#pragma once
#include "settings.hpp"
#include "scene_tags.hpp"
#include "generated/material_catalog.hpp"
#include <generated/material_ps.hpp>
#include <bcrypt.h>
#include <cstring>

// Keep native vertex processing, depth, streams and declarations for every material.
class MaterialFX {
    IDirect3DDevice9 *device_;
    Com<IDirect3DPixelShader9> ps_;
    Com<IDirect3DTexture9> detail_;
    Com<IDirect3DBaseTexture9> savedDetail_;
    static constexpr D3DSAMPLERSTATETYPE DetailStates[] = {D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,
        D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER,D3DSAMP_SRGBTEXTURE,
        D3DSAMP_MAXMIPLEVEL,D3DSAMP_MIPMAPLODBIAS};
    DWORD savedSampler_[std::size(DetailStates)]{};
    BCRYPT_ALG_HANDLE hash_ = nullptr;
    static constexpr UINT PixelConstants = 17;
    float savedPixel_[PixelConstants][4]{};
    struct Coordinates { DWORD index=0,transform=0,wrap=0; D3DMATRIX matrix{}; } savedCoordinates_[2];
    DWORD savedFog_ = 0;
    bool active_ = false;
    bool detailSaved_ = false;
    bool detailFailed_ = false;
    bool detailTexture() {
        if(detail_.p)return true;
        if(detailFailed_)return false;
        constexpr unsigned size=128;
        // Periodic, deterministic height field. Generated once, sampled in material UVs,
        // and filtered through a complete mip chain rather than screen-space noise.
        auto random=[](unsigned x,unsigned y,unsigned seed) {
            unsigned h=x*0x9e3779b9u+y*0x85ebca6bu+seed;
            h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;
            return float(h&65535)/65535;
        };
        auto noise=[&](float x,float y,unsigned grid,unsigned seed) {
            x*=grid/float(size);y*=grid/float(size);
            unsigned ix=unsigned(x),iy=unsigned(y);float a=x-ix,b=y-iy;
            a=a*a*(3-2*a);b=b*b*(3-2*b);
            float p=random(ix%grid,iy%grid,seed),q=random((ix+1)%grid,iy%grid,seed);
            float r=random(ix%grid,(iy+1)%grid,seed),t=random((ix+1)%grid,(iy+1)%grid,seed);
            return (p+(q-p)*a)*(1-b)+(r+(t-r)*a)*b;
        };
        std::vector<float> heights(size*size);
        for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x)
            heights[y*size+x]=noise(float(x),float(y),32,31)*.65f+noise(float(x),float(y),64,89)*.35f;
        std::vector<DWORD> pixels(size*size);
        auto byte=[](float v){return DWORD(std::clamp(v,0.f,1.f)*255+.5f);};
        for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x) {
            float dx=heights[y*size+(x+1)%size]-heights[y*size+(x+size-1)%size];
            float dy=heights[((y+1)%size)*size+x]-heights[((y+size-1)%size)*size+x];
            pixels[y*size+x]=(byte(random(x,y,127))<<24)|(byte(.5f+dx*1.4f)<<16)|
                (byte(.5f+dy*1.4f)<<8)|byte(heights[y*size+x]);
        }
        if(FAILED(device_->CreateTexture(size,size,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,detail_.out(),nullptr))){detailFailed_=true;return false;}
        unsigned width=size;
        for(UINT level=0;level<detail_->GetLevelCount();++level) {
            D3DLOCKED_RECT lock{};
            if(FAILED(detail_->LockRect(level,&lock,nullptr,0))){detail_.out();detailFailed_=true;return false;}
            for(unsigned y=0;y<width;++y)memcpy((BYTE *)lock.pBits+y*lock.Pitch,pixels.data()+y*width,width*4);
            detail_->UnlockRect(level);
            if(width==1)break;
            std::vector<DWORD> next(width*width/4);
            for(unsigned y=0;y<width/2;++y)for(unsigned x=0;x<width/2;++x) {
                DWORD v=0;
                for(unsigned shift:{0u,8u,16u,24u}) {
                    unsigned sum=0;
                    for(unsigned b=0;b<2;++b)for(unsigned a=0;a<2;++a)sum+=(pixels[(y*2+b)*width+x*2+a]>>shift)&255;
                    v|=((sum+2)/4)<<shift;
                }
                next[y*(width/2)+x]=v;
            }
            pixels=std::move(next);width/=2;
        }
        return true;
    }
    static void rgba(DWORD v,float *out) {
        for(int i=0;i<3;++i)out[i]=float((v>>(16-8*i))&255)/255;
        out[3]=float(v>>24)/255;
    }
    unsigned classify(IDirect3DTexture9 *texture) {
        DWORD kind=0,size=sizeof(kind);
        if(SUCCEEDED(texture->GetPrivateData(MaterialTag,&kind,&size))&&size==sizeof(kind))return kind<=6?kind:0;
        D3DSURFACE_DESC desc{};
        if(FAILED(texture->GetLevelDesc(0,&desc))||desc.Pool!=D3DPOOL_MANAGED||desc.Usage)return 0;
        UINT bytes=0,rows=desc.Height;
        if(desc.Format==D3DFMT_A8R8G8B8||desc.Format==D3DFMT_X8R8G8B8)bytes=desc.Width*4;
        else if(desc.Format==D3DFMT_A1R5G5B5||desc.Format==D3DFMT_A4R4G4B4||desc.Format==D3DFMT_R5G6B5)bytes=desc.Width*2;
        else if(desc.Format==D3DFMT_DXT1||desc.Format==D3DFMT_DXT3||desc.Format==D3DFMT_DXT5){
            rows=(desc.Height+3)/4;bytes=(desc.Width+3)/4*(desc.Format==D3DFMT_DXT1?8:16);
        } else return 0;
        if(!hash_)BCryptOpenAlgorithmProvider(&hash_,BCRYPT_SHA256_ALGORITHM,nullptr,0);
        D3DLOCKED_RECT lock{};
        if(!hash_||FAILED(texture->LockRect(0,&lock,nullptr,D3DLOCK_READONLY)))return 0;
        BCRYPT_HASH_HANDLE hash=nullptr;
        unsigned char digest[32]{};
        bool ok=BCryptCreateHash(hash_,&hash,nullptr,0,nullptr,0,0)>=0;
        for(UINT row=0;ok&&row<rows;++row)ok=BCryptHashData(hash,(PUCHAR)lock.pBits+row*lock.Pitch,bytes,0)>=0;
        ok=ok&&BCryptFinishHash(hash,digest,32,0)>=0;
        if(hash)BCryptDestroyHash(hash);
        texture->UnlockRect(0);
        if(!ok)return 0;
        size_t lo=0,hi=std::size(MaterialIdentities);
        while(lo<hi){size_t mid=(lo+hi)/2;if(memcmp(MaterialIdentities[mid].hash,digest,32)<0)lo=mid+1;else hi=mid;}
        kind=lo<std::size(MaterialIdentities)&&!memcmp(MaterialIdentities[lo].hash,digest,32)?MaterialIdentities[lo].kind:0;
        texture->SetPrivateData(MaterialTag,&kind,sizeof(kind),0);
        return kind;
    }
public:
    explicit MaterialFX(IDirect3DDevice9 *d):device_(d){}
    ~MaterialFX(){invalidate();if(hash_)BCryptCloseAlgorithmProvider(hash_,0);}
    void invalidate(){end();ps_.out();detail_.out();detailFailed_=false;}
    bool begin(const Settings &s) {
        if(active_||!s.enabled||!s.materials||s.materialStrength<=0)return false;
        DWORD fvf=0,z=0,write=0,test=0,blend=0,src=0,dst=0,specular=0;
        if(FAILED(device_->GetFVF(&fvf))||(fvf!=0x112&&fvf!=0x212))return false;
        Com<IDirect3DBaseTexture9> base;
        if(FAILED(device_->GetTexture(0,base.out()))||!base.p||base->GetType()!=D3DRTYPE_TEXTURE)return false;
        auto *texture=static_cast<IDirect3DTexture9 *>(base.p);
        unsigned kind=classify(texture);
        if(!kind)return false;
        Com<IDirect3DVertexShader9> vs;Com<IDirect3DPixelShader9> ps;
        device_->GetVertexShader(vs.out());device_->GetPixelShader(ps.out());
        if(vs.p||ps.p)return false;
        device_->GetRenderState(D3DRS_ZENABLE,&z);device_->GetRenderState(D3DRS_ZWRITEENABLE,&write);
        device_->GetRenderState(D3DRS_ALPHATESTENABLE,&test);device_->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend);
        device_->GetRenderState(D3DRS_SRCBLEND,&src);device_->GetRenderState(D3DRS_DESTBLEND,&dst);
        device_->GetRenderState(D3DRS_SPECULARENABLE,&specular);
        DWORD clipPlanes=0;device_->GetRenderState(D3DRS_CLIPPLANEENABLE,&clipPlanes);
        if(clipPlanes||!z||!write||(test&&kind!=6)||specular||
           (blend&&(fvf!=0x212||src!=D3DBLEND_SRCALPHA||dst!=D3DBLEND_INVSRCALPHA)))return false;
        D3DMATRIX view{},projection{};
        if(FAILED(device_->GetTransform(D3DTS_VIEW,&view))||FAILED(device_->GetTransform(D3DTS_PROJECTION,&projection))||
           std::abs(projection._34)<.00001f||std::abs(projection._44)>.00001f)return false;
        float p[PixelConstants][4]{};
        const D3DTEXTURESTAGESTATETYPE keys[]={D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2};
        bool stopped=false;
        for(DWORD stage=0;stage<3;++stage) {
            DWORD values[6]{},coord=0,transform=0,constant=0;
            for(int k=0;k<6;++k)device_->GetTextureStageState(stage,keys[k],values+k);
            if(stopped)values[0]=D3DTOP_DISABLE;
            stopped|=values[0]==D3DTOP_DISABLE;
            if(!stage&&stopped)return false;
            if(values[0]!=D3DTOP_DISABLE) {
                if(values[0]<D3DTOP_SELECTARG1||values[0]>D3DTOP_ADD||values[3]>D3DTOP_ADD)return false;
                for(int k:{1,2,4,5})if(((values[k]&15)>3&&(values[k]&15)!=6)||values[k]>63)return false;
                DWORD result=0;device_->GetTextureStageState(stage,D3DTSS_RESULTARG,&result);
                if(result!=D3DTA_CURRENT)return false;
            }
            device_->GetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,&coord);
            device_->GetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,&transform);
            if(stopped){coord=0;transform=0;}
            const bool generated=stage==2&&(coord&0xffff0000)==D3DTSS_TCI_CAMERASPACEPOSITION;
            if(!generated&&(coord>1||(coord==1&&fvf!=0x212)))return false;
            if(stage<2&&(transform!=D3DTTFF_DISABLE&&transform!=D3DTTFF_COUNT2))return false;
            if(stage==2&&transform) {
                DWORD count=transform&7;
                if((transform&~DWORD(D3DTTFF_PROJECTED|7))||count<2||count>4||
                   ((transform&D3DTTFF_PROJECTED)&&count<3))return false;
            }
            if(!stage&&coord)return false;
            const UINT pixel=stage==2?14:2+stage*2;
            for(int k=0;k<4;++k)p[pixel][k]=float(values[k]);
            p[pixel+1][0]=float(values[4]);p[pixel+1][1]=float(values[5]);
            if(stage&&values[0]!=D3DTOP_DISABLE) {
                Com<IDirect3DBaseTexture9> layer;device_->GetTexture(stage,layer.out());
                p[pixel+1][2]=layer.p?1.f:0.f;
                if(layer.p&&layer->GetType()!=D3DRTYPE_TEXTURE)return false;
            }
            if(stage==2) {
                p[15][3]=(transform&D3DTTFF_PROJECTED)?float(transform&7):0;
                if(!stopped){DWORD next=0;device_->GetTextureStageState(3,D3DTSS_COLOROP,&next);if(next!=D3DTOP_DISABLE)return false;}
            }
            device_->GetTextureStageState(stage,D3DTSS_CONSTANT,&constant);rgba(constant,p[stage==2?16:7+stage]);
        }
        DWORD factor=0;device_->GetRenderState(D3DRS_TEXTUREFACTOR,&factor);rgba(factor,p[6]);
        for(DWORD index=0;index<8;++index) {
            BOOL enabled=FALSE;D3DLIGHT9 light{};
            if(FAILED(device_->GetLightEnable(index,&enabled))||!enabled||FAILED(device_->GetLight(index,&light))||light.Type!=D3DLIGHT_DIRECTIONAL)continue;
            for(int axis=0;axis<3;++axis)p[11][axis]=-light.Direction.x*view.m[0][axis]-light.Direction.y*view.m[1][axis]-light.Direction.z*view.m[2][axis];
            memcpy(p[12],&light.Diffuse,sizeof(float)*3);break;
        }
        DWORD fog=0,vertexFog=0,tableFog=0,rangeFog=0,color=0;
        device_->GetRenderState(D3DRS_FOGENABLE,&fog);device_->GetRenderState(D3DRS_FOGVERTEXMODE,&vertexFog);
        device_->GetRenderState(D3DRS_FOGTABLEMODE,&tableFog);device_->GetRenderState(D3DRS_RANGEFOGENABLE,&rangeFog);
        device_->GetRenderState(D3DRS_FOGCOLOR,&color);rgba(color,p[9]);
        const D3DRENDERSTATETYPE fogKeys[]={D3DRS_FOGDENSITY,D3DRS_FOGSTART,D3DRS_FOGEND};
        for(int i=0;i<3;++i){DWORD bits=0;device_->GetRenderState(fogKeys[i],&bits);memcpy(&p[10][i+1],&bits,4);}
        p[10][0]=fog?float(tableFog?tableFog:vertexFog)*(rangeFog&&!tableFog?-1.f:1.f):0;
        D3DSURFACE_DESC desc{};texture->GetLevelDesc(0,&desc);
        p[0][0]=s.materialStrength;p[0][1]=s.materialRelief;p[0][2]=s.materialGlints;p[0][3]=s.materialSnow;
        p[1][0]=1.f/desc.Width;p[1][1]=1.f/desc.Height;p[1][2]=float(kind);
        p[1][3]=kind==1?.02f:(kind==3||kind==6)?.012f:kind==2?.008f:.005f;
        const bool detailed=(kind==1||kind==6)&&detailTexture();
        p[13][0]=kind==1?2.f:3.f;p[13][1]=kind==1?2.f:.35f;
        p[13][2]=detailed?1.f:0.f;p[13][3]=kind==6?15.f:kind==3?10.f:kind==2?7.f:1.f;
        if(!ps_.p&&FAILED(device_->CreatePixelShader(ShaderMaterialPS,ps_.out())))return false;
        if(FAILED(device_->GetPixelShaderConstantF(0,savedPixel_[0],PixelConstants))||
           FAILED(device_->GetRenderState(D3DRS_FOGENABLE,&savedFog_)))return false;
        for(int index=0;index<2;++index) {
            auto &saved=savedCoordinates_[index];const DWORD stage=3+index;
            if(FAILED(device_->GetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,&saved.index))||
               FAILED(device_->GetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,&saved.transform))||
               FAILED(device_->GetTransform(D3DTRANSFORMSTATETYPE(D3DTS_TEXTURE0+stage),&saved.matrix))||
               FAILED(device_->GetRenderState(D3DRENDERSTATETYPE(D3DRS_WRAP3+index),&saved.wrap)))return false;
        }
        if(FAILED(device_->GetTexture(3,savedDetail_.out())))return false;
        if(detailed) {
            for(size_t i=0;i<std::size(DetailStates);++i)
                if(FAILED(device_->GetSamplerState(3,DetailStates[i],savedSampler_+i))){savedDetail_.out();return false;}
        }
        active_=true;
        detailSaved_=detailed;
        // SM2a flattens the optional sample: never read a stale client binding,
        // even for materials that do not use the generated detail map.
        if(FAILED(device_->SetTexture(3,detailed?detail_.p:nullptr))){end();return false;}
        if(detailed) {
            const DWORD values[]={D3DTADDRESS_WRAP,D3DTADDRESS_WRAP,D3DTEXF_LINEAR,D3DTEXF_LINEAR,D3DTEXF_LINEAR,FALSE,0,0};
            for(size_t i=0;i<std::size(DetailStates);++i)
                if(FAILED(device_->SetSamplerState(3,DetailStates[i],values[i]))){end();return false;}
        }
        D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
        // The native vertex converter supplies view position and normal in spare coordinates.
        // It retains the original POSITION/depth calculations for coplanar terrain layers.
        for(int index=0;index<2;++index) {
            const DWORD stage=3+index;
            if(FAILED(device_->SetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,index?D3DTSS_TCI_CAMERASPACENORMAL:D3DTSS_TCI_CAMERASPACEPOSITION))||
               FAILED(device_->SetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_COUNT3))||
               FAILED(device_->SetTransform(D3DTRANSFORMSTATETYPE(D3DTS_TEXTURE0+stage),&identity))||
               FAILED(device_->SetRenderState(D3DRENDERSTATETYPE(D3DRS_WRAP3+index),0))){end();return false;}
        }
        if(FAILED(device_->SetPixelShader(ps_))||FAILED(device_->SetPixelShaderConstantF(0,p[0],PixelConstants))||
           FAILED(device_->SetRenderState(D3DRS_FOGENABLE,FALSE))){end();return false;}
        return true;
    }
    void end() {
        if(!active_)return;
        device_->SetTexture(3,savedDetail_);savedDetail_.out();
        if(detailSaved_) {
            for(size_t i=0;i<std::size(DetailStates);++i)device_->SetSamplerState(3,DetailStates[i],savedSampler_[i]);
            detailSaved_=false;
        }
        device_->SetPixelShaderConstantF(0,savedPixel_[0],PixelConstants);
        for(int index=0;index<2;++index) {
            const auto &saved=savedCoordinates_[index];const DWORD stage=3+index;
            device_->SetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,saved.index);
            device_->SetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,saved.transform);
            device_->SetTransform(D3DTRANSFORMSTATETYPE(D3DTS_TEXTURE0+stage),&saved.matrix);
            device_->SetRenderState(D3DRENDERSTATETYPE(D3DRS_WRAP3+index),saved.wrap);
        }
        device_->SetRenderState(D3DRS_FOGENABLE,savedFog_);
        device_->SetPixelShader(nullptr);active_=false;
    }
};
