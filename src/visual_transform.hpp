#pragma once
#include <array>
#include <algorithm>
#include <cmath>

namespace visual_transform {
struct Vec { float x=0,y=0,z=0; };
inline Vec operator+(Vec a,Vec b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec operator-(Vec a,Vec b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec operator*(Vec a,float b) { return {a.x*b,a.y*b,a.z*b}; }
inline float dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec cross(Vec a,Vec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Vec lerp(Vec a,Vec b,float t) { return a+(b-a)*t; }
struct Quat { float x=0,y=0,z=0,w=1; };
inline Quat normalize(Quat q) {
    float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    return {q.x/n,q.y/n,q.z/n,q.w/n};
}
inline Quat slerp(Quat a,Quat b,float t) {
    float d=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    if(d<0) { b={-b.x,-b.y,-b.z,-b.w}; d=-d; }
    float u=1-t,v=t;
    if(d<.9995f) {
        float angle=std::acos(std::clamp(d,-1.f,1.f)),den=std::sin(angle);
        u=std::sin((1-t)*angle)/den; v=std::sin(t*angle)/den;
    }
    return normalize({a.x*u+b.x*v,a.y*u+b.y*v,a.z*u+b.z*v,a.w*u+b.w*v});
}
struct Transform { Vec position,scale; Quat rotation; };
inline bool decompose(const float *m,Transform &out) {
    for(int i=0;i<16;++i) if(!std::isfinite(m[i])) return false;
    if(std::fabs(m[3])+std::fabs(m[7])+std::fabs(m[11])+std::fabs(m[15]-1)>1e-4f) return false;
    Vec rows[3]={{m[0],m[1],m[2]},{m[4],m[5],m[6]},{m[8],m[9],m[10]}};
    float lengths[3];
    for(int i=0;i<3;++i) { lengths[i]=std::sqrt(dot(rows[i],rows[i])); if(lengths[i]<1e-6f) return false; rows[i]=rows[i]*(1/lengths[i]); }
    // Reflected and sheared passes retain their original transform.
    if(dot(cross(rows[0],rows[1]),rows[2])<.99f ||
       std::fabs(dot(rows[0],rows[1]))+std::fabs(dot(rows[0],rows[2]))+std::fabs(dot(rows[1],rows[2]))>.01f) return false;
    float r[3][3]; for(int i=0;i<3;++i) for(int j=0;j<3;++j) r[i][j]=m[j*4+i]/lengths[j];
    Quat q; float trace=r[0][0]+r[1][1]+r[2][2];
    if(trace>0) { float s=std::sqrt(trace+1)*2; q={ (r[2][1]-r[1][2])/s,(r[0][2]-r[2][0])/s,(r[1][0]-r[0][1])/s,.25f*s}; }
    else if(r[0][0]>r[1][1] && r[0][0]>r[2][2]) { float s=std::sqrt(1+r[0][0]-r[1][1]-r[2][2])*2; q={.25f*s,(r[0][1]+r[1][0])/s,(r[0][2]+r[2][0])/s,(r[2][1]-r[1][2])/s}; }
    else if(r[1][1]>r[2][2]) { float s=std::sqrt(1+r[1][1]-r[0][0]-r[2][2])*2; q={(r[0][1]+r[1][0])/s,.25f*s,(r[1][2]+r[2][1])/s,(r[0][2]-r[2][0])/s}; }
    else { float s=std::sqrt(1+r[2][2]-r[0][0]-r[1][1])*2; q={(r[0][2]+r[2][0])/s,(r[1][2]+r[2][1])/s,.25f*s,(r[1][0]-r[0][1])/s}; }
    out={{m[12],m[13],m[14]},{lengths[0],lengths[1],lengths[2]},normalize(q)}; return true;
}
inline std::array<float,16> compose(const Transform &t) {
    auto q=t.rotation; float x=q.x,y=q.y,z=q.z,w=q.w;
    return {(1-2*(y*y+z*z))*t.scale.x,2*(x*y+z*w)*t.scale.x,2*(x*z-y*w)*t.scale.x,0,
            2*(x*y-z*w)*t.scale.y,(1-2*(x*x+z*z))*t.scale.y,2*(y*z+x*w)*t.scale.y,0,
            2*(x*z+y*w)*t.scale.z,2*(y*z-x*w)*t.scale.z,(1-2*(x*x+y*y))*t.scale.z,0,
            t.position.x,t.position.y,t.position.z,1};
}
inline bool interpolate(const float *a,const float *b,float alpha,float *out) {
    Transform previous,current;
    if(!decompose(a,previous) || !decompose(b,current)) return false;
    auto delta=current.position-previous.position;
    if(dot(delta,delta)>=100.f) return false;
    auto result=compose({lerp(previous.position,current.position,alpha),lerp(previous.scale,current.scale,alpha),slerp(previous.rotation,current.rotation,alpha)});
    std::copy(result.begin(),result.end(),out); return true;
}
}
