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
        return std::exp(-d*d)*.77f+.23f*std::exp(-d*d*.13f);
    }
    static float glow(Point p,Point center,float width,float height) {
        const float x=(p.x-center.x)/width,y=(p.y-center.y)/height;
        return std::exp(-x*x-y*y);
    }
    static float feather(float distance,float width) {
        const float outside=std::max(-distance,0.f)/width;
        return std::exp(-outside*outside);
    }
    static float mask(Style style,Point p) {
        constexpr float diagonal=.70710678f;
        const Point uv=p;
        const float phase=float(style)*1.71f;
        p.x+=.035f*std::sin(uv.y*7+phase)+.018f*std::sin(uv.y*18-uv.x*7);
        p.y+=.031f*std::sin(uv.x*8-phase)+.013f*std::sin(uv.x*17+uv.y*8);
        const float radius=p.x*p.x+p.y*p.y;
        const float grain=.90f+.08f*std::sin(p.x*25+p.y*13+phase)*std::sin(p.y*22-p.x*11)
            +.08f*std::sin(p.x*10-p.y*8+phase);
        float light=0;
        switch(style) {
        case Style::Stars:{
            const float cross=std::min(std::abs(p.x-p.y),std::abs(p.x+p.y))*diagonal;
            light=.59f*stroke(std::abs(p.x),.061f)*std::exp(-radius*2.7f)+
                .47f*stroke(std::abs(p.y),.072f)*std::exp(-radius*3.9f)+
                .38f*stroke(cross,.059f)*std::exp(-radius*6.3f)+
                .51f*glow(p,{-.045f,.025f},.22f,.25f)+.18f*std::exp(-radius*4);
            break;
        }
        case Style::Bolts:{
            const Point top{.25f,-.78f},elbow{-.33f,.04f},joint{.21f,-.06f},tip{-.24f,.74f};
            const float width=.060f+.052f*glow(p,{-.12f,.03f},.51f,.38f);
            const float bolt=std::min({segment(p,top,elbow),segment(p,elbow,joint),segment(p,joint,tip)});
            light=stroke(bolt,width)*(.76f+.30f*std::exp(-radius*3))+
                .41f*glow(p,{-.10f,.015f},.39f,.29f)+
                .25f*stroke(segment(p,{.10f,.13f},{.48f,.45f}),.032f);
            break;
        }
        case Style::Triangles:{
            // The three planes bound a luminous shard with a brighter tilted face.
            const float distance=std::min({.49f-p.y,.51f*p.y+.36f-.87f*p.x,.48f*p.y+.34f+.88f*p.x});
            light=feather(distance,.137f)*(.39f+.57f*glow(p,{-.13f,.09f},.32f,.48f))
                +.24f*glow(p,{-.23f,.08f},.42f,.44f);
            break;
        }
        case Style::Rings:{
            const float angle=std::atan2(p.y,p.x),arc=.5f+.5f*std::cos(angle-.9f);
            const float round=std::hypot(p.x,.93f*p.y);
            const float ripple=.53f+.05f*std::sin(angle*3+.6f)+.02f*std::cos(angle*5);
            light=(.25f+.67f*arc)*stroke(std::abs(round-ripple),.057f+.055f*arc)+
                .27f*arc*stroke(std::abs(round-ripple*.65f),.073f)+
                .28f*glow(p,{.33f,.33f},.35f,.32f);
            break;
        }
        case Style::Crystals:{
            const Point q{p.x+.22f*p.y,p.y};
            const float distance=.54f-std::abs(q.x)-.69f*std::abs(q.y);
            const float facet=glow(q,{-.15f,-.12f},.21f,.43f);
            light=feather(distance,.109f)*(.30f+.56f*facet)+
                .29f*glow(q,{.04f,.13f},.30f,.57f)+.25f*glow(q,{.01f,-.08f},.16f,.22f);
            break;
        }
        case Style::Game:break;
        }
        const float edge=std::clamp((1-std::max(std::abs(uv.x),std::abs(uv.y)))*6,0.f,1.f);
        return std::min(light*grain,1.f)*edge*edge*(3-2*edge);
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
