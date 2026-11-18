#pragma once
#include "common.hpp"
#include "trail_geometry.hpp"
#include <array>
#include <span>

namespace skate_trail {
enum class Style { Game, Mist, Ribbons, Stardust, Lightning, Embers };
enum class Mask { Vapor, Halo, Spark, Cloud, Ember, Count };
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
    std::array<Vec,512> sides_{};
    size_t size_=0;
public:
    explicit Path(std::span<const Point> points):points_(points) {
        if(points.size()<2||points.size()>distances_.size())return;
        sides_[0]=points[0].across;
        for(size_t i=1;i<points.size();++i) {
            const float distance=length(points[i].center-points[i-1].center);
            if(!std::isfinite(distance)||distance>8)break;
            distances_[i]=distances_[i-1]+distance;
            // These are side axes, so opposite orientations are equivalent.
            // Keep their sign consistent across abrupt native yaw changes.
            const auto across=points[i].across;
            sides_[i]=dot(across,sides_[i-1])<0?across*(-1.f):across;
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
        const Vec side=sides_[a]+(sides_[b]-sides_[a])*t;
        const float width=length(side);
        return {p.center+(q.center-p.center)*t,width>.001f?side*(1/width):sides_[a],
                p.opacity+(q.opacity-p.opacity)*t};
    }
};

class Renderer {
    IDirect3DDevice9 *device_;
    std::array<Com<IDirect3DTexture9>,size_t(Mask::Count)> masks_;
    Com<IDirect3DStateBlock9> saved_;
    std::vector<Vertex> vertices_;
    D3DMATRIX view_{};
    static float density(Mask mask,float u,float v) {
        if(mask==Mask::Vapor) {
            const float phase=v*3.14159265f;
            const float swirl=u+.21f*std::sin(phase)+.12f*std::sin(3*phase+u*2);
            return std::exp(-swirl*swirl*3.2f)*smooth((1-std::abs(u))*3)
                *(.70f+.22f*std::sin(phase*2+u*5)+.08f*std::cos(phase*4-u*5));
        }
        if(mask==Mask::Halo)return std::exp(-u*u*3.7f)*smooth((1-std::abs(u))*4);
        const float radius=u*u+v*v;
        if(mask==Mask::Cloud) {
            const float lobes=.84f+.11f*std::cos(u*9+v*4)+.05f*std::sin(v*11-u*3);
            return std::exp(-radius*2.8f)*smooth((1-radius)*2)*lobes;
        }
        if(mask==Mask::Ember)return std::exp(-u*u*14-v*v*5)*smooth((1-radius)*3);
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
        if(vertices_.empty())return true;
        const bool ok=SUCCEEDED(device_->SetTexture(0,masks_[size_t(mask)]))&&
            SUCCEEDED(device_->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(vertices_.size()/3),vertices_.data(),sizeof(Vertex)));
        vertices_.clear();
        return ok;
    }
    struct Strip {
        float width,offset,wave,lift,intensity,fraction;
        DWORD rgb;
        Mask mask;
        float start=0;
        bool electric=false;
    };
    trail_geometry::Vec toView(Vec p) const { return trail_geometry::transform({p.x,p.y,p.z},view_); }
    static float zigzag(float phase) {
        const int step=int(std::floor(phase));
        const float fraction=phase-step;
        const float a=(step%2?-1.f:1.f)*(.38f+.62f*random(unsigned(step)));
        const float b=(step%2?1.f:-1.f)*(.38f+.62f*random(unsigned(step+1)));
        return a+(b-a)*fraction;
    }
    bool strip(const Path &path,const Strip &style) {
        constexpr int Steps=100;
        const float extent=path.lengthInWorld()*style.fraction;
        std::array<trail_geometry::Knot,Steps+1> knots{};
        for(int i=0;i<=Steps;++i) {
            const float age=float(i)/Steps,distance=style.start*path.lengthInWorld()+age*extent;
            auto sample=path.at(distance);
            // Smooth sampled position along arc length. Keep the board anchor
            // and the far endpoint in place.
            const float window=std::min({.42f,distance,path.lengthInWorld()-distance});
            if(window>.001f)
                sample.center=sample.center*.5f+(path.at(distance-window).center+path.at(distance+window).center)*.25f;
            const Vec across=sample.across;
            const float phase=dot(sample.center,{.57f,.22f,1.1f});
            const float wave=style.electric?zigzag(phase*2.1f):std::sin(phase);
            const auto p=sample.center+across*((style.offset+style.wave*wave)*smooth(age*7))
                         +Vec{0,style.lift*smooth(age*8),0};
            const float turbulence=style.mask==Mask::Vapor?1+.22f*std::sin(phase*1.7f):1;
            const float width=style.width*(.72f+smooth(age*7)*.46f)*(1-.36f*age)*turbulence;
            const float strength=style.intensity*sample.opacity*(1-smooth((age-.07f)/.93f));
            knots[i]={toView(p),width,color(style.rgb,strength),phase*.27f};
        }
        trail_geometry::ribbon(knots,true,[&](auto a,auto b,auto c,auto d){
            auto vertex=[](const auto &v){return Vertex{{v.p.x,v.p.y,v.p.z},v.tint,v.u,v.v};};
            quad(vertex(a),vertex(b),vertex(c),vertex(d));
        },!style.electric);
        return flush(style.mask);
    }
    void sprite(Vec world,float width,float height,DWORD tint) {
        const auto view=toView(world);
        const Vec p{view.x,view.y,view.z},a{width,0,0},b{0,height,0};
        if(p.z<.025f)return;
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
        D3DMATRIX world{};
        if(FAILED(device_->GetTransform(D3DTS_WORLD,&world))||FAILED(device_->GetTransform(D3DTS_VIEW,&view_))||
           FAILED(saved_->Capture()))return false;
        D3DMATRIX identity{};
        identity._11=identity._22=identity._33=identity._44=1;
        bool ok=SUCCEEDED(device_->SetTransform(D3DTS_WORLD,&identity))&&
            SUCCEEDED(device_->SetTransform(D3DTS_VIEW,&identity))&&
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
                ok=strip(path,{.77f,0,.24f,.13f,.25f,.94f,tint(0x549cef),Mask::Vapor})&&
                   strip(path,{.36f,-.10f,-.30f,.07f,.24f,.78f,tint(0xa8e4ff),Mask::Vapor});
                const float extent=path.lengthInWorld();
                const int wisps=std::clamp(int(extent/.65f),4,32);
                for(int i=0;i<wisps;++i) {
                    const float age=(i+.45f)/wisps;
                    const auto p=path.at(age*extent);
                    const float phase=dot(p.center,{.92f,.41f,.73f});
                    const float spread=.16f+age*.51f;
                    const Vec center=p.center+p.across*(std::sin(phase*1.65f)*spread)
                        +Vec{0,(.14f+.11f*std::cos(phase))*smooth(age*6),0};
                    const float size=(.37f+.41f*age)*(.85f+.15f*std::sin(phase*1.7f));
                    const float opacity=.23f*(1-smooth(age))*p.opacity;
                    sprite(center,size,size*.79f,color(tint(0x829deb),opacity));
                }
                ok=flush(Mask::Cloud)&&ok;
            } else if(style==Style::Ribbons) {
                ok=strip(path,{.69f,0,0,0,.13f,1,tint(0x729eff),Mask::Halo});
                for(float side:{-1.f,1.f}) {
                    const DWORD rgb=tint(side<0?0x69ecff:0xcf9aff);
                    ok=ok&&strip(path,{.17f,side*.23f,side*.30f,side*.13f,.37f,1,rgb,Mask::Halo})&&
                           strip(path,{.048f,side*.23f,side*.30f,side*.13f,.95f,1,rgb,Mask::Halo});
                }
            } else if(style==Style::Stardust) {
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
                    sprite(position,radius,radius,color(tint(burst?0xc5ddff:0xa79aff),opacity));
                }
                ok=flush(Mask::Spark)&&ok;
            } else if(style==Style::Lightning) {
                ok=strip(path,{.33f,0,.32f,.05f,.32f,.80f,tint(0xffbd38),Mask::Halo,0,true})&&
                   strip(path,{.061f,0,.32f,.05f,1,.80f,tint(0xffeb62),Mask::Halo,0,true});
                for(float side:{-1.f,1.f}) {
                    const float start=side<0?.15f:.32f;
                    ok=ok&&strip(path,{.046f,side*.62f,.37f,.09f,.68f,.23f,
                                       tint(0xffd444),Mask::Halo,start,true});
                }
            } else if(style==Style::Embers) {
                ok=strip(path,{.37f,0,.1f,.12f,.23f,.60f,tint(0xff6121),Mask::Vapor});
                const float extent=path.lengthInWorld();
                const int count=std::clamp(int(extent*6),5,100);
                for(int i=1;i<=count;++i) {
                    const float age=(i-.5f)/count;
                    const auto p=path.at(age*extent);
                    const unsigned seed=unsigned(i)*23u;
                    const float spread=.09f+.83f*age;
                    const Vec position=p.center+p.across*((random(seed)*2-1)*spread)
                        +Vec{0,age*(.3f+random(seed+1)*.8f),0};
                    const float radius=.026f+random(seed+2)*.06f;
                    const float opacity=smooth(age*24)*(1-smooth(age))*(.6f+random(seed+3)*.4f)*p.opacity;
                    sprite(position,radius,radius*(2+random(seed+4)*1.7f),
                           color(tint(random(seed+5)>.55f?0xffd35c:0xff6520),opacity));
                }
                ok=flush(Mask::Ember)&&ok;
            }
            DWORD head=0x99eaff;
            if(style==Style::Embers)head=0xff9b46;
            if(style==Style::Stardust)head=0x929bff;
            if(style==Style::Lightning)head=0xffe575;
            sprite(points.front().center,.35f,.35f,color(tint(head),.43f*points.front().opacity));
            ok=flush(Mask::Spark)&&ok;
        }
        const HRESULT restored=saved_->Apply();
        device_->SetTransform(D3DTS_WORLD,&world);
        device_->SetTransform(D3DTS_VIEW,&view_);
        return ok&&SUCCEEDED(restored);
    }
};
}
