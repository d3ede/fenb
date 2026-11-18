#pragma once
#include "draw_call.hpp"
#include "native_refinement.hpp"
#include "state_guard.hpp"
#include <array>

namespace refinement_trail {
struct Vec {
    float x=0,y=0,z=0;
    Vec operator+(Vec b)const{return{x+b.x,y+b.y,z+b.z};}
    Vec operator-(Vec b)const{return{x-b.x,y-b.y,z-b.z};}
    Vec operator*(float b)const{return{x*b,y*b,z*b};}
};
inline float length(Vec a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
inline bool finite(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline Vec transform(Vec p,const D3DMATRIX &m){return{p.x*m._11+p.y*m._21+p.z*m._31+m._41,
    p.x*m._12+p.y*m._22+p.z*m._32+m._42,p.x*m._13+p.y*m._23+p.z*m._33+m._43};}
struct Puff { Vec position;float radius=.1f;std::array<float,4> color{}; };
struct Vertex { float x,y,z;DWORD color;float u,v; };
inline DWORD packed(const std::array<float,4>&color,float gain=1){
    auto c=[&](int i){return DWORD(std::clamp(color[i]*(i==3?gain:1),0.f,1.f)*255+.5f);};
    return(c(3)<<24)|(c(0)<<16)|(c(1)<<8)|c(2);
}
// Copy only complete, conventional UP quads. No GPU buffer readback, WRITEONLY
// lock, retained native pointer or cross-model nearest-neighbor association.
inline bool extract(const ParticleDraw &draw,DWORD fvf,const D3DMATRIX &world,
                    const std::array<float,4>&tint,std::vector<Puff>&out) {
    if(!draw.data || (fvf!=0x102&&fvf!=0x142) || draw.stride!=(fvf==0x142?24u:20u) ||
       !draw.primitives || draw.primitives>512 || std::abs(world._14)+std::abs(world._24)+
       std::abs(world._34)+std::abs(world._44-1)>.00001f)return false;
    const bool list=draw.type==D3DPT_TRIANGLELIST;
    if((!list&&draw.type!=D3DPT_TRIANGLESTRIP&&draw.type!=D3DPT_TRIANGLEFAN)||
       (list?draw.primitives%2!=0:draw.primitives!=2))return false;
    if(draw.indices && ((draw.indexFormat!=D3DFMT_INDEX16&&draw.indexFormat!=D3DFMT_INDEX32)||
       !draw.vertices||draw.vertices>4096||draw.minVertex>4096-draw.vertices))return false;
    const size_t before=out.size();
    const unsigned quads=list?draw.primitives/2:1,per=list?6:4;
    for(unsigned q=0;q<quads;++q){
        std::array<Vec,4> positions{};std::array<std::array<float,4>,4> colors{};
        unsigned mask=0;
        for(unsigned j=0;j<per;++j){
            UINT index=q*per+j;
            if(draw.indices){
                if(draw.indexFormat==D3DFMT_INDEX16){WORD value;std::memcpy(&value,(const BYTE*)draw.indices+index*2,2);index=value;}
                else std::memcpy(&index,(const BYTE*)draw.indices+index*4,4);
                if(index<draw.minVertex||index-draw.minVertex>=draw.vertices){out.resize(before);return false;}
            }
            const BYTE *data=(const BYTE*)draw.data+size_t(index)*draw.stride;
            Vec position;float uv[2];DWORD color=0xffffffff;
            std::memcpy(&position,data,12);std::memcpy(uv,data+(fvf==0x142?16:12),8);
            if(fvf==0x142)std::memcpy(&color,data+12,4);
            if(!finite(position)||!std::isfinite(uv[0])||!std::isfinite(uv[1])||
               std::abs(uv[0]-std::round(uv[0]))>.0001f||std::abs(uv[1]-std::round(uv[1]))>.0001f||
               uv[0]<0||uv[0]>1||uv[1]<0||uv[1]>1){out.resize(before);return false;}
            unsigned corner=unsigned(uv[0])|unsigned(uv[1])*2;
            auto p=transform(position,world);
            if(!finite(p)||(mask&(1u<<corner)&&length(p-positions[corner])>.0001f)){out.resize(before);return false;}
            positions[corner]=p;mask|=1u<<corner;
            colors[corner]={float((color>>16)&255)/255,float((color>>8)&255)/255,float(color&255)/255,float(color>>24)/255};
        }
        if(mask!=15){out.resize(before);return false;}
        Puff puff;
        for(unsigned j=0;j<4;++j){puff.position=puff.position+positions[j]*.25f;for(int c=0;c<4;++c)puff.color[c]+=colors[j][c]*tint[c]*.25f;}
        float width=length(positions[1]-positions[0]),height=length(positions[2]-positions[0]);
        if(width<.001f||height<.001f||width>2||height>2){out.resize(before);return false;}
        puff.radius=std::clamp(std::max(width*.8f,height*.5f),.16f,.6f);
        out.push_back(puff);
    }
    return true;
}
// Merge paired emitters at the same fade stage, then resample their own path.
// Absolute world positions are rebuilt from current native particles each render.
inline void merge(std::vector<Puff>&puffs){
    std::stable_sort(puffs.begin(),puffs.end(),[](const Puff&a,const Puff&b){return a.color[3]>b.color[3];});
    const float tolerance=(puffs.empty()?0:puffs.front().color[3])*.045f+.0000001f;
    for(size_t i=0;i<puffs.size();++i)for(size_t j=i+1;j<puffs.size();){
        auto &a=puffs[i];auto &b=puffs[j];const float distance=length(a.position-b.position);
        if(std::abs(a.color[3]-b.color[3])<tolerance&&distance<std::max(a.radius,b.radius)*1.8f){
            a.position=(a.position+b.position)*.5f;a.radius=std::max(a.radius,b.radius)+distance*.2f;
            for(int c=0;c<4;++c)a.color[c]=(a.color[c]+b.color[c])*.5f;
            puffs.erase(puffs.begin()+j);
        }else ++j;
    }
}
inline void mesh(std::vector<Puff>&puffs,const D3DMATRIX &view,std::vector<Vertex>&out){
    out.clear();merge(puffs);
    auto append=[&](Puff puff,float gain){
        if(out.size()+6>12288)return;
        const float alpha=std::clamp(puff.color[3],0.f,1.f),age=1-alpha;
        puff.position.y+=age*.08f;
        Vec p=transform(puff.position,view);
        const float r=puff.radius*(1.3f+age*.9f);
        auto color=packed(puff.color,gain*std::pow(alpha,.6f));
        Vertex quad[]={{p.x-r,p.y+r,p.z,color,0,0},{p.x+r,p.y+r,p.z,color,1,0},
                       {p.x-r,p.y-r,p.z,color,0,1},{p.x+r,p.y-r,p.z,color,1,1}};
        for(int i:{0,1,2,2,1,3})out.push_back(quad[i]);
    };
    for(size_t i=0;i<puffs.size();++i){
        const auto &a=puffs[i];size_t next=puffs.size();float distance=std::min(1.25f,a.radius*7);
        for(size_t j=i+1;j<puffs.size();++j){float d=length(a.position-puffs[j].position);
            if(d>.005f&&d<distance){distance=d;next=j;}}
        if(next==puffs.size()){append(a,.75f);continue;}
        const unsigned allowance=unsigned((2048-out.size()/6)/(puffs.size()-i));
        const auto &b=puffs[next];unsigned steps=std::clamp(unsigned(std::ceil(distance/std::max(.035f,a.radius*.4f))),1u,std::min(16u,std::max(1u,allowance)));
        for(unsigned step=0;step<steps;++step){
            float t=float(step)/steps;Puff p;
            p.position=a.position*(1-t)+b.position*t;p.radius=a.radius*(1-t)+b.radius*t;
            for(int c=0;c<4;++c)p.color[c]=a.color[c]*(1-t)+b.color[c]*t;
            append(p,.30f);
        }
    }
}
}
