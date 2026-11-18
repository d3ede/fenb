#pragma once
#include <array>

// Editable, original vector artwork for the isolated meadow study. These
// curves are painted into alpha cards; they are not the rejected 3D tuft mesh.
namespace MeadowArtwork {
constexpr unsigned Size=2048,Tiles=2,Leaves=12;
struct Point {float x,y;Point operator+(Point b)const{return {x+b.x,y+b.y};}Point operator-(Point b)const{return {x-b.x,y-b.y};}Point operator*(float s)const{return {x*s,y*s};}};
using RGB=std::array<float,3>;
float smooth(float a,float b,float x){float t=std::clamp((x-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
RGB mix(RGB a,RGB b,float t){for(unsigned i=0;i<3;++i)a[i]=std::lerp(a[i],b[i],t);return a;}
Point bezier(Point a,Point b,Point c,Point d,float t){float s=1-t;return a*(s*s*s)+b*(3*s*s*t)+c*(3*s*t*t)+d*(t*t*t);}
struct Random {unsigned state;float next(){state^=state<<13;state^=state>>17;state^=state<<5;return (state>>8)*(1.f/16777216);}};
DWORD pack(RGB rgb,float alpha){auto byte=[](float f){return unsigned(std::clamp(f,0.f,1.f)*255+.5f);};return D3DCOLOR_ARGB(byte(alpha),byte(rgb[0]),byte(rgb[1]),byte(rgb[2]));}
std::vector<DWORD> paint(bool winter){
    std::vector<DWORD> atlas(Size*Size);constexpr unsigned Tile=Size/Tiles,Steps=96;
    constexpr float heights[]={.23f,.77f,.44f,.89f,.18f,.61f,.83f,.27f,.93f,.52f,.74f,.20f};
    constexpr float leans[]={-.16f,-.18f,.13f,.21f,-.10f,.17f,-.22f,.11f,-.16f,.19f,-.13f,.10f,.14f,-.19f};
    for(unsigned variant=0;variant<4;++variant){Random random{0x67c1a31u+variant*0x1a391u};
        for(unsigned leaf=0;leaf<Leaves;++leaf){
            const unsigned shape=(leaf+variant*3)%Leaves;
            const float root=.11f+leaf*.069f+(random.next()-.5f)*.065f;
            const float height=heights[shape]*(.89f+.11f*random.next());
            float lean=leans[shape]*(1.05f+.40f*random.next());
            lean=std::clamp(lean,.055f-root,.945f-root);
            const bool arch=shape%4==0;
            const Point a{root,.965f},b{root+lean*.08f,.965f-height*.42f},
                c{root+lean*.48f,.965f-height*(arch?1.04f:.88f)},d{root+lean,.965f-height};
            const float radius=(.016f+random.next()*.010f)*Tile*std::clamp(height*1.5f,.55f,1.f);
            std::vector<float> coverage(Tile*Tile),position(Tile*Tile),across(Tile*Tile);
            auto point=[&](float t){return bezier(a,b,c,d,t)*float(Tile);};
            for(unsigned step=0;step<Steps;++step){float t0=float(step)/Steps,t1=float(step+1)/Steps;
                Point p=point(t0),q=point(t1),delta=q-p;float length2=delta.x*delta.x+delta.y*delta.y;
                int x0=std::max(0,int(std::floor(std::min(p.x,q.x)-radius-1))),x1=std::min(int(Tile)-1,int(std::ceil(std::max(p.x,q.x)+radius+1)));
                int y0=std::max(0,int(std::floor(std::min(p.y,q.y)-radius-1))),y1=std::min(int(Tile)-1,int(std::ceil(std::max(p.y,q.y)+radius+1)));
                for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){Point relative=Point{x+.5f,y+.5f}-p;
                    float along=std::clamp((relative.x*delta.x+relative.y*delta.y)/std::max(.0001f,length2),0.f,1.f);
                    float t=std::lerp(t0,t1,along);Point error=relative-delta*along;
                    float width=radius*(.65f+1.8f*t)*std::pow(std::max(0.f,1-t),.80f);
                    float distance=std::sqrt(error.x*error.x+error.y*error.y);
                    float alpha=std::clamp(width+.5f-distance,0.f,1.f);size_t i=size_t(y)*Tile+x;
                    if(alpha<=coverage[i])continue;
                    coverage[i]=alpha;position[i]=t;
                    across[i]=(delta.x*relative.y-delta.y*relative.x)/std::sqrt(std::max(.0001f,length2))/std::max(.3f,width);
                }
            }
            for(unsigned y=0;y<Tile;++y)for(unsigned x=0;x<Tile;++x){size_t i=size_t(y)*Tile+x;float alpha=coverage[i];if(alpha<=0)continue;
                // Vary the rolled leaf face, not the brightness at random.
                float signedFace=(shape+variant)%3==0?-across[i]:across[i];
                float roll=(int(shape%5)-2)*.13f;
                float t=position[i],face=smooth(-.06f+roll,.17f+roll,-signedFace);
                float ridge=std::exp(-130.f*(across[i]+.16f)*(across[i]+.16f));
                const float hue=float((shape+variant)%5)/4;
                RGB dark=mix({.045f,.23f,.012f},{.24f+.05f*hue,.49f+.06f*hue,.025f},std::pow(t,.65f));
                RGB lit=mix({.18f,.45f,.025f},{.65f+.10f*hue,.85f+.09f*hue,.075f},std::pow(t,.65f));
                RGB color=mix(dark,lit,face);
                color=mix(color,{.78f,.94f,.15f},ridge*(.04f+.07f*t));
                if(winter){
                    RGB shadow=mix({.25f,.30f,.18f},{.40f,.48f,.36f},t);
                    RGB light=mix({.43f,.49f,.28f},{.67f,.75f,.52f},t);
                    if(shape%3==0){shadow=mix(shadow,{.36f,.26f,.14f},.40f);light=mix(light,{.64f,.52f,.30f},.35f);}
                    color=mix(shadow,light,face);
                    float frost=smooth(.58f,.98f,t)*(.28f+.55f*face+.17f*ridge);
                    color=mix(color,{.83f,.88f,.88f},frost*.65f);
                }
                size_t destination=(y+(variant/2)*Tile)*Size+x+(variant%2)*Tile;
                DWORD old=atlas[destination];float oldAlpha=(old>>24)/255.f,outAlpha=alpha+oldAlpha*(1-alpha);
                RGB out{};for(unsigned channel=0;channel<3;++channel){float prior=((old>>(16-channel*8))&255)/255.f;out[channel]=(color[channel]*alpha+prior*oldAlpha*(1-alpha))/std::max(.00001f,outAlpha);}
                // The plant emerges through the substrate instead of ending
                // in a hard, opaque horizontal card seam.
                atlas[destination]=pack(out,outAlpha);
            }
        }
    }
    for(unsigned y=0;y<Size;++y){float localY=float(y%(Size/2))/(Size/2);
        float rootFade=smooth(.965f,.925f,localY);
        for(unsigned x=0;x<Size;++x){DWORD &pixel=atlas[y*Size+x];pixel=(pixel&0xffffff)|(unsigned((pixel>>24)*rootFade+.5f)<<24);}}
    return atlas;
}
// Premultiplied filtering preserves the narrow leaf color in partially covered
// mip texels instead of blending green edges with transparent black.
std::vector<DWORD> mip(const std::vector<DWORD> &source,unsigned width,unsigned height){
    unsigned w=std::max(1u,width/2),h=std::max(1u,height/2);std::vector<DWORD> result(w*h);
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){float alpha=0;RGB rgb{};
        for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){DWORD p=source[std::min(height-1,y*2+dy)*width+std::min(width-1,x*2+dx)];float a=(p>>24)/255.f;alpha+=a;
            for(unsigned c=0;c<3;++c)rgb[c]+=((p>>(16-c*8))&255)/255.f*a;}
        if(alpha>0)for(auto &c:rgb)c/=alpha;result[y*w+x]=pack(rgb,alpha*.25f);
    }
    return result;
}
}
