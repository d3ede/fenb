#pragma once
#include "common.hpp"
#include <array>

namespace grass_card {
inline constexpr unsigned Size=256;
inline constexpr size_t Bytes=(Size*Size*4-1)/3*4;

// The art is defined in card coordinates. The four leaves share a small
// root area, with independent silhouettes, heights and painted highlights.
inline std::vector<DWORD> paint(){
    struct Leaf {float root,tip,height,width,bend,tint;};
    constexpr Leaf leaves[]={
        {.53f,.78f,.76f,.104f,-.020f,.09f},
        {.48f,.17f,.85f,.096f,.074f,.02f},
        {.49f,.72f,.98f,.101f,-.075f,.15f},
        {.51f,.38f,.68f,.087f,.041f,-.03f},
    };
    std::vector<DWORD> pixels(Size*Size);
    for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x){
        const float u=(x+.5f)/Size,v=1-(y+.5f)/Size;
        float alpha=0,nearest=2;
        std::array<float,3> rgb{},edge{};
        for(const auto &leaf:leaves){
            const float t=std::min(v/leaf.height,1.f);
            const float center=std::lerp(leaf.root,leaf.tip,t*t*.78f+t*.22f)+leaf.bend*4*t*(1-t);
            const float half=leaf.width*.5f*(.57f+.98f*std::sin(t*3.14159265f))*(1-t);
            const float distance=std::max(std::abs(u-center)-half,v-leaf.height);
            const float left=std::max(u-.5f/Size,center-half);
            const float right=std::min(u+.5f/Size,center+half);
            const float coverage=std::max(0.f,(right-left)*Size)*std::clamp((leaf.height-v)*Size+.5f,0.f,1.f);
            const float across=std::clamp((u-center)/std::max(half,1.f/Size),-1.f,1.f);
            const float rim=std::exp(-13*(across+.48f)*(across+.48f));
            const float shade=.80f+.16f*(1-across)*.5f+rim*.21f;
            const float tip=t*t*(3-2*t);
            const std::array<float,3> color={
                (.11f+.50f*tip+leaf.tint*t)*shade,
                (.36f+.42f*tip+leaf.tint*.43f)*shade,
                (.078f+.14f*tip)*shade,
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
