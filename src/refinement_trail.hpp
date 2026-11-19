#pragma once
#include "draw_call.hpp"
#include "native_refinement.hpp"
#include "state_guard.hpp"
#include "trail_geometry.hpp"
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
inline float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline bool finite(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline Vec transform(Vec p,const D3DMATRIX &m){return{p.x*m._11+p.y*m._21+p.z*m._31+m._41,
    p.x*m._12+p.y*m._22+p.z*m._32+m._42,p.x*m._13+p.y*m._23+p.z*m._33+m._43};}
struct Puff { Vec position;float radius=.1f;std::array<float,4> color{}; };
struct Vertex { float x,y,z;DWORD color;float u,v; };
inline constexpr size_t MotionSamples=40;

// World-space emitter samples survive native particle births, without keeping
// native vertex data or making the glow follow the player with a time delay.
class MotionPath {
    std::array<Vec,MotionSamples> positions_{};
    std::array<ULONGLONG,MotionSamples> times_{};
    size_t size_=0;
public:
    std::span<const Vec> observe(Vec origin,ULONGLONG time) {
        if(!finite(origin)){size_=0;return {};}
        if(size_&&(time<times_[0]||time-times_[0]>160||length(origin-positions_[0])>8.f))
            size_=0;
        const bool replace=size_&&(time==times_[0]||
            (size_>1&&time-times_[1]<18&&length(origin-positions_[1])<.3f));
        if(!replace) {
            size_=std::min(size_+1,positions_.size());
            std::move_backward(positions_.begin(),positions_.begin()+size_-1,positions_.begin()+size_);
            std::move_backward(times_.begin(),times_.begin()+size_-1,times_.begin()+size_);
        }
        positions_[0]=origin;
        times_[0]=time;
        while(size_>1&&time-times_[size_-1]>640)--size_;
        return {positions_.data(),size_};
    }
};

// Join recorded motion to the recent native particles using their own offset
// from the model. Keep tracing through an emission instead of replacing a
// recorded turn with the chord between the two newest particles.
inline size_t guideHead(std::span<const Puff> puffs,std::span<const Vec> motion,float maxGap,
                       std::array<Puff,MotionSamples> &head,size_t &anchor) {
    if(puffs.size()<2||motion.size()<2||motion.size()>MotionSamples)return 0;
    const auto &newest=puffs.front();
    const float spacing=length(newest.position-puffs[1].position);
    std::array<float,MotionSamples> distances{};
    for(size_t i=1;i<motion.size();++i)distances[i]=distances[i-1]+length(motion[i]-motion[i-1]);
    if(distances[motion.size()-1]<.01f)return 0;
    struct Match {Vec position{};float distance=0,error=INFINITY;};
    auto match=[&](Vec target,float after) {
        Match closest;
        for(size_t i=1;i<motion.size();++i) {
            const float span=distances[i]-distances[i-1];
            if(span<.0001f||distances[i]<after)continue;
            const auto delta=motion[i]-motion[i-1];
            const float t=std::clamp(dot(target-motion[i-1],delta)/(span*span),
                                     std::max(0.f,(after-distances[i-1])/span),1.f);
            const Vec position=motion[i-1]+delta*t,offset=target-position;
            const float error=dot(offset,offset);
            if(error<closest.error)closest={position,distances[i-1]+span*t,error};
        }
        return closest;
    };
    const auto newestMatch=match(newest.position,0);
    const float tolerance=std::max(.50f,newest.radius*1.5f);
    const float reach=std::min(maxGap,std::max(newest.radius*2,spacing*2.5f));
    if(newestMatch.error>tolerance*tolerance||newestMatch.distance>reach)return 0;

    struct Fit {float distance;Vec offset;size_t puff;};
    const auto offset=newest.position-newestMatch.position;
    std::array<Fit,5> fits{};
    size_t fitted=1;
    fits[0]={newestMatch.distance,offset,0};
    for(size_t i=1;i<puffs.size()&&i<fits.size();++i) {
        const auto previous=match(puffs[i].position-offset,fits[fitted-1].distance);
        const float separation=length(puffs[i].position-puffs[i-1].position);
        const float extent=previous.distance-fits[fitted-1].distance;
        if(previous.error>tolerance*tolerance*.49f||extent<separation*.45f||
           extent>std::max(tolerance,separation*2.25f))break;
        if(extent<.01f)continue;
        fits[fitted++]={previous.distance,puffs[i].position-previous.position,i};
    }
    const auto &oldest=fits[fitted-1];
    if(oldest.distance<.01f)return 0;
    anchor=oldest.puff;
    const float fall=fitted>1?
        (newest.color[3]-puffs[fits[1].puff].color[3])/(fits[1].distance-newestMatch.distance):
        (newest.color[3]-puffs[1].color[3])/std::max(spacing,.001f);
    size_t count=0,interval=0;
    for(size_t i=0;i<motion.size()&&distances[i]<oldest.distance-.001f;++i) {
        const float distance=distances[i];
        if(i&&oldest.distance-distance<newest.radius*.55f)break;
        while(interval+1<fitted&&distance>fits[interval+1].distance)++interval;
        const auto &a=fits[interval],&b=fits[std::min(interval+1,fitted-1)];
        const float blend=a.distance<b.distance?
            std::clamp((distance-a.distance)/(b.distance-a.distance),0.f,1.f):0.f;
        const auto &p0=puffs[a.puff],&p1=puffs[b.puff];
        Puff p=p0;
        p.position=motion[i]+a.offset+(b.offset-a.offset)*blend;
        if(count&&length(p.position-head[count-1].position)<newest.radius*.65f)continue;
        p.radius=std::lerp(p0.radius,p1.radius,blend);
        for(size_t channel=0;channel<p.color.size();++channel)
            p.color[channel]=std::lerp(p0.color[channel],p1.color[channel],blend);
        p.color[3]=std::clamp(p.color[3]+fall*std::max(newestMatch.distance-distance,0.f),0.f,1.f);
        head[count++]=p;
    }
    return count;
}
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
                 const Vec *emitter=nullptr,bool perspective=false,float grassLift=0,
                 std::span<const Vec> motion={}){
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
    std::array<Node,256+MotionSamples+1> nodes{};
    auto node=[&](const Puff &p,float separation){
        const float alpha=std::clamp(p.color[3],0.f,1.f),age=1-alpha;
        const float drift=std::clamp((separation-.25f)/.9f,0.f,1.f);
        const float rise=(age*.08f+grassLift)*drift*drift*(3-2*drift);
        return Node{transform(p.position+Vec{0,rise,0},view),
            p.radius*(1.3f+age*.9f),p.color};
    };
    auto unit=[](Vec v){float n=length(v);return n>.00001f?v*(1/n):Vec{0,1,0};};
    auto emit=[&](unsigned count){
        std::array<trail_geometry::Knot,trail_geometry::MaxKnots> knots{};
        // A heavily folded trail uses at most three quads per span.
        const unsigned reduced=std::min(count,170u);
        for(unsigned i=0;i<reduced;++i){
            const auto &n=nodes[i*(count-1)/std::max(1u,reduced-1)];
            knots[i]={{n.p.x,n.p.y,n.p.z},n.width,packed(n.color,.95f)};
        }
        trail_geometry::ribbon({knots.data(),reduced},perspective,
            [&](auto a,auto b,auto c,auto d){
                if(out.size()+6>MaxVertices)return;
                for(const auto &v:{a,b,c,c,b,d})out.push_back({v.p.x,v.p.y,v.p.z,v.tint,v.u,v.cap});
            });
    };
    for(size_t first=0;first<puffs.size();){
        size_t end=first+1;
        while(end<puffs.size()&&length(puffs[end].position-puffs[end-1].position)<=maxGap)++end;
        const auto &newest=puffs[first];
        // A resting effect is a soft glow centered on its newest particle.
        // Connecting small idle offsets would leave one long cap and a flat edge.
        if(std::all_of(puffs.begin()+first+1,puffs.begin()+end,[&](const Puff &p){
            return length(p.position-newest.position)<newest.radius*.5f;
        })){
            nodes[0]=node(newest,0);
            const size_t firstVertex=out.size();
            emit(1);
            // Stretch only the upper falloff, leaving the bright center on the armor.
            const float centerY=nodes[0].p.y;
            for(size_t i=firstVertex;i<out.size();++i){
                auto &v=out[i];
                if(v.y>centerY)v.y=centerY+(v.y-centerY)*1.18f;
            }
            first=end;
            continue;
        }
        unsigned count=0;
        std::array<Puff,MotionSamples> head{};
        size_t anchor=0;
        const size_t guided=first?0:guideHead({puffs.data()+first,end-first},motion,maxGap,head,anchor);
        const Vec liftOrigin=guided?head[0].position:newest.position;
        auto append=[&](const Puff &p){
            const Vec offset=p.position-liftOrigin;
            const auto n=node(p,std::sqrt(offset.x*offset.x+offset.z*offset.z));
            if(!count||length(n.p-nodes[count-1].p)>=.001f)nodes[count++]=n;
        };
        for(size_t i=0;i<guided;++i)append(head[i]);
        // Use the model origin only when the newest visible particles confirm
        // that it extends their path. A resting emitter can have a local offset.
        if(!guided&&!first&&emitter&&end-first>1&&finite(*emitter)){
            const auto &previous=puffs[first+1];
            const Vec tangent=newest.position-previous.position,offset=*emitter-newest.position;
            const float spacing=length(tangent),distance=length(offset);
            if(spacing>.001f&&distance>.001f){
                const float along=dot(offset,tangent);
                const float reach=std::min(maxGap,std::max(newest.radius*2,spacing*2.5f));
                auto ease=[](float value) {
                    const float t=std::clamp(value,0.f,1.f);
                    return t*t*(3-2*t);
                };
                const float direction=ease((along/(spacing*distance)-.42f)/.28f);
                const float range=ease((reach-distance)/(reach*.24f));
                const float movement=ease((spacing/newest.radius-.27f)/.23f);
                const float progress=along/(spacing*spacing)*direction*range*movement;
                if(progress*spacing>.001f) {
                    Puff extrapolated=newest;
                    extrapolated.position=newest.position+tangent*progress;
                    extrapolated.color[3]=std::clamp(newest.color[3]+(newest.color[3]-previous.color[3])*progress,0.f,1.f);
                    nodes[count++]=node(extrapolated,0);
                }
            }
        }
        for(size_t i=first+anchor;i<end&&count<nodes.size()-1;++i)append(puffs[i]);
        const unsigned last=count-1;
        float tail=nodes[last].width*.85f;
        if(last>0){
            const float fall=nodes[last-1].color[3]-nodes[last].color[3];
            if(fall>1.f/255)tail=std::clamp(length(nodes[last].p-nodes[last-1].p)*nodes[last].color[3]/fall,
                nodes[last].width*.35f,maxGap);
        }
        if(last>0){
            Node endpoint=nodes[last];
            endpoint.p=endpoint.p+unit(endpoint.p-nodes[last-1].p)*tail;
            endpoint.color[3]=0;
            nodes[count++]=endpoint;
        }
        emit(count);first=end;
    }
}
}
