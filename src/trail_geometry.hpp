#pragma once
#include "common.hpp"
#include <array>
#include <cmath>
#include <span>

namespace trail_geometry {
struct Vec {
    float x=0,y=0,z=0;
    Vec operator+(Vec b)const{return{x+b.x,y+b.y,z+b.z};}
    Vec operator-(Vec b)const{return{x-b.x,y-b.y,z-b.z};}
    Vec operator*(float factor)const{return{x*factor,y*factor,z*factor};}
};
inline Vec transform(Vec p,const D3DMATRIX &m) {
    return {p.x*m._11+p.y*m._21+p.z*m._31+m._41,
            p.x*m._12+p.y*m._22+p.z*m._32+m._42,
            p.x*m._13+p.y*m._23+p.z*m._33+m._43};
}
inline float cross(Vec a,Vec b){return a.x*b.y-a.y*b.x;}
inline float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y;}
inline float length(Vec v){return std::sqrt(dot(v,v));}
inline Vec unit(Vec v){float n=length(v);return n>.000001f?v*(1/n):Vec{};}
inline Vec across(Vec direction){return{direction.y,-direction.x,0};}
struct Knot {Vec p;float width;DWORD tint;float v=0;};
struct Vertex {Vec p;DWORD tint;float u,v,cap;};
inline constexpr unsigned MaxKnots=264;

// Work in the projected plane for winding and joins. If two neighboring
// cross-sections cannot form a convex quad, use a local span with feathered
// joints. All vertices of a cap use its endpoint's depth.
template<class Quad>
void ribbon(std::span<const Knot> input,bool perspective,Quad emit,bool roundCorners=true) {
    if(input.empty()||input.size()>MaxKnots)return;
    if(perspective) {
        auto visible=[](const Knot &k){return std::isfinite(k.p.z)&&k.p.z>=.025f;};
        if(!std::all_of(input.begin(),input.end(),visible)) {
            // Adjacent samples on opposite sides of the camera are separate
            // runs. Never bridge across an invisible span behind the viewer.
            while(!input.empty()) {
                if(!visible(input.front())) { input=input.subspan(1); continue; }
                const auto end=std::find_if_not(input.begin(),input.end(),visible);
                const auto count=size_t(end-input.begin());
                ribbon(input.first(count),perspective,emit,roundCorners);
                input=input.subspan(count);
            }
            return;
        }
    }
    struct Section { Knot knot;Vec position,side;float depth,width; };
    std::array<Section,MaxKnots> sections{};
    std::array<Vec,MaxKnots> axes{};
    std::array<bool,MaxKnots> joined{};
    size_t count=0;
    for(const auto &knot:input) {
        const float depth=perspective?knot.p.z:1.f;
        if(!std::isfinite(depth)||depth<.025f)continue;
        const Vec position{knot.p.x/depth,knot.p.y/depth,0};
        const float width=knot.width/depth;
        if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(width))continue;
        if(count&&length(position-sections[count-1].position)<.02f*std::min(width,sections[count-1].width))continue;
        sections[count++]={knot,position,{},depth,width};
    }
    if(!count)return;
    if(roundCorners) {
        for(unsigned pass=0;pass<2;++pass) {
            auto previous=sections[0].position;
            for(size_t i=1;i+1<count;++i) {
                auto &section=sections[i];
                const auto center=section.position;
                const Vec incoming=center-previous,outgoing=sections[i+1].position-center;
                const float curve=std::clamp((.92f-dot(unit(incoming),unit(outgoing)))*.65f,0.f,.4f);
                section.position=center+(unit(outgoing)-unit(incoming))*
                    (curve*std::min(length(incoming),length(outgoing)));
                previous=center;
            }
        }
    }
    for(size_t i=1;i<count;++i)axes[i]=unit(sections[i].position-sections[i-1].position);
    if(count==1)axes[1]={0,-1,0};
    for(size_t i=0;i<count;++i) {
        const auto before=axes[std::max<size_t>(1,i)];
        const auto after=axes[std::min(i+1,std::max<size_t>(1,count-1))];
        auto direction=unit(before+after);
        if(length(direction)==0)direction=length(after)>0?after:Vec{0,-1,0};
        sections[i].side=across(direction);
    }
    auto edges=[](const Section &s,Vec side,Vec offset={},bool cap=false) {
        const Vec p=s.position+offset,span=side*s.width;
        const Vec a=p-span,b=p+span;
        const DWORD tint=cap?(s.knot.tint&0xffffff):s.knot.tint;
        return std::array<Vertex,2>{
            Vertex{{a.x*s.depth,a.y*s.depth,s.knot.p.z},tint,0,s.knot.v,cap?1.f:0.f},
            Vertex{{b.x*s.depth,b.y*s.depth,s.knot.p.z},tint,1,s.knot.v,cap?1.f:0.f}};
    };
    auto convex=[](Vec a,Vec b,Vec c,Vec d) {
        return cross(b-a,c-b)>0&&cross(c-b,d-c)>0&&cross(d-c,a-d)>0&&cross(a-d,b-a)>0;
    };
    for(size_t i=1;i<count;++i) {
        const auto &a=sections[i-1],&b=sections[i];
        const Vec sideA=a.side*a.width,sideB=b.side*b.width;
        joined[i]=convex(a.position-sideA,a.position+sideA,b.position+sideB,b.position-sideB);
    }
    auto quad=[&](const auto &a,const auto &b){emit(a[0],a[1],b[0],b[1]);};
    auto cap=[&](const Section &s,Vec side,bool start,float reach=1.f) {
        const Vec along{-side.y,side.x,0};
        auto root=edges(s,side);
        auto end=edges(s,side,along*(s.width*reach*(start?-.95f:.95f)),true);
        if(start)quad(end,root);
        else quad(root,end);
    };
    if(count==1) {
        cap(sections[0],sections[0].side,true);
        cap(sections[0],sections[0].side,false);
        return;
    }
    auto softenJoin=[](Section s,bool split) {
        if(split)s.knot.tint=(s.knot.tint&0xffffff)|(((s.knot.tint>>24)+1)/2<<24);
        return s;
    };
    for(size_t i=1;i<count;++i) {
        const bool splitBefore=i>1&&(!joined[i-1]||!joined[i]);
        const bool splitAfter=i+1<count&&(!joined[i+1]||!joined[i]);
        const auto a=softenJoin(sections[i-1],splitBefore),b=softenJoin(sections[i],splitAfter);
        const Vec local=across(axes[i]);
        if(length(local)==0)continue;
        const auto sideA=joined[i]?a.side:local,sideB=joined[i]?b.side:local;
        if(i==1||splitBefore)cap(a,sideA,true,splitBefore?.7f:1.f);
        quad(edges(a,sideA),edges(b,sideB));
        if(i+1==count||splitAfter)cap(b,sideB,false,splitAfter?.7f:1.f);
    }
}
}
