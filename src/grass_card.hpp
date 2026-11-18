#pragma once
#include "common.hpp"
#include <array>

namespace grass_card {
inline constexpr unsigned Size=256;
inline constexpr size_t Bytes=(Size*Size*4-1)/3*4;

// One painted, folded leaf. The simple mesh supplies each leaf's outline and
// bend instead of flattening a whole tuft into one transparent billboard.
inline std::vector<DWORD> paint(){
    std::vector<DWORD> pixels(Size*Size);
    for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x){
        const float u=(x+.5f)/Size,v=1-(y+.5f)/Size;
        const float tip=std::sqrt(v);
        const float fold=1/(1+std::exp((u-.46f)*90));
        const float rib=std::exp(-1800*(u-.46f)*(u-.46f))*(.3f+.7f*tip);
        const float brush=1+.025f*std::sin(v*35+u*13);
        const std::array<float,3> rgb={
            (std::lerp(.055f+.23f*tip,.13f+.44f*tip,fold)+.055f*rib)*brush,
            (std::lerp(.20f+.34f*tip,.30f+.50f*tip,fold)+.045f*rib)*brush,
            std::lerp(.012f+.025f*tip,.025f+.055f*tip,fold),
        };
        auto channel=[](float value){return DWORD(std::clamp(value,0.f,1.f)*255+.5f);};
        pixels[y*Size+x]=D3DCOLOR_ARGB(255,channel(rgb[0]),channel(rgb[1]),channel(rgb[2]));
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
