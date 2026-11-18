// Standalone, deterministic DX9 scenes using FENB's shipping effect implementations.
// Local client artwork is opened read-only. No client process, DLL or settings are touched.
#include "effects.hpp"
#include "grass.hpp"
#include "material.hpp"
#include "trees.hpp"
#include "sfx.hpp"
#include "actor_visibility.hpp"
#include <gdiplus.h>
#include <filesystem>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <functional>

std::wstring ModuleDirectory() { return L"build/"; }
void Log(const char *message) { puts(message); }

namespace {
constexpr unsigned Width=1600, Height=900;
constexpr float Pi=3.14159265359f;
void Require(bool ok,const char *message) { if(!ok)throw std::runtime_error(message); }
struct Vec {
    float x=0,y=0,z=0;
    Vec operator+(Vec b)const{return {x+b.x,y+b.y,z+b.z};}
    Vec operator-(Vec b)const{return {x-b.x,y-b.y,z-b.z};}
    Vec operator*(float a)const{return {x*a,y*a,z*a};}
};
float Dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec Cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vec Unit(Vec p){return p*(1/std::max(.00001f,std::sqrt(Dot(p,p))));}
D3DMATRIX Identity(){D3DMATRIX m{};m._11=m._22=m._33=m._44=1;return m;}
D3DMATRIX Look(Vec eye,Vec target){
    Vec z=Unit(target-eye),x=Unit(Cross({0,1,0},z)),y=Cross(z,x);
    return {x.x,y.x,z.x,0,x.y,y.y,z.y,0,x.z,y.z,z.z,0,-Dot(x,eye),-Dot(y,eye),-Dot(z,eye),1};
}
struct Random {
    unsigned state=2311786;
    float next(){state^=state<<13;state^=state>>17;state^=state<<5;return (state>>8)*(1.f/16777216);}
};
struct Vertex{float x,y,z,nx,ny,nz,u,v,a=0,b=0;}; // Native terrain FVF 0x212.
Vertex Make(Vec p,Vec n,float u,float v){return {p.x,p.y,p.z,n.x,n.y,n.z,u,v,u,v};}
struct Mesh {
    std::vector<Vertex> data;
    Com<IDirect3DVertexBuffer9> buffer;
    void tri(Vertex a,Vertex b,Vertex c){data.insert(data.end(),{a,b,c});}
    void quad(Vertex a,Vertex b,Vertex c,Vertex e){tri(a,b,c);tri(a,c,e);}
    void upload(IDirect3DDevice9 *d){
        if(data.empty())return;
        Require(SUCCEEDED(d->CreateVertexBuffer(UINT(data.size()*sizeof(Vertex)),D3DUSAGE_WRITEONLY,0x212,D3DPOOL_MANAGED,buffer.out(),nullptr)),"mesh allocation");
        void *p=nullptr;Require(SUCCEEDED(buffer->Lock(0,0,&p,0)),"mesh lock");
        memcpy(p,data.data(),data.size()*sizeof(Vertex));buffer->Unlock();
    }
    HRESULT draw(IDirect3DDevice9 *d,DWORD fvf=0x212){
        d->SetFVF(fvf);d->SetStreamSource(0,buffer,0,sizeof(Vertex));
        return d->DrawPrimitive(D3DPT_TRIANGLELIST,0,UINT(data.size()/3));
    }
    SurfaceDraw geometry()const{return {D3DPT_TRIANGLELIST,0,UINT(data.size()/3)};}
};
float Ground(float x,float z){return .17f*std::sin(x*.31f)*std::cos(z*.23f)+.07f*std::sin(z*.56f);}
void Terrain(Mesh &m,bool flat=false){
    constexpr int N=80;
    auto point=[&](int x,int z){float a=-32+x*.8f,b=-18+z*.8f;
        float dx=flat?0:.17f*.31f*std::cos(a*.31f)*std::cos(b*.23f);
        float dz=flat?0:-.17f*.23f*std::sin(a*.31f)*std::sin(b*.23f)+.07f*.56f*std::cos(b*.56f);
        return Make({a,flat?0:Ground(a,b),b},Unit({-dx,1,-dz}),a*.24f,b*.24f);};
    for(int z=0;z<N;++z)for(int x=0;x<N;++x)m.quad(point(x,z),point(x,z+1),point(x+1,z+1),point(x+1,z));
}
void Tube(Mesh &m,const std::vector<Vec>&points,const std::vector<float>&radii,float repeats=1){
    constexpr unsigned Sides=18;
    std::vector<std::array<Vertex,Sides+1>> rings;
    for(size_t i=0;i<points.size();++i){
        Vec axis=Unit(points[std::min(i+1,points.size()-1)]-points[i?i-1:0]);
        Vec x=Unit(Cross(axis,{0,0,1})),z=Cross(axis,x);
        std::array<Vertex,Sides+1> ring{};
        for(unsigned j=0;j<=Sides;++j){float angle=j*2*Pi/Sides;
            Vec normal=x*std::cos(angle)+z*std::sin(angle);
            ring[j]=Make(points[i]+normal*(radii[i]*(1+.035f*std::sin(angle*5+i*.6f))),normal,float(j)/Sides*repeats,1-float(i)/(points.size()-1));}
        rings.push_back(ring);
    }
    for(size_t i=0;i+1<rings.size();++i)for(unsigned j=0;j<Sides;++j)
        m.quad(rings[i][j],rings[i+1][j],rings[i+1][j+1],rings[i][j+1]);
}
void Card(Mesh &m,Vec center,Vec right,Vec up){
    Vec n=Unit(Cross(right,up));
    m.quad(Make(center-right-up,n,0,1),Make(center+right-up,n,1,1),Make(center+right+up,n,1,0),Make(center-right+up,n,0,0));
}
void Ball(Mesh &m,Vec p,Vec scale){
    constexpr int R=8,S=16;
    auto point=[&](int i,int j){float a=i*Pi/R,b=j*2*Pi/S;
        Vec n{std::sin(a)*std::cos(b),std::cos(a),std::sin(a)*std::sin(b)};
        return Make(p+Vec{n.x*scale.x,n.y*scale.y,n.z*scale.z},Unit({n.x/scale.x,n.y/scale.y,n.z/scale.z}),float(j)/S,float(i)/R);};
    for(int i=0;i<R;++i)for(int j=0;j<S;++j)m.quad(point(i,j),point(i+1,j),point(i+1,j+1),point(i,j+1));
}
void Tree(Mesh &bark,Mesh &leaves,Vec origin,float scale,bool winter,Random &random){
    auto at=[&](float y){return origin+Vec{-.12f*std::sin(y*.9f),y,.05f*std::sin(y*.7f)}*scale;};
    std::vector<Vec> line;std::vector<float> radii;
    for(int j=0;j<=12;++j){float y=j*5.5f/12;line.push_back(at(y));radii.push_back((.42f*std::pow(1-y/6.1f,1.2f)+.02f+(j==0?.18f:0))*scale);}
    Tube(bark,line,radii);
    for(int k=0;k<7;++k){float a=k*2.399963f;Vec dir{std::cos(a),0,std::sin(a)};
        Tube(bark,{origin+dir*(.98f*scale)+Vec{0,.02f,0},origin+dir*(.45f*scale)+Vec{0,.16f*scale,0},at(.65f)},
             {.02f*scale,.14f*scale,.26f*scale});}
    for(int k=0;k<10;++k){float y=2.05f+k*.29f,a=k*2.399963f;Vec dir{std::cos(a),0,std::sin(a)};
        Vec start=at(y),tip=start+dir*((2.05f-k*.1f)*scale)+Vec{0,.68f*scale,0};
        Tube(bark,{start,start+(tip-start)*.45f+Vec{0,-.08f*scale,0},tip},{.135f*scale,.09f*scale,.012f*scale});
        if(winter)for(int q=0;q<12;++q){float t=random.next()*2*Pi;
            Vec p=tip+Vec{(random.next()-.5f)*1.4f,(random.next()-.5f)*.6f,(random.next()-.5f)*1.4f}*scale;
            Card(leaves,p,Vec{std::cos(t),0,std::sin(t)}*(.57f*scale),{0,.43f*scale,0});}
    }
    if(!winter)for(int layer=0;layer<7;++layer){float y=2.9f+layer*.36f,r=2.05f*(1-std::pow(float(layer)/7,1.8f));
        for(int j=0;j<38;++j){float a=random.next()*2*Pi,rr=std::sqrt(random.next())*r;
            Vec p=at(y)+Vec{std::cos(a)*rr,(random.next()-.5f)*.25f,std::sin(a)*rr}*scale;
            float t=random.next()*2*Pi;Card(leaves,p,Vec{std::cos(t),0,std::sin(t)}*(.70f*scale),{0,.53f*scale,0});}}
}

class Renderer {
    HMODULE d3dx_=nullptr;
    using LoadTexture=HRESULT(WINAPI *)(IDirect3DDevice9 *,LPCWSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,DWORD,DWORD,D3DCOLOR,void *,PALETTEENTRY *,IDirect3DTexture9 **);
    LoadTexture load_=nullptr;
    std::filesystem::path artwork_,output_;
    Com<IDirect3D9> root_;
    Com<IDirect3DDevice9> device_;
    std::unique_ptr<GrassFX> grass_;
    std::unique_ptr<MaterialFX> materials_;
    std::unique_ptr<TreeFX> trees_;
    std::unique_ptr<SfxFX> particles_;
    std::unique_ptr<Effects> effects_;
    Com<IDirect3DCubeTexture9> skyCube_;
    Com<IDirect3DTexture9> ground_,snow_,soil_,paving_,bark_,winterBark_,foliage_,winterFoliage_,stone_,white_,aura_,card_;
    Mesh terrain_,flat_,trunks_,crowns_,winterTrunks_,winterCrowns_,rocks_,snowRocks_,posts_;
    D3DMATRIX view_{},projection_{};
    Settings current_;
    HWND window_=nullptr;
    unsigned materialDraws_=0,treeDraws_=0,particleDraws_=0;
    unsigned frames_=0;
    std::vector<std::pair<std::string,std::vector<DWORD>>> checks_;
    void load(Com<IDirect3DTexture9>&texture,const wchar_t *file){
        auto path=artwork_/file;
        Require(SUCCEEDED(load_(device_,path.c_str(),UINT(-1),UINT(-1),0,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,1,1,0,nullptr,nullptr,texture.out())),"load local artwork");
        printf("ARTWORK %ls\n",file);
    }
    void solid(Com<IDirect3DTexture9>&texture,DWORD color){
        Require(SUCCEEDED(device_->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,texture.out(),nullptr)),"solid texture");
        D3DLOCKED_RECT lock{};texture->LockRect(0,&lock,nullptr,0);
        for(int y=0;y<4;++y)std::fill_n((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch),4,color);
        texture->UnlockRect(0);
    }
    void setup(bool winter=false,bool night=false){
        auto*d=device_.p;auto identity=Identity();
        d->SetTransform(D3DTS_WORLD,&identity);d->SetTransform(D3DTS_VIEW,&view_);d->SetTransform(D3DTS_PROJECTION,&projection_);
        for(DWORD i=0;i<4;++i){d->SetTexture(i,nullptr);d->SetTextureStageState(i,D3DTSS_TEXCOORDINDEX,0);
            d->SetTextureStageState(i,D3DTSS_TEXTURETRANSFORMFLAGS,0);d->SetTransform(D3DTS_TEXTURE0,&identity);
            d->SetTextureStageState(i,D3DTSS_COLOROP,i?D3DTOP_DISABLE:D3DTOP_MODULATE);
            d->SetTextureStageState(i,D3DTSS_COLORARG1,D3DTA_TEXTURE);d->SetTextureStageState(i,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
            d->SetTextureStageState(i,D3DTSS_ALPHAOP,i?D3DTOP_DISABLE:D3DTOP_MODULATE);
            d->SetTextureStageState(i,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);d->SetTextureStageState(i,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
            d->SetSamplerState(i,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);d->SetSamplerState(i,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
            d->SetSamplerState(i,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);d->SetSamplerState(i,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
            d->SetSamplerState(i,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);d->SetSamplerState(i,D3DSAMP_SRGBTEXTURE,FALSE);}
        d->SetVertexShader(nullptr);d->SetPixelShader(nullptr);d->SetFVF(0x212);
        d->SetRenderState(D3DRS_ZENABLE,TRUE);d->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);d->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
        d->SetRenderState(D3DRS_LIGHTING,TRUE);d->SetRenderState(D3DRS_NORMALIZENORMALS,TRUE);
        d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
        d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);d->SetRenderState(D3DRS_SPECULARENABLE,FALSE);
        d->SetRenderState(D3DRS_AMBIENT,night?0xff232b38:winter?0xff777f90:0xff686a58);
        d->SetRenderState(D3DRS_FOGENABLE,FALSE);
        D3DMATERIAL9 material{};material.Diffuse=material.Ambient={1,1,1,1};d->SetMaterial(&material);
        for(DWORD i=0;i<8;++i)d->LightEnable(i,FALSE);
        D3DLIGHT9 sun{};sun.Type=D3DLIGHT_DIRECTIONAL;sun.Direction={.42f,-.75f,.36f};
        sun.Diffuse=night?D3DCOLORVALUE{.18f,.24f,.32f,1}:winter?D3DCOLORVALUE{.48f,.48f,.46f,1}:D3DCOLORVALUE{.63f,.61f,.48f,1};
        d->SetLight(0,&sun);d->LightEnable(0,TRUE);
    }
    void tint(DWORD color){
        D3DMATERIAL9 material{};material.Diffuse=material.Ambient={float((color>>16)&255)/255,float((color>>8)&255)/255,float(color&255)/255,1};
        device_->SetMaterial(&material);
    }
    void draw(Mesh&m,IDirect3DTexture9*texture,bool material=false,bool tree=false){
        auto*d=device_.p;d->SetTexture(0,texture);
        bool t=tree&&trees_->begin(current_);treeDraws_+=t;
        bool s=material&&materials_->begin(current_);materialDraws_+=s;
        Require(SUCCEEDED(m.draw(d)),"scene draw");
        if(s)materials_->end();if(t)trees_->end();
    }
    void sky(bool winter,bool night){
        auto*d=device_.p;StateGuard guard(d);
        struct Screen{float x,y,z,w;DWORD color;};
        DWORD top=night?0xff101c32:winter?0xff769bb4:0xff688ea7;
        DWORD bottom=night?0xff596373:winter?0xffd9e4e6:0xffe1d9b6;
        Screen q[]={{-.5f,-.5f,1,1,top},{Width-.5f,-.5f,1,1,top},{-.5f,Height-.5f,1,1,bottom},{Width-.5f,Height-.5f,1,1,bottom}};
        d->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);d->SetTexture(0,nullptr);
        d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
        d->SetRenderState(D3DRS_LIGHTING,FALSE);d->SetRenderState(D3DRS_ZENABLE,FALSE);d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
        d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(Screen));
    }
    void floor(bool winter,bool flat,IDirect3DTexture9*overrideTexture=nullptr){
        auto*d=device_.p;auto&m=flat?flat_:terrain_;auto*tex=overrideTexture?overrideTexture:winter?snow_.p:ground_.p;
        d->SetTexture(0,tex);d->SetFVF(0x212);d->SetStreamSource(0,m.buffer,0,sizeof(Vertex));
        auto call=[&](){return m.draw(d);};
        if(current_.grass)grass_->capture(current_,call,m.geometry());
        if(current_.puddles)effects_->sceneFX().captureSurface(current_,call,m.geometry());
        draw(m,tex,current_.materials);
    }
    void scenery(bool winter,bool flat=false){
        draw(winter?winterTrunks_:trunks_,winter?winterBark_.p:bark_.p,true,true);
        draw(rocks_,stone_,true);if(winter)draw(snowRocks_,snow_,true);
        auto*d=device_.p;
        d->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE);d->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATER);d->SetRenderState(D3DRS_ALPHAREF,110);
        draw(winter?winterCrowns_:crowns_,winter?winterFoliage_.p:foliage_.p,winter);
        d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
        if(flat){tint(0xffb7a08b);draw(posts_,bark_);tint(0xffffffff);}
    }
    void actors(const Settings &s){
        struct Actor {std::array<BYTE,0x700> bytes{};};
        Actor player;Random random;
        unsigned counts[4]{};
        for(int row=0;row<4;++row)for(int col=0;col<6;++col){
            int kind=col%3;Vec p{(col-2.5f)*2.5f,0,float(row)*5+1};
            Actor object;auto field=[&](size_t offset,auto value){memcpy(object.bytes.data()+offset,&value,sizeof(value));};
            field(0x170,DWORD(5));field(0x31c,int(kind==0));field(0x18c,DWORD(kind==2?5:0));field(0x6e8,DWORD(0));
            Vec delta=p-Vec{0,5,-11};field(0x2cc,std::sqrt(Dot(delta,delta)));
            if(!actor_visibility::visible(s,object.bytes.data(),player.bytes.data()))continue;
            ++counts[kind];Mesh body,head;
            p.y=0;
            if(kind==0){Ball(body,p+Vec{0,.9f,0},{.33f,.62f,.23f});Ball(head,p+Vec{0,1.7f,0},{.23f,.25f,.23f});
                Tube(body,{p+Vec{-.18f,.6f,0},p+Vec{-.24f,.12f,0}},{.13f,.10f});Tube(body,{p+Vec{.18f,.6f,0},p+Vec{.24f,.12f,0}},{.13f,.10f});}
            if(kind==1){Ball(body,p+Vec{0,.62f,0},{.62f,.55f,.42f});Ball(head,p+Vec{0,1.2f,-.1f},{.4f,.34f,.32f});
                Tube(body,{p+Vec{-.25f,1.35f,0},p+Vec{-.5f,1.8f,.1f}},{.17f,.008f});Tube(body,{p+Vec{.25f,1.35f,0},p+Vec{.5f,1.8f,.1f}},{.17f,.008f});}
            if(kind==2){Ball(body,p+Vec{0,.38f,0},{.4f,.32f,.5f});Ball(head,p+Vec{0,.72f,-.25f},{.27f,.26f,.27f});
                Tube(body,{p+Vec{-.17f,.8f,-.18f},p+Vec{-.24f,1.15f,-.14f}},{.14f,.008f});Tube(body,{p+Vec{.17f,.8f,-.18f},p+Vec{.24f,1.15f,-.14f}},{.14f,.008f});}
            body.upload(device_);head.upload(device_);
            tint(kind==0?0xff688cc6:kind==1?0xffb87358:0xffe7d29a);draw(body,white_);
            tint(kind==0?0xffe5cbb0:kind==1?0xffd7996d:0xfff1e1ba);draw(head,white_);tint(0xffffffff);
        }
        // The local player and peaceful NPC remain visible through the shipping predicate.
        for(int i=0;i<2;++i){Actor object;DWORD type=5,peaceful=1;memcpy(object.bytes.data()+0x170,&type,4);memcpy(object.bytes.data()+0x6e8,&peaceful,4);
            const void *ptr=i?object.bytes.data():player.bytes.data();Require(actor_visibility::visible(s,ptr,player.bytes.data()),"local/NPC preservation");
            Mesh m;Vec p{i?9.f:-9.f,.9f,-1};Ball(m,p,{.38f,.75f,.3f});Ball(m,p+Vec{0,.96f,0},{.27f,.28f,.27f});m.upload(device_);
            tint(i?0xffbac778:0xffb99adb);draw(m,white_);tint(0xffffffff);}
        printf("ACTORS players=%u monsters=%u pets=%u local=1 peaceful=1\n",counts[0],counts[1],counts[2]);
    }
    void trails(int mode,float fade){
        auto*d=device_.p;particles_->observe();
        // Identical source quads, alpha history and model scopes in every variant.
        for(int model=0;model<3;++model){
            native_refinement::Scope scope(&model);
            for(int i=22;i>=0;--i)for(int foot:{-1,1}){
                float age=float(i)/23;auto world=Identity();
                world._41=(model-1)*2.6f+std::sin(age*5+model)*.35f+foot*.14f;
                world._42=.25f+age*.12f;world._43=4.5f-age*7;
                d->SetTransform(D3DTS_WORLD,&world);d->SetTexture(0,aura_);
                d->SetRenderState(D3DRS_LIGHTING,FALSE);d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
                d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);
                d->SetFVF(0x142);
                struct P {float x,y,z;DWORD color;float u,v;};
                DWORD color=(DWORD((1-age)*185*fade)<<24)|0x00bacbd9;
                P q[]={{-.12f,.30f,0,color,0,0},{.12f,.30f,0,color,1,0},{-.12f,-.10f,0,color,0,1},{.12f,-.10f,0,color,1,1}};
                Settings s=current_;s.sceneQuality=mode!=1;
                bool active=mode&&particles_->begin(s,false,true,mode==1?ParticleDraw{}:ParticleDraw{D3DPT_TRIANGLESTRIP,2,q,sizeof(P)});
                if(active)++particleDraws_;
                d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(P));if(active)particles_->end(true);
            }
        }
        setup(false,true);
    }
    void nativeCards(bool fixed){
        auto*d=device_.p;d->SetTexture(0,card_);d->SetRenderState(D3DRS_LIGHTING,FALSE);
        d->SetRenderState(D3DRS_TEXTUREFACTOR,0x8090c85a);
        d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_TFACTOR);d->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_TFACTOR);
        d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
        for(int i=0;i<6;++i){Mesh m;Card(m,{(i-2.5f)*1.6f,.65f,1.5f+(i%2)*2.f},{.85f,0,0},{0,.7f,0});m.upload(d);
            d->SetFVF(0x112);
            if(fixed)effects_->sceneFX().beginGrassDepth(current_);m.draw(d,0x112);if(fixed)effects_->sceneFX().endGrassDepth();}
        setup();
    }
    std::vector<DWORD> read(){
        auto*d=device_.p;Com<IDirect3DSurface9> target,copy;d->GetRenderTarget(0,target.out());
        Require(SUCCEEDED(d->CreateOffscreenPlainSurface(Width,Height,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,copy.out(),nullptr)),"capture allocation");
        Require(SUCCEEDED(d->GetRenderTargetData(target,copy)),"GPU capture");D3DLOCKED_RECT lock{};
        Require(SUCCEEDED(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"capture lock");
        std::vector<DWORD> result(Width*Height);for(unsigned y=0;y<Height;++y)memcpy(result.data()+y*Width,(BYTE *)lock.pBits+y*lock.Pitch,Width*4);
        copy->UnlockRect();return result;
    }
    void save(const std::string &name,const std::vector<DWORD>&pixels){
        UINT n=0,size=0;Gdiplus::GetImageEncodersSize(&n,&size);std::vector<BYTE> bytes(size);auto*enc=(Gdiplus::ImageCodecInfo *)bytes.data();
        Gdiplus::GetImageEncoders(n,size,enc);CLSID png{};for(UINT i=0;i<n;++i)if(!wcscmp(enc[i].MimeType,L"image/png"))png=enc[i].Clsid;
        auto path=output_/(name+".png");Gdiplus::Bitmap image(Width,Height,Width*4,PixelFormat32bppRGB,(BYTE *)pixels.data());
        Require(image.Save(path.c_str(),&png,nullptr)==Gdiplus::Ok,"save PNG");
    }
  public:
    Renderer(std::filesystem::path artwork,std::filesystem::path output):artwork_(artwork),output_(output){
        std::filesystem::create_directories(output_);
        WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FENBFeatureShowcase";RegisterClassW(&wc);
        window_=CreateWindowW(wc.lpszClassName,L"FENB standalone feature renderer",WS_OVERLAPPEDWINDOW,30,30,Width,Height,nullptr,nullptr,wc.hInstance,nullptr);
        root_.p=Direct3DCreate9(D3D_SDK_VERSION);Require(root_.p,"Direct3DCreate9");
        D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.hDeviceWindow=window_;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
        pp.BackBufferWidth=Width;pp.BackBufferHeight=Height;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24X8;
        Require(SUCCEEDED(root_->CreateDevice(0,D3DDEVTYPE_HAL,window_,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,device_.out())),"DX9 device");
        d3dx_=LoadLibraryExW(L"d3dx9_43.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);Require(d3dx_,"system D3DX9 texture loader");
        load_=reinterpret_cast<LoadTexture>(GetProcAddress(d3dx_,"D3DXCreateTextureFromFileExW"));Require(load_,"D3DX9 texture API");
        grass_=std::make_unique<GrassFX>(device_);materials_=std::make_unique<MaterialFX>(device_);trees_=std::make_unique<TreeFX>(device_);
        particles_=std::make_unique<SfxFX>(device_);effects_=std::make_unique<Effects>(device_);
        load(ground_,L"World/Texture/ground_lawn001.dds");load(snow_,L"World/Texture/ground_n_ice02.dds");
        load(soil_,L"World/Texture/ground_clay001.dds");load(bark_,L"Model/Texture/Obj_MaCoPrTr13-02.dds");
        load(winterBark_,L"Model/Texture/Obj_AzriaTree01-B.dds");load(foliage_,L"Model/Texture/Obj_MaCoPrTr01-01.dds");
        load(winterFoliage_,L"Model/Texture/Obj_AzriaTree01-1.dds");load(aura_,L"SFX/Texture/sfx_EffectEnchant.dds");
        solid(white_,0xffffffff);solid(stone_,0xff9c9b99);
        // A repeatable paving atlas and fading grass card represent known fixture identities.
        device_->CreateTexture(256,256,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,paving_.out(),nullptr);
        for(UINT l=0;l<paving_->GetLevelCount();++l){D3DSURFACE_DESC desc{};paving_->GetLevelDesc(l,&desc);D3DLOCKED_RECT lock{};paving_->LockRect(l,&lock,nullptr,0);
            for(UINT y=0;y<desc.Height;++y)for(UINT x=0;x<desc.Width;++x){unsigned xx=x*256/desc.Width,yy=y*256/desc.Height;
                bool seam=xx%64<2||yy%48<2;unsigned noise=(xx*17+yy*23)%9;
                unsigned a=seam?76:145+noise,b=seam?78:143+noise,c=seam?80:136+noise;
                ((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=0xff000000|(a<<16)|(b<<8)|c;}paving_->UnlockRect(l);}
        device_->CreateTexture(128,128,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,card_.out(),nullptr);
        {D3DLOCKED_RECT lock{};card_->LockRect(0,&lock,nullptr,0);for(int y=0;y<128;++y)for(int x=0;x<128;++x){float u=x/127.f,v=y/127.f;float blade=std::abs(std::sin(u*39+v*2));
            unsigned alpha=blade>v*.8f+.18f?255:0;((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=(alpha<<24)|0x007e9e42;}card_->UnlockRect(0);DWORD tag=1024;card_->SetPrivateData(EmitterTag,&tag,4,0);}
        // Surface fixtures use the same catalog tags as the native integration tests.
        DWORD city=32,grass=2048,soil=4096;paving_->SetPrivateData(EmitterTag,&city,4,0);ground_->SetPrivateData(EmitterTag,&grass,4,0);soil_->SetPrivateData(EmitterTag,&soil,4,0);
        Require(SUCCEEDED(device_->CreateCubeTexture(32,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,skyCube_.out(),nullptr)),"sky cube");
        for(int face=0;face<6;++face){D3DLOCKED_RECT lock{};skyCube_->LockRect(D3DCUBEMAP_FACES(face),0,&lock,nullptr,0);
            for(int y=0;y<32;++y)for(int x=0;x<32;++x){float cloud=std::max(0.f,std::sin(x*.24f+face)*std::cos(y*.27f));
                ((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=D3DCOLOR_XRGB(105+int(cloud*65),130+int(cloud*58),150+int(cloud*48));}skyCube_->UnlockRect(D3DCUBEMAP_FACES(face),0);}
        Terrain(terrain_);Terrain(flat_,true);Random random;
        for(auto position:{Vec{-4,0,4},Vec{5,0,8},Vec{-8,0,15},Vec{1,0,22},Vec{11,0,22}}){position.y=Ground(position.x,position.z);
            Tree(trunks_,crowns_,position,position.z<10?1.f:.9f,false,random);Tree(winterTrunks_,winterCrowns_,position,1,true,random);}
        for(int i=0;i<16;++i){Vec p{(random.next()-.5f)*22,0,random.next()*24};p.y=Ground(p.x,p.z);
            Vec scale{.25f+random.next()*.45f,.15f+random.next()*.25f,.25f+random.next()*.35f};
            Ball(rocks_,p,scale);Ball(snowRocks_,p+Vec{0,scale.y*.62f,0},{scale.x*.93f,scale.y*.45f,scale.z*.95f});}
        for(float x:{-3.6f,3.6f})for(int i=0;i<5;++i){float z=2+i*3.3f;Tube(posts_,{{x,0,z},{x,1.05f,z}},{.14f,.13f});}
        for(float x:{-3.6f,3.6f})Tube(posts_,{{x,.8f,2},{x,.8f,15.2f}},{.075f,.075f});
        for(auto*m:{&terrain_,&flat_,&trunks_,&crowns_,&winterTrunks_,&winterCrowns_,&rocks_,&snowRocks_,&posts_})m->upload(device_);
        projection_={};projection_._11=1.25f;projection_._22=1.25f*Width/Height;projection_._33=160/159.9f;projection_._34=1;projection_._43=-.1f*projection_._33;
    }
    ~Renderer(){particles_.reset();effects_.reset();trees_.reset();materials_.reset();grass_.reset();if(d3dx_)FreeLibrary(d3dx_);if(window_)DestroyWindow(window_);}
    void frame(std::string name,std::string scene,Settings settings,float water=0,int mode=2,float fade=1){
        current_=settings;auto*d=device_.p;bool winter=scene=="winter"||scene=="snow"||scene=="winter-tree";
        bool puddles=scene=="paving"||scene=="soil"||scene=="wet-grass";bool sfx=scene=="sfx";bool actorsScene=scene=="actors";
        if(scene=="snow")view_=Look({1.8f,.72f,-1.3f},{0,0,2.1f});
        else if(scene=="tree-detail")view_=Look({-2.8f,1.8f,1.6f},{-4,1.7f,4});
        else if(scene=="tree")view_=Look({-1.5f,2.9f,-2.8f},{-4,2.65f,4});
        else if(scene=="winter-tree")view_=Look({-1.5f,2.9f,-2.8f},{-4,2.65f,4});
        else if(sfx)view_=Look({7,4.2f,-8},{0,.4f,1});
        else if(actorsScene)view_=Look({0,7,-13},{0,0,8});
        else if(puddles)view_=Look({5.5f,3.3f,-7},{0,.1f,5.8f});
        else if(scene=="ssao")view_=Look({0,3.8f,-6},{0,.15f,4});
        else view_=Look({7.5f,3.2f,-8},{0,1.1f,7.5f});
        setup(winter,sfx);effects_->sceneFX().beginFrame();particles_->beginFrame(true);trees_->beginFrame(true);
        float player[]={0,0,6};grass_->beginFrame(settings.grass,player);
        if(puddles){
            // Drive the real accumulation clock with deterministic one-second steps.
            Settings dry=settings;dry.puddles=false;
            effects_->sceneFX().updatePuddles(dry,0,0);
            effects_->sceneFX().updatePuddles(settings,1,0);
            unsigned seconds=unsigned(water*settings.puddleFillTime+.5f);
            for(unsigned i=1;i<=seconds;++i)effects_->sceneFX().updatePuddles(settings,1,i*1000);
        }
        effects_->sceneFX().skyEnvironment(skyCube_);
        Require(SUCCEEDED(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff8799a8,1,0)),"clear");
        Require(SUCCEEDED(d->BeginScene()),"BeginScene");sky(winter,sfx);effects_->sceneFX().observe(settings);
        if(puddles)floor(false,true,scene=="paving"?paving_.p:scene=="soil"?soil_.p:ground_.p);
        else floor(winter,actorsScene||sfx);
        if(actorsScene)actors(settings);
        else if(sfx){draw(rocks_,stone_);trails(mode,fade);}
        else {scenery(winter,puddles);if(scene=="ssao")nativeCards(mode!=0);}
        grass_->flush();grass_->draw(settings);effects_->sceneFX().flushSurfaces();
        if(settings.postEnabled()){
            StateGuard guard(d);guard.isolateTargets();Com<IDirect3DSurface9> target;d->GetRenderTarget(0,target.out());
            Require(effects_->prepare(target,settings),"post-processing prepare");effects_->draw(target,settings);
            if(settings.ssao||settings.puddles)Require(effects_->sceneFX().depthReady(),"required scene depth unavailable");
        }
        Require(SUCCEEDED(d->EndScene()),"EndScene");auto pixels=read();save(name,pixels);++frames_;
        printf("CAPTURE %s materials=%u trees=%u particles=%u grassBytes=%zu\n",name.c_str(),materialDraws_,treeDraws_,particleDraws_,grass_->bytes());
        checks_.emplace_back(name,std::move(pixels));
        // Each still is independent: no wetness, exposure or camera history leaks into the next.
        effects_->invalidate();grass_->invalidate();particles_->invalidate();
    }
    void verify(){
        Require(materialDraws_>0&&treeDraws_>0&&particleDraws_>0,"effect draw paths were not exercised");
        FILE*f=_wfopen((output_/L"pixel-checks.json").c_str(),L"wb");Require(f,"pixel check report");fputs("[\n",f);
        unsigned comparisons=0;
        for(size_t i=0;i+1<checks_.size();++i){auto &a=checks_[i],&b=checks_[i+1];
            if(!a.first.ends_with("-off")||b.first!=a.first.substr(0,a.first.size()-4)+"-on")continue;
            unsigned count=0;for(size_t p=0;p<a.second.size();++p)if((a.second[p]&0xffffff)!=(b.second[p]&0xffffff))++count;
            Require(count>100,"comparison has no visible pixel difference");
            fprintf(f,"%s{\"before\":\"%s.png\",\"after\":\"%s.png\",\"changedPixels\":%u,\"totalPixels\":%u}",comparisons++?",\n":"",a.first.c_str(),b.first.c_str(),count,Width*Height);
        }
        fputs("\n]\n",f);fclose(f);printf("PASS: %u captures; %u nonempty comparisons; shipping material, tree, grass, SFX, scene and actor paths.\n",frames_,comparisons);
    }
};
Settings Base(){Settings s;s.color=s.bloom=s.water=false;s.nightFoliage=0;s.sceneQuality=true;return s;}
}

int wmain(int argc,wchar_t **argv){
    if(argc!=3){puts("Usage: feature-showcase <read-only-client-artwork-root> <raw-output-directory>");return 2;}
    Gdiplus::GdiplusStartupInput startup;ULONG_PTR token=0;Gdiplus::GdiplusStartup(&token,&startup,nullptr);
    try{
        Renderer r(argv[1],argv[2]);Settings base=Base(),s=base;
        r.frame("01-grass-off","meadow",base);s.grass=true;s.grassQuality=2;s.grassWind=0;
        r.frame("01-grass-on","meadow",s);s.grassDensity=.55f;s.grassHeight=.24f;r.frame("01-grass-short","meadow",s);
        s.grassDensity=2;s.grassHeight=.85f;r.frame("01-grass-tall","meadow",s);
        r.frame("02-snow-grass-off","winter",base);s=base;s.grass=s.grassSnow=true;s.grassQuality=2;s.grassWind=0;r.frame("02-snow-grass-on","winter",s);
        r.frame("03-snow-off","snow",base);s=base;s.materials=true;r.frame("03-snow-on","snow",s);
        r.frame("04-winter-material-off","winter-tree",base);r.frame("04-winter-material-on","winter-tree",s);
        r.frame("05-trees-off","tree",base);s=base;s.trees=true;r.frame("05-trees-on","tree",s);
        r.frame("05-bark-detail-off","tree-detail",base);r.frame("05-bark-detail-on","tree-detail",s);
        r.frame("06-winter-trees-off","winter-tree",base);r.frame("06-winter-trees-on","winter-tree",s);
        for(auto item:{std::pair{"07-puddles-city","paving"},std::pair{"08-puddles-grass","wet-grass"},std::pair{"09-puddles-soil","soil"}}){
            s=base;s.puddles=true;s.puddleFillTime=20;
            r.frame(std::string(item.first)+"-off",item.second,s,0);r.frame(std::string(item.first)+"-on",item.second,s,1);
            if(std::string(item.second)=="paving"){r.frame("07-puddles-half","paving",s,.5f);s.puddleRipples=0;r.frame("07-puddles-no-ripples","paving",s,1);}
        }
        r.frame("10-refinement-off","sfx",base,0,0);s=base;s.sfx=true;r.frame("10-refinement-on","sfx",s);
        r.frame("10-refinement-wisps","sfx",s,0,1);r.frame("10-refinement-fading","sfx",s,0,2,.28f);
        s.sfxAuraCustomColor=true;s.sfxAuraR=.16f;s.sfxAuraG=.62f;s.sfxAuraB=1;r.frame("11-color-cyan","sfx",s);
        s.sfxAuraR=.65f;s.sfxAuraG=.22f;s.sfxAuraB=1;r.frame("11-color-violet","sfx",s);
        s.sfxAuraR=1;s.sfxAuraG=.28f;s.sfxAuraB=.08f;r.frame("11-color-amber","sfx",s);
        for(int i=0;i<3;++i){std::string id=i==0?"12-hide-players":i==1?"13-hide-monsters":"14-hide-pets";
            r.frame(id+"-off","actors",base);s=base;if(i==0)s.renderPlayers=false;if(i==1)s.renderMonsters=false;if(i==2)s.renderPets=false;
            r.frame(id+"-on","actors",s);
        }
        r.frame("15-distance-off","actors",base);s=base;s.playerDrawDistance=18;s.monsterDrawDistance=23;s.petDrawDistance=28;r.frame("15-distance-on","actors",s);
        for(int i=0;i<3;++i){std::string id=i==0?"15-distance-players":i==1?"15-distance-monsters":"15-distance-pets";
            r.frame(id+"-off","actors",base);s=base;if(i==0)s.playerDrawDistance=18;if(i==1)s.monsterDrawDistance=18;if(i==2)s.petDrawDistance=18;
            r.frame(id+"-on","actors",s);}
        s=base;s.ssao=true;s.aoStrength=1.2f;r.frame("16-grass-depth-off","ssao",s,0,0);r.frame("16-grass-depth-on","ssao",s,0,2);
        r.verify();
    }catch(const std::exception &e){fprintf(stderr,"ERROR: %s\n",e.what());Gdiplus::GdiplusShutdown(token);return 1;}
    Gdiplus::GdiplusShutdown(token);return 0;
}
