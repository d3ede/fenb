#pragma once
#include "common.hpp"
#include <array>

namespace grass_card {
inline constexpr unsigned Size=256;
inline constexpr size_t Bytes=(Size*Size*4-1)/3*4;

// Painted narrow leaves on transparent crossed cards. Staggered roots and
// independently curved tips keep each tuft open instead of forming a broad fan.
inline std::vector<DWORD> paint(){
    struct Leaf {float root,tip,height,width,bend,tint;};
    constexpr Leaf leaves[]={
        {.19f,.05f,.67f,.043f,.034f,-.02f},
        {.30f,.48f,.97f,.054f,-.060f,.02f},
        {.39f,.16f,.84f,.048f,.052f,.00f},
        {.46f,.63f,.73f,.040f,-.022f,-.03f},
        {.54f,.36f,1.00f,.055f,.075f,.03f},
        {.62f,.85f,.89f,.047f,-.041f,.01f},
        {.72f,.57f,.80f,.044f,.034f,-.01f},
        {.81f,.96f,.62f,.038f,-.025f,-.03f},
    };
    std::vector<DWORD> pixels(Size*Size);
    for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x){
        const float u=(x+.5f)/Size,v=1-(y+.5f)/Size;
        float alpha=0,nearest=2;
        std::array<float,3> rgb{},edge{};
        for(const auto &leaf:leaves){
            const float t=std::min(v/leaf.height,1.f);
            const float center=std::lerp(leaf.root,leaf.tip,t*t*.78f+t*.22f)+leaf.bend*4*t*(1-t);
            const float half=leaf.width*.5f*(.76f+.36f*std::sin(t*3.14159265f))*(1-t);
            const float distance=std::max(std::abs(u-center)-half,v-leaf.height);
            const float left=std::max(u-.5f/Size,center-half);
            const float right=std::min(u+.5f/Size,center+half);
            const float coverage=std::max(0.f,(right-left)*Size)*std::clamp((leaf.height-v)*Size+.5f,0.f,1.f);
            const float across=std::clamp((u-center)/std::max(half,1.f/Size),-1.f,1.f);
            const float rib=std::exp(-32*(across+.18f)*(across+.18f));
            const float shade=.72f+.22f*(1-across)*.5f+rib*.12f;
            const float tip=std::sqrt(t);
            const std::array<float,3> color={
                (.075f+.29f*tip+leaf.tint)*shade,
                (.20f+.36f*tip+leaf.tint*.6f)*shade,
                (.018f+.043f*tip)*shade,
            };
            if(distance<nearest){nearest=distance;edge=color;}
            for(int c=0;c<3;++c)rgb[c]=color[c]*coverage+rgb[c]*(1-coverage);
            alpha=coverage+alpha*(1-coverage);
        }
        auto channel=[](float value){return DWORD(std::clamp(value,0.f,1.f)*255+.5f);};
        pixels[y*Size+x]=D3DCOLOR_ARGB(channel(alpha),channel(alpha>0?rgb[0]/alpha:edge[0]),
            channel(alpha>0?rgb[1]/alpha:edge[1]),channel(alpha>0?rgb[2]/alpha:edge[2]));
    }
    return pixels;
}

inline void downsample(std::vector<DWORD> &pixels,unsigned size){
    for(unsigned y=0;y<size/2;++y)for(unsigned x=0;x<size/2;++x){
        const size_t i=2*y*size+2*x;
        const DWORD samples[]={pixels[i],pixels[i+1],pixels[i+size],pixels[i+size+1]};
        unsigned alpha=0;
        for(DWORD sample:samples)alpha+=sample>>24;
        DWORD color=((alpha+2)/4)<<24;
        for(unsigned shift:{0u,8u,16u}){
            unsigned sum=0;
            for(DWORD sample:samples)sum+=((sample>>shift)&255)*(alpha?sample>>24:1);
            color|=(sum/(alpha?alpha:4))<<shift;
        }
        pixels[y*(size/2)+x]=color;
    }
    pixels.resize(size*size/4);
}
}
