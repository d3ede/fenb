#pragma once
#include "common.hpp"
#include <array>

namespace skate_particles {
enum class Style { Game, Stars, Bolts, Triangles, Rings, Crystals };

inline DWORD palette(Style style) {
    constexpr DWORD colors[]={0xa67aff,0xa67aff,0xffd252,0x51deff,0x64f5ac,0xff78c3};
    return colors[size_t(style)];
}

class Renderer {
    struct Point {float x,y;};
    static constexpr int Size=64;
    IDirect3DDevice9 *device_;
    std::array<Com<IDirect3DTexture9>,5> masks_;
    Com<IDirect3DBaseTexture9> original_;
    DWORD factor_=0;
    static constexpr D3DTEXTURESTAGESTATETYPE keys_[]={D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2};
    DWORD stage_[std::size(keys_)]{};

    static float segment(Point p,Point a,Point b) {
        const float x=b.x-a.x,y=b.y-a.y;
        const float t=std::clamp(((p.x-a.x)*x+(p.y-a.y)*y)/(x*x+y*y),0.f,1.f);
        return std::hypot(p.x-a.x-t*x,p.y-a.y-t*y);
    }
    static float stroke(float distance,float width) {
        const float d=distance/width;
        return std::exp(-d*d)*.84f+.16f*std::exp(-d*d*.11f);
    }
    static float mask(Style style,Point p) {
        constexpr float diagonal=.70710678f;
        const float radius=p.x*p.x+p.y*p.y;
        float light=0;
        switch(style) {
        case Style::Stars:{
            const float axis=std::min(std::abs(p.x),std::abs(p.y));
            const float cross=std::min(std::abs(p.x-p.y),std::abs(p.x+p.y))*diagonal;
            light=.64f*stroke(axis,.045f)*std::exp(-radius*2.9f)+
                .48f*stroke(cross,.046f)*std::exp(-radius*5.8f)+.55f*std::exp(-radius*22);
            break;
        }
        case Style::Bolts:{
            const Point top{.24f,-.76f},elbow{-.32f,.06f},joint{.22f,-.05f},tip{-.24f,.76f};
            const float bolt=std::min({segment(p,top,elbow),segment(p,elbow,joint),segment(p,joint,tip)});
            light=stroke(bolt,.078f);
            break;
        }
        case Style::Triangles:{
            const Point top{0,-.69f},left{-.67f,.48f},right{.67f,.48f};
            light=stroke(std::min({segment(p,top,left),segment(p,left,right),segment(p,right,top)}),.075f);
            break;
        }
        case Style::Rings:{
            const float round=std::sqrt(radius);
            light=stroke(std::abs(round-.57f),.07f)+.26f*stroke(std::abs(round-.30f),.042f);
            break;
        }
        case Style::Crystals:{
            const Point q{std::abs(p.x),std::abs(p.y)};
            light=stroke(segment(q,{0,.76f},{.46f,0}),.069f)
                +.45f*stroke(segment(q,{0,.76f},{0,0}),.045f)+.30f*std::exp(-radius*12);
            break;
        }
        case Style::Game:break;
        }
        const float edge=std::clamp((1-std::max(std::abs(p.x),std::abs(p.y)))*6,0.f,1.f);
        return std::min(light,1.f)*edge*edge*(3-2*edge);
    }
    IDirect3DTexture9 *prepare(Style style) {
        auto &texture=masks_[size_t(style)-1];
        if(texture.p)return texture.p;
        Com<IDirect3DTexture9> candidate,upload;
        if(FAILED(device_->CreateTexture(Size,Size,1,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,candidate.out(),nullptr))||
           FAILED(device_->CreateTexture(Size,Size,1,0,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,upload.out(),nullptr)))return nullptr;
        D3DLOCKED_RECT lock{};
        if(FAILED(upload->LockRect(0,&lock,nullptr,0)))return nullptr;
        for(int y=0;y<Size;++y) {
            auto *row=reinterpret_cast<DWORD *>(static_cast<BYTE *>(lock.pBits)+y*lock.Pitch);
            for(int x=0;x<Size;++x) {
                const DWORD intensity=DWORD(mask(style,{(x+.5f)*2/Size-1,(y+.5f)*2/Size-1})*255+.5f);
                row[x]=0xff000000u|intensity*0x010101u;
            }
        }
        if(FAILED(upload->UnlockRect(0))||FAILED(device_->UpdateTexture(upload,candidate)))return nullptr;
        texture.p=candidate.p;
        candidate.p=nullptr;
        return texture.p;
    }
public:
    explicit Renderer(IDirect3DDevice9 *device):device_(device) {}
    bool begin(Style style,DWORD rgb,bool custom) {
        if(original_.p||(!custom&&style==Style::Game))return false;
        if(style==Style::Game)style=Style::Stars;
        auto *maskTexture=prepare(style);
        if(!maskTexture||FAILED(device_->GetRenderState(D3DRS_TEXTUREFACTOR,&factor_)))return false;
        for(size_t i=0;i<std::size(keys_);++i)
            if(FAILED(device_->GetTextureStageState(0,keys_[i],stage_+i)))return false;
        if(FAILED(device_->GetTexture(0,original_.out()))||!original_.p)return false;
        const DWORD color=0xff000000u|(custom?rgb:palette(style));
        if(SUCCEEDED(device_->SetTexture(0,maskTexture))&&
           SUCCEEDED(device_->SetRenderState(D3DRS_TEXTUREFACTOR,color))) {
            const DWORD replacement[]={D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_TFACTOR};
            size_t i=0;
            for(;i<std::size(keys_);++i)
                if(FAILED(device_->SetTextureStageState(0,keys_[i],replacement[i])))break;
            if(i==std::size(keys_))return true;
        }
        end();
        return false;
    }
    void end() {
        if(!original_.p)return;
        device_->SetTexture(0,original_);
        original_.out();
        device_->SetRenderState(D3DRS_TEXTUREFACTOR,factor_);
        for(size_t i=0;i<std::size(keys_);++i)device_->SetTextureStageState(0,keys_[i],stage_[i]);
    }
    void invalidate() {
        end();
        for(auto &mask:masks_)mask.out();
    }
    size_t bytes() const {
        size_t total=0;
        for(const auto &mask:masks_)if(mask.p)total+=Size*Size*4;
        return total;
    }
};
}
