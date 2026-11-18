#pragma once
#include "common.hpp"
#include <array>
#include <span>

namespace skate_trail {
enum class Style { Game, Mist, Ribbons, Stardust };
enum class Mask { Vapor, Halo, Spark, Count };
struct Vec {
    float x=0,y=0,z=0;
    Vec operator+(Vec b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec operator-(Vec b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec operator*(float k) const { return {x*k,y*k,z*k}; }
};
inline float dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline bool finite(Vec p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
inline float length(Vec v) { return std::sqrt(dot(v,v)); }
inline float smooth(float x) { x=std::clamp(x,0.f,1.f); return x*x*(3-2*x); }
inline float random(unsigned seed) {
    seed^=seed>>16;
    seed*=0x7feb352du;
    seed^=seed>>15;
    return float(seed&0xffffu)/65535.f;
}
inline DWORD color(DWORD rgb,float opacity) {
    return (rgb&0xffffffu)|(DWORD(std::clamp(opacity,0.f,1.f)*255+.5f)<<24);
}
struct Point { Vec center,across; float opacity=1; };
struct Vertex { Vec position; DWORD tint; float u,v; };
static_assert(sizeof(Vertex)==24);

// Samples are ordered from the board to the tail. Resample by distance so the
// result does not depend on the legacy emission rate or the number of draws.
class Path {
    std::span<const Point> points_;
    std::array<float,512> distances_{};
    size_t size_=0;
public:
    explicit Path(std::span<const Point> points):points_(points) {
        if(points.size()<2||points.size()>distances_.size())return;
        for(size_t i=1;i<points.size();++i) {
            const float distance=length(points[i].center-points[i-1].center);
            if(!std::isfinite(distance)||distance>8)break;
            distances_[i]=distances_[i-1]+distance;
            size_=i+1;
        }
    }
    float lengthInWorld() const { return size_>1?distances_[size_-1]:0; }
    Point at(float distance) const {
        const auto end=distances_.begin()+size_;
        size_t b=size_t(std::upper_bound(distances_.begin(),end,distance)-distances_.begin());
        b=std::clamp<size_t>(b,1,size_-1);
        const size_t a=b-1;
        const float t=std::clamp((distance-distances_[a])/std::max(.00001f,distances_[b]-distances_[a]),0.f,1.f);
        const auto &p=points_[a],&q=points_[b];
        return {p.center+(q.center-p.center)*t,p.across+(q.across-p.across)*t,
                p.opacity+(q.opacity-p.opacity)*t};
    }
};

class Renderer {
    IDirect3DDevice9 *device_;
    std::array<Com<IDirect3DTexture9>,size_t(Mask::Count)> masks_;
    Com<IDirect3DStateBlock9> saved_;
    std::vector<Vertex> vertices_;
    static float density(Mask mask,float u,float v) {
        if(mask==Mask::Vapor) {
            const float phase=v*3.14159265f;
            const float swirl=u+.12f*std::sin(phase)+.07f*std::sin(3*phase+u*2);
            return std::exp(-swirl*swirl*3.2f)*smooth((1-std::abs(u))*3)
                *(.79f+.16f*std::sin(phase*2+u*5)+.05f*std::cos(phase*4-u*5));
        }
        if(mask==Mask::Halo)return std::exp(-u*u*3.7f)*smooth((1-std::abs(u))*4);
        const float radius=u*u+v*v;
        const float flare=std::exp(-std::min(u*u,v*v)*980)*std::exp(-std::max(u*u,v*v)*3.8f);
        return std::min(1.f,std::exp(-radius*16)*.88f+flare*.42f)*smooth((1-std::sqrt(radius))*5);
    }
    bool prepare() {
        constexpr int Size=128;
        for(size_t i=0;i<masks_.size();++i) {
            auto &texture=masks_[i];
            if(texture.p)continue;
            if(FAILED(device_->CreateTexture(Size,Size,1,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,texture.out(),nullptr)))return false;
            Com<IDirect3DTexture9> upload;
            if(FAILED(device_->CreateTexture(Size,Size,1,0,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,upload.out(),nullptr))) {
                texture.out();
                return false;
            }
            D3DLOCKED_RECT locked{};
            if(FAILED(upload->LockRect(0,&locked,nullptr,0))) { texture.out(); return false; }
            for(int y=0;y<Size;++y) {
                auto *row=reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits)+y*locked.Pitch);
                for(int x=0;x<Size;++x)
                    row[x]=color(0xffffff,density(Mask(i),(x+.5f)*2/Size-1,(y+.5f)*2/Size-1));
            }
            if(FAILED(upload->UnlockRect(0))||FAILED(device_->UpdateTexture(upload,texture))) {
                texture.out();
                return false;
            }
        }
        return saved_.p||SUCCEEDED(device_->CreateStateBlock(D3DSBT_ALL,saved_.out()));
    }
    void quad(Vertex a,Vertex b,Vertex c,Vertex d) {
        const Vertex triangles[]={a,b,c,c,b,d};
        vertices_.insert(vertices_.end(),std::begin(triangles),std::end(triangles));
    }
    bool flush(Mask mask) {
        const bool ok=SUCCEEDED(device_->SetTexture(0,masks_[size_t(mask)]))&&
            SUCCEEDED(device_->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(vertices_.size()/3),vertices_.data(),sizeof(Vertex)));
        vertices_.clear();
        return ok;
    }
    struct Strip { float width,offset,wave,lift,intensity,fraction; DWORD rgb; Mask mask; };
    bool strip(const Path &path,const Strip &style) {
        constexpr int Steps=100;
        const float extent=path.lengthInWorld()*style.fraction;
        Vertex left{},right{};
        for(int i=0;i<=Steps;++i) {
            const float age=float(i)/Steps,distance=age*extent;
            const auto sample=path.at(distance);
            const Vec across=sample.across;
            const float phase=dot(sample.center,{.57f,.22f,1.1f});
            const auto p=sample.center+across*((style.offset+style.wave*std::sin(phase))*smooth(age*7))
                         +Vec{0,style.lift*smooth(age*8),0};
            const float width=style.width*(.72f+smooth(age*7)*.46f)*(1-.36f*age);
            const float strength=style.intensity*sample.opacity*(1-smooth((age-.13f)/.87f));
            const DWORD tint=color(style.rgb,strength);
            Vertex a{p-across*width,tint,0,phase*.2f},b{p+across*width,tint,1,phase*.2f};
            if(i)quad(left,right,a,b);
            left=a;
            right=b;
        }
        return flush(style.mask);
    }
    void sprite(Vec p,Vec right,Vec up,float radius,DWORD tint) {
        const Vec a=right*radius,b=up*radius;
        quad({p-a+b,tint,0,0},{p+a+b,tint,1,0},{p-a-b,tint,0,1},{p+a-b,tint,1,1});
    }
public:
    explicit Renderer(IDirect3DDevice9 *device):device_(device) {}
    void invalidate() {
        saved_.out();
        for(auto &mask:masks_)mask.out();
    }
    size_t bytes() const {
        size_t total=0;
        for(const auto &mask:masks_)if(mask.p)total+=128*128*4;
        return total;
    }
    bool draw(std::span<const Point> points,Style style,DWORD customColor=0,bool custom=false) {
        Path path(points);
        if(style==Style::Game||path.lengthInWorld()<.05f||!prepare())return false;
        D3DMATRIX world{},view{};
        if(FAILED(device_->GetTransform(D3DTS_WORLD,&world))||FAILED(device_->GetTransform(D3DTS_VIEW,&view))||
           FAILED(saved_->Capture()))return false;
        const Vec right{view._11,view._21,view._31},up{view._12,view._22,view._32};
        D3DMATRIX identity{};
        identity._11=identity._22=identity._33=identity._44=1;
        bool ok=SUCCEEDED(device_->SetTransform(D3DTS_WORLD,&identity))&&
            SUCCEEDED(device_->SetVertexShader(nullptr))&&SUCCEEDED(device_->SetPixelShader(nullptr))&&
            SUCCEEDED(device_->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1));
        auto state=[&](D3DRENDERSTATETYPE kind,DWORD value){ok &= SUCCEEDED(device_->SetRenderState(kind,value));};
        auto stage=[&](DWORD n,D3DTEXTURESTAGESTATETYPE kind,DWORD value){ok &= SUCCEEDED(device_->SetTextureStageState(n,kind,value));};
        auto sampler=[&](D3DSAMPLERSTATETYPE kind,DWORD value){ok &= SUCCEEDED(device_->SetSamplerState(0,kind,value));};
        state(D3DRS_LIGHTING,FALSE);
        state(D3DRS_FOGENABLE,FALSE);
        state(D3DRS_ALPHATESTENABLE,FALSE);
        state(D3DRS_ZWRITEENABLE,FALSE);
        state(D3DRS_CULLMODE,D3DCULL_NONE);
        state(D3DRS_ALPHABLENDENABLE,TRUE);
        state(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
        state(D3DRS_BLENDOP,D3DBLENDOP_ADD);
        state(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
        state(D3DRS_DESTBLEND,D3DBLEND_ONE);
        stage(0,D3DTSS_COLOROP,D3DTOP_MODULATE);
        stage(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
        stage(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        stage(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
        stage(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
        stage(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
        stage(0,D3DTSS_TEXCOORDINDEX,0);
        stage(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
        stage(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        sampler(D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
        sampler(D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
        sampler(D3DSAMP_MIPFILTER,D3DTEXF_NONE);
        sampler(D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
        sampler(D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
        sampler(D3DSAMP_SRGBTEXTURE,FALSE);
        auto tint=[&](DWORD value){return custom?customColor:value;};
        if(ok) {
            if(style==Style::Mist) {
                ok=strip(path,{1.05f,0,.11f,.13f,.29f,1,tint(0x5276ef),Mask::Vapor})&&
                   strip(path,{.62f,.09f,.17f,.05f,.46f,.92f,tint(0x54efed),Mask::Vapor})&&
                   strip(path,{.29f,-.11f,-.21f,.04f,.35f,.76f,tint(0xafe9ff),Mask::Vapor});
            } else if(style==Style::Ribbons) {
                ok=strip(path,{.69f,0,0,0,.13f,1,tint(0x729eff),Mask::Halo});
                for(float side:{-1.f,1.f}) {
                    const DWORD rgb=tint(side<0?0x69ecff:0xcf9aff);
                    ok=ok&&strip(path,{.17f,side*.23f,side*.30f,side*.13f,.37f,1,rgb,Mask::Halo})&&
                           strip(path,{.048f,side*.23f,side*.30f,side*.13f,.95f,1,rgb,Mask::Halo});
                }
            } else {
                ok=strip(path,{.76f,0,.06f,0,.19f,1,tint(0x7276ff),Mask::Vapor});
                const float extent=path.lengthInWorld();
                const int count=std::clamp(int(extent*13),2,184);
                for(int i=1;i<=count;++i) {
                    const float age=(i-.5f)/count;
                    const auto p=path.at(age*extent);
                    const unsigned seed=unsigned(i)*13u;
                    const float spread=.12f+age*.82f;
                    const Vec position=p.center+p.across*((random(seed)*2-1)*spread)
                        +Vec{0,(random(seed+1)*2-1)*spread*.45f,0};
                    const bool burst=random(seed+2)>.94f;
                    const float radius=burst?.19f+random(seed+3)*.13f:.029f+random(seed+3)*.05f;
                    const float opacity=smooth(age*32)*(1-smooth(age))*(.59f+random(seed+4)*.41f)*p.opacity;
                    sprite(position,right,up,radius,color(tint(burst?0xc5ddff:0xa79aff),opacity));
                }
                ok=flush(Mask::Spark)&&ok;
            }
            sprite(points.front().center,right,up,.35f,color(tint(style==Style::Stardust?0x929bff:0x99eaff),.43f*points.front().opacity));
            ok=flush(Mask::Spark)&&ok;
        }
        const HRESULT restored=saved_->Apply();
        device_->SetTransform(D3DTS_WORLD,&world);
        return ok&&SUCCEEDED(restored);
    }
};
}
