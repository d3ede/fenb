#pragma once
#include "draw_call.hpp"
#include "native_refinement.hpp"
#include "state_guard.hpp"
#include <array>

namespace refinement_trail {
inline constexpr unsigned MaxSprites=512,MaxVertices=MaxSprites*6;
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
        puff.radius=std::clamp(std::max(width*1.4f,height*.65f),.20f,.75f);
        out.push_back(puff);
    }
    return true;
}
// Merge paired emitters at the same fade stage, then resample their own path.
// Absolute world positions are rebuilt from current native particles each render.
inline void merge(std::vector<Puff>&puffs){
    std::stable_sort(puffs.begin(),puffs.end(),[](const Puff&a,const Puff&b){return a.color[3]>b.color[3];});
    const float tolerance=.5f/255.f;
    for(size_t i=0;i<puffs.size();++i)for(size_t j=i+1;j<puffs.size();){
        auto &a=puffs[i];auto &b=puffs[j];const float distance=length(a.position-b.position);
        if(std::abs(a.color[3]-b.color[3])<tolerance&&distance<std::max(a.radius,b.radius)*1.8f){
            a.position=(a.position+b.position)*.5f;a.radius=std::max(a.radius,b.radius)+distance*.2f;
            for(int c=0;c<4;++c)a.color[c]=(a.color[c]+b.color[c])*.5f;
            puffs.erase(puffs.begin()+j);
        }else ++j;
    }
}
// A joined camera-facing surface has one coverage layer regardless of native
// emission spacing. New particles subdivide it instead of adding bright blobs.
inline void mesh(std::vector<Puff>&puffs,const D3DMATRIX &view,std::vector<Vertex>&out,
                 const Vec *emitter=nullptr){
    out.clear();merge(puffs);
    if(puffs.empty())return;
    // A regular fast trail can have arbitrarily wider spacing than a walk.
    // Scale the continuity guard from its own typical spacing, while rejecting
    // isolated teleports rather than stretching a cloud across the whole map.
    std::array<float,256> gaps{};size_t gapCount=0;
    for(size_t i=1;i<puffs.size()&&gapCount<gaps.size();++i){
        const float distance=length(puffs[i].position-puffs[i-1].position);
        if(distance>.001f)gaps[gapCount++]=distance;
    }
    float maxGap=12.f;
    if(gapCount>=2){
        std::sort(gaps.begin(),gaps.begin()+gapCount);
        maxGap=std::clamp(gaps[(gapCount-1)/4]*4,12.f,128.f);
    }
    struct Node {Vec p;float width;std::array<float,4> color;};
    std::array<Node,262> nodes{};
    auto node=[&](const Puff &p){
        const float alpha=std::clamp(p.color[3],0.f,1.f),age=1-alpha;
        return Node{transform(p.position+Vec{0,age*.08f,0},view),
            p.radius*(1.3f+age*.9f),p.color};
    };
    auto unit=[](Vec v){float n=length(v);return n>.00001f?v*(1/n):Vec{0,1,0};};
    auto cap=[&](Node n,Vec direction,float distance,float width,float opacity){
        n.p=n.p+direction*distance;n.width*=width;n.color[3]*=opacity;return n;
    };
    auto emit=[&](unsigned count){
        std::array<Vec,262> side{};
        for(unsigned i=0;i<count;++i){
            Vec tangent=nodes[std::min(i+1,count-1)].p-nodes[i?i-1:0].p;
            float xy=std::hypot(tangent.x,tangent.y);
            side[i]=xy>.00001f?Vec{tangent.y/xy,-tangent.x/xy,0}:Vec{1,0,0};
        }
        for(unsigned i=1;i<count&&out.size()+6<=MaxVertices;++i){
            auto edge=[&](unsigned j,float u){const auto &n=nodes[j];
                Vec p=n.p+side[j]*(n.width*(u*2-1));
                return Vertex{p.x,p.y,p.z,packed(n.color,.95f),u,0};};
            Vertex a=edge(i-1,0),b=edge(i-1,1),c=edge(i,0),d=edge(i,1);
            for(const auto &v:{a,b,c,c,b,d})out.push_back(v);
        }
    };
    for(size_t first=0;first<puffs.size();){
        size_t end=first+1;
        while(end<puffs.size()&&length(puffs[end].position-puffs[end-1].position)<=maxGap)++end;
        unsigned count=2;
        // The current native emitter fills the unsampled part of a fast run.
        // Linear fade extrapolation keeps the head's energy steady across births.
        if(!first&&emitter&&finite(*emitter)){
            const auto &a=puffs[first];float distance=length(*emitter-a.position);
            float spacing=end-first>1?length(puffs[first+1].position-a.position):a.radius*2;
            if(distance>.001f&&distance<=std::min(maxGap,std::max(a.radius*2,spacing*2.5f))){
                Puff head=a;head.position=*emitter;
                if(end-first>1&&spacing>.001f)
                    head.color[3]=std::clamp(a.color[3]+(a.color[3]-puffs[first+1].color[3])*distance/spacing,0.f,1.f);
                nodes[count++]=node(head);
            }
        }
        for(size_t i=first;i<end&&count<259;++i){
            Node n=node(puffs[i]);
            if(count>2&&length(n.p-nodes[count-1].p)<.001f)continue;
            nodes[count++]=n;
        }
        const unsigned last=count-1;
        Vec forward=count>3?unit(nodes[3].p-nodes[2].p):Vec{0,-1,0};
        Vec backward=count>3?unit(nodes[last].p-nodes[last-1].p):forward;
        nodes[0]=cap(nodes[2],forward,-nodes[2].width*1.1f,1,0);
        nodes[1]=cap(nodes[2],forward,-nodes[2].width*.55f,1,.5f);
        float tail=nodes[last].width*.85f;
        if(last>2){
            const float fall=nodes[last-1].color[3]-nodes[last].color[3];
            if(fall>1.f/255)tail=std::clamp(length(nodes[last].p-nodes[last-1].p)*nodes[last].color[3]/fall,
                nodes[last].width*.35f,maxGap);
        }
        nodes[count++]=cap(nodes[last],backward,tail*.5f,1,.5f);
        nodes[count++]=cap(nodes[last],backward,tail,1,0);
        emit(count);first=end;
    }
}
}
