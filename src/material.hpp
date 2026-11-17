#pragma once
#include "settings.hpp"
#include "scene_tags.hpp"
#include "generated/material_catalog.hpp"
#include <generated/material_vs.hpp>
#include <generated/material_rigid_vs.hpp>
#include <generated/material_ps.hpp>
#include <bcrypt.h>
#include <cstring>

// One native draw, original artwork and coverage. Unknown/programmable passes stay native.
class MaterialFX {
    IDirect3DDevice9 *device_;
    Com<IDirect3DVertexShader9> vs_,rigidVS_;
    Com<IDirect3DPixelShader9> ps_;
    BCRYPT_ALG_HANDLE hash_ = nullptr;
    static constexpr UINT VertexConstants = 60, PixelConstants = 17;
    float savedVertex_[VertexConstants][4]{}, savedPixel_[PixelConstants][4]{};
    DWORD savedFog_ = 0;
    bool active_ = false;
    static D3DMATRIX multiply(const D3DMATRIX &a, const D3DMATRIX &b) {
        D3DMATRIX out{};
        for (int i=0;i<4;++i) for(int j=0;j<4;++j) for(int k=0;k<4;++k)
            out.m[i][j]+=a.m[i][k]*b.m[k][j];
        return out;
    }
    static void rgba(DWORD v,float *out) {
        for(int i=0;i<3;++i)out[i]=float((v>>(16-8*i))&255)/255;
        out[3]=float(v>>24)/255;
    }
    static bool normalMatrix(const D3DMATRIX &m,float out[3][4]) {
        float c[3][3]={{m._22*m._33-m._23*m._32,m._23*m._31-m._21*m._33,m._21*m._32-m._22*m._31},
                       {m._13*m._32-m._12*m._33,m._11*m._33-m._13*m._31,m._12*m._31-m._11*m._32},
                       {m._12*m._23-m._13*m._22,m._13*m._21-m._11*m._23,m._11*m._22-m._12*m._21}};
        float det=m._11*c[0][0]+m._12*c[0][1]+m._13*c[0][2];
        if(!std::isfinite(det)||std::abs(det)<.00000001f)return false;
        for(int i=0;i<3;++i)for(int j=0;j<3;++j)out[i][j]=c[i][j]/det;
        return true;
    }
    static void direction(const D3DVECTOR &v,const D3DMATRIX &view,float *out,bool point) {
        for(int i=0;i<3;++i)out[i]=v.x*view.m[0][i]+v.y*view.m[1][i]+v.z*view.m[2][i]+(point?view.m[3][i]:0);
    }
    unsigned classify(IDirect3DTexture9 *texture) {
        DWORD kind=0,size=sizeof(kind);
        if(SUCCEEDED(texture->GetPrivateData(MaterialTag,&kind,&size))&&size==sizeof(kind))return kind<=5?kind:0;
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
    void invalidate(){end();vs_.out();rigidVS_.out();ps_.out();}
    bool begin(const Settings &s) {
        if(active_||!s.enabled||!s.materials||s.materialStrength<=0)return false;
        DWORD fvf=0,z=0,write=0,test=0,blend=0,src=0,dst=0,specular=0,lit=0;
        if(FAILED(device_->GetFVF(&fvf))||(fvf!=0x112&&fvf!=0x212))return false;
        // Reject cached unknown artwork before querying the rest of the native pipeline.
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
        device_->GetRenderState(D3DRS_SPECULARENABLE,&specular);device_->GetRenderState(D3DRS_LIGHTING,&lit);
        DWORD clipPlanes=0;device_->GetRenderState(D3DRS_CLIPPLANEENABLE,&clipPlanes);
        if(clipPlanes)return false;
        if(!z||!write||test||specular||(blend&&(fvf!=0x212||src!=D3DBLEND_SRCALPHA||dst!=D3DBLEND_INVSRCALPHA)))return false;
        float v[VertexConstants][4]{},p[PixelConstants][4]{};
        D3DMATRIX world{},view{},projection{},uv[3]{};
        if(FAILED(device_->GetTransform(D3DTS_WORLD,&world))||FAILED(device_->GetTransform(D3DTS_VIEW,&view))||
           FAILED(device_->GetTransform(D3DTS_PROJECTION,&projection))||std::abs(projection._34)<.00001f||
           std::abs(projection._44)>.00001f)return false;
        auto worldView=multiply(world,view),clip=multiply(worldView,projection);
        memcpy(v[0],&clip,sizeof(clip));memcpy(v[4],&worldView,sizeof(worldView));
        float normals[3][4]{};
        if(!normalMatrix(worldView,normals))return false;
        memcpy(v[8],normals,sizeof(normals));
        const D3DTEXTURESTAGESTATETYPE stageKeys[]={D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2};
        bool stopped=false;
        for(DWORD stage=0;stage<3;++stage) {
            DWORD values[6]{},coord=0,transform=0,constant=0;
            for(int k=0;k<6;++k)device_->GetTextureStageState(stage,stageKeys[k],values+k);
            if(stopped)values[0]=D3DTOP_DISABLE;
            stopped|=values[0]==D3DTOP_DISABLE;
            if(!stage&&values[0]==D3DTOP_DISABLE)return false;
            if(values[0]!=D3DTOP_DISABLE) {
                if(values[0]<D3DTOP_SELECTARG1||values[0]>D3DTOP_ADD||values[3]>D3DTOP_ADD)return false;
                for(int k:{1,2,4,5})if(((values[k]&15)>3&&(values[k]&15)!=6)||values[k]>63)return false;
            }
            device_->GetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,&coord);
            device_->GetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,&transform);
            if(stage&&values[0]==D3DTOP_DISABLE){coord=0;transform=0;}
            const bool generated=stage==2&&(coord&0xffff0000)==D3DTSS_TCI_CAMERASPACEPOSITION;
            if(!generated&&(coord>1||(coord==1&&fvf!=0x212)))return false;
            if(stage<2&&(transform!=D3DTTFF_DISABLE&&transform!=D3DTTFF_COUNT2))return false;
            if(stage==2&&transform){
                DWORD count=transform&7;
                if((transform&~DWORD(D3DTTFF_PROJECTED|7))||count<2||count>4||
                   ((transform&D3DTTFF_PROJECTED)&&count<3))return false;
            }
            if(!stage&&coord)return false;
            if(stage==1)v[13][1]=float(coord);
            if(stage==2){
                v[59][0]=generated?2.f:float(coord);
                v[59][1]=(transform&D3DTTFF_PROJECTED)?float(transform&7):0;
                if(values[0]!=D3DTOP_DISABLE){DWORD next=0;device_->GetTextureStageState(3,D3DTSS_COLOROP,&next);if(next!=D3DTOP_DISABLE)return false;}
            }
            if(transform)device_->GetTransform(D3DTRANSFORMSTATETYPE(D3DTS_TEXTURE0+stage),uv+stage);
            else uv[stage]._11=uv[stage]._22=uv[stage]._33=uv[stage]._44=1;
            memcpy(v[stage==2?55:31+stage*4],uv+stage,sizeof(D3DMATRIX));
            const UINT pixel=stage==2?14:2+stage*2;
            for(int k=0;k<4;++k)p[pixel][k]=float(values[k]);
            p[pixel+1][0]=float(values[4]);p[pixel+1][1]=float(values[5]);
            if(stage&&values[0]!=D3DTOP_DISABLE) {
                Com<IDirect3DBaseTexture9> layer;device_->GetTexture(stage,layer.out());
                p[pixel+1][2]=layer.p?1.f:0.f;
                if(layer.p&&layer->GetType()!=D3DRTYPE_TEXTURE)return false;
            }
            device_->GetTextureStageState(stage,D3DTSS_CONSTANT,&constant);rgba(constant,p[stage==2?16:7+stage]);
        }
        D3DMATERIAL9 material{};device_->GetMaterial(&material);
        DWORD ambient=0,factor=0,normalize=0;
        device_->GetRenderState(D3DRS_AMBIENT,&ambient);device_->GetRenderState(D3DRS_TEXTUREFACTOR,&factor);
        device_->GetRenderState(D3DRS_NORMALIZENORMALS,&normalize);rgba(factor,p[6]);
        float global[4]{};rgba(ambient,global);
        for(int k=0;k<3;++k)v[11][k]=global[k]*(&material.Ambient.r)[k]+(&material.Emissive.r)[k];
        memcpy(v[12],&material.Diffuse,sizeof(float)*4);memcpy(p[13],&material.Diffuse,sizeof(float)*4);
        v[13][0]=lit?1.f:0.f;v[13][3]=normalize?1.f:0.f;
        bool sun=false;
        for(DWORD index=0;index<8;++index) {
            BOOL enabled=FALSE;D3DLIGHT9 light{};
            if(FAILED(device_->GetLightEnable(index,&enabled))||!enabled||FAILED(device_->GetLight(index,&light)))continue;
            if(light.Type!=D3DLIGHT_DIRECTIONAL&&light.Type!=D3DLIGHT_POINT)return false;
            bool point=light.Type==D3DLIGHT_POINT;
            if(point)direction(light.Position,view,v[14+index],true);
            else {D3DVECTOR incoming{-light.Direction.x,-light.Direction.y,-light.Direction.z};direction(incoming,view,v[14+index],false);}
            v[14+index][3]=point?1.f:0.f;
            memcpy(v[22+index],&light.Diffuse,sizeof(float)*3);
            v[39+index][0]=light.Attenuation0;v[39+index][1]=light.Attenuation1;
            v[39+index][2]=light.Attenuation2;v[39+index][3]=light.Range;
            for(int k=0;k<3;++k)v[47+index][k]=(&light.Ambient.r)[k]*(&material.Ambient.r)[k];
            if(!point&&!sun){memcpy(p[11],v[14+index],sizeof(float)*3);memcpy(p[12],&light.Diffuse,sizeof(float)*3);sun=true;}
        }
        DWORD fog=0,vertexFog=0,tableFog=0,rangeFog=0,fogColor=0;
        device_->GetRenderState(D3DRS_FOGENABLE,&fog);device_->GetRenderState(D3DRS_FOGVERTEXMODE,&vertexFog);
        device_->GetRenderState(D3DRS_FOGTABLEMODE,&tableFog);device_->GetRenderState(D3DRS_RANGEFOGENABLE,&rangeFog);
        device_->GetRenderState(D3DRS_FOGCOLOR,&fogColor);rgba(fogColor,p[9]);
        const D3DRENDERSTATETYPE fogKeys[]={D3DRS_FOGDENSITY,D3DRS_FOGSTART,D3DRS_FOGEND};
        for(int i=0;i<3;++i){DWORD bits=0;device_->GetRenderState(fogKeys[i],&bits);memcpy(&p[10][i+1],&bits,4);v[30][i+1]=p[10][i+1];}
        p[10][0]=fog?float(tableFog):0;v[30][0]=fog&&!tableFog?float(vertexFog)*(rangeFog?-1.f:1.f):0;
        D3DSURFACE_DESC desc{};texture->GetLevelDesc(0,&desc);
        p[0][0]=s.materialStrength;p[0][1]=s.materialRelief;p[0][2]=s.materialGlints;p[0][3]=s.materialSnow;
        p[1][0]=1.f/desc.Width;p[1][1]=1.f/desc.Height;p[1][2]=float(kind);p[1][3]=lit?1.f:0.f;
        auto &vertex=fvf==0x212?vs_:rigidVS_;
        if((!vertex.p&&FAILED(device_->CreateVertexShader(fvf==0x212?ShaderMaterialVS:ShaderMaterialRigidVS,vertex.out())))||
           (!ps_.p&&FAILED(device_->CreatePixelShader(ShaderMaterialPS,ps_.out()))))return false;
        if(FAILED(device_->GetVertexShaderConstantF(0,savedVertex_[0],VertexConstants))||
           FAILED(device_->GetPixelShaderConstantF(0,savedPixel_[0],PixelConstants))||
           FAILED(device_->GetRenderState(D3DRS_FOGENABLE,&savedFog_)))return false;
        active_=true;
        if(FAILED(device_->SetVertexShader(vertex))||FAILED(device_->SetPixelShader(ps_))||
           FAILED(device_->SetVertexShaderConstantF(0,v[0],VertexConstants))||
           FAILED(device_->SetPixelShaderConstantF(0,p[0],PixelConstants))||
           FAILED(device_->SetRenderState(D3DRS_FOGENABLE,FALSE))){end();return false;}
        return true;
    }
    void end(){
        if(!active_)return;
        device_->SetVertexShader(nullptr);device_->SetPixelShader(nullptr);
        device_->SetVertexShaderConstantF(0,savedVertex_[0],VertexConstants);
        device_->SetPixelShaderConstantF(0,savedPixel_[0],PixelConstants);
        device_->SetRenderState(D3DRS_FOGENABLE,savedFog_);active_=false;
    }
};
