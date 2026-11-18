#include "refinement_trail.hpp"
#include <cassert>
#include <cstdio>
void Log(const char*){}
namespace {
int renderCalls=0;
BOOL __thiscall nativeRender(void *model,void *device,void *matrix){
    assert(native_refinement::current&&native_refinement::current->model==model);
    assert(device==reinterpret_cast<void*>(2)&&matrix==reinterpret_cast<void*>(3));
    if(native_refinement::positionAvailable){
        assert(native_refinement::current->hasOrigin);
        assert(native_refinement::current->origin[0]==9&&native_refinement::current->origin[2]==7);
    }
    ++renderCalls;return FALSE;
}
}
int main(int argc,char **argv){
    int model=1,nested=2,finished=0;
    {
        native_refinement::Scope scope(&model);
        assert(native_refinement::current->model==&model);
        native_refinement::current->consumer=&finished;
        native_refinement::current->finish=[](void *p){++*static_cast<int*>(p);};
        {native_refinement::Scope inner(&nested);assert(native_refinement::current->model==&nested);}
        assert(native_refinement::current==&scope.context&&finished==0);
    }
    assert(!native_refinement::current&&finished==1);
    native_refinement::original=&nativeRender;
    assert(!native_refinement::renderHook(&model,nullptr,reinterpret_cast<void*>(2),reinterpret_cast<void*>(3)));
    assert(renderCalls==1&&!native_refinement::current);
    std::array<char,0x160> nativeModel{};float position[]={9,8,7};
    memcpy(nativeModel.data()+0x12c,position,sizeof(position));native_refinement::positionAvailable=true;
    assert(!native_refinement::renderHook(nativeModel.data(),nullptr,reinterpret_cast<void*>(2),reinterpret_cast<void*>(3)));
    assert(renderCalls==2&&!native_refinement::current);native_refinement::positionAvailable=false;
    native_refinement::original=nullptr;
    D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
    using namespace refinement_trail;
    Vertex quad[]={{-.04f,.1f,0,0x80ffffff,0,0},{.04f,.1f,0,0x80ffffff,1,0},
                   {-.04f,-.1f,0,0x80ffffff,0,1},{.04f,-.1f,0,0x80ffffff,1,1}};
    std::vector<Puff> puffs;
    ParticleDraw draw{D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex)};
    assert(extract(draw,0x142,identity,{1,1,1,1},puffs)&&puffs.size()==1);
    assert(puffs[0].radius>=.12f&&std::abs(puffs[0].color[3]-128.f/255)<.001f);
    quad[3].u=.5f;assert(!extract(draw,0x142,identity,{1,1,1,1},puffs)&&puffs.size()==1);quad[3].u=1;
    WORD indices[]={0,1,2,2,1,3};draw={D3DPT_TRIANGLELIST,2,quad,sizeof(Vertex),indices,D3DFMT_INDEX16,0,4};
    assert(extract(draw,0x142,identity,{1,1,1,1},puffs));
    indices[5]=4;assert(!extract(draw,0x142,identity,{1,1,1,1},puffs)&&puffs.size()==2);
    puffs.clear();
    for(int i=0;i<8;++i)for(int side:{-1,1})puffs.push_back({{side*.07f,0,i*.3f},.12f,{1,1,1,1-i*.11f}});
    merge(puffs);assert(puffs.size()==8);
    std::vector<Vertex> vertices;mesh(puffs,identity,vertices);
    assert(vertices.size()>8*6&&vertices.size()<=MaxVertices);
    const size_t linked=vertices.size()/6;
    printf("Resampled trail: %zu sprites for %zu merged points\n",linked,puffs.size());
    puffs.clear();
    for(int i=0;i<256;++i)puffs.push_back({{0,0,i*.3f},.12f,{1,1,1,1-i*.003f}});
    mesh(puffs,identity,vertices);assert(vertices.size()<=MaxVertices);
    for(const auto &v:vertices)assert(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z));
    std::vector<Puff> separated={{{0,0,0},.12f,{1,1,1,1}},{{20,0,0},.12f,{1,1,1,.5f}}};
    mesh(separated,identity,vertices);
    for(size_t i=0;i<vertices.size();i+=3){
        float lo=std::min({vertices[i].x,vertices[i+1].x,vertices[i+2].x});
        float hi=std::max({vertices[i].x,vertices[i+1].x,vertices[i+2].x});
        assert(hi-lo<2); // Caps remain local; no teleport bridge.
    }
    std::vector<Puff> fading={{{0,0,0},.16f,{1,1,1,1}}};
    auto peak=[&](){DWORD alpha=0;for(const auto &v:vertices)alpha=std::max(alpha,v.color>>24);return alpha;};
    auto width=[&](){float lo=100,hi=-100;for(const auto &v:vertices){lo=std::min(lo,v.x);hi=std::max(hi,v.x);}return hi-lo;};
    mesh(fading,identity,vertices);const float freshWidth=width();
    const DWORD freshAlpha=peak();
    fading[0].color[3]=.3f;mesh(fading,identity,vertices);
    assert(width()>freshWidth && peak()<freshAlpha);
    // Co-moving sample points must retain the same energy across a full native
    // emission cycle, including spacing beyond the previous 2.75-unit cutoff.
    auto coverage=[&](float x,float y){
        float alpha=0;
        for(size_t i=0;i<vertices.size();i+=3){
            const auto &a=vertices[i],&b=vertices[i+1],&c=vertices[i+2];
            float det=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
            if(std::abs(det)<.000001f)continue;
            float u=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/det;
            float v=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/det,w=1-u-v;
            if(std::min({u,v,w})>=-.00001f)
                alpha=std::max(alpha,u*(a.color>>24)+v*(b.color>>24)+w*(c.color>>24));
        }
        return alpha;
    };
    Vec emitter{};
    for(float spacing:{.3f,1.f,4.f,8.f,16.f,32.f}){
        float low=255,high=0;
        for(int phase=0;phase<=100;++phase){
            puffs.clear();float fraction=phase/100.f;
            for(int i=0;i<6;++i)puffs.push_back({{0,-(i+fraction)*spacing,0},.2f,{1,1,1,.56f*(1-(i+fraction)/6)}});
            mesh(puffs,identity,vertices,&emitter);
            float energy=coverage(0,-spacing*.6f);low=std::min(low,energy);high=std::max(high,energy);
            assert(coverage(0,-spacing*2.5f)>40); // Filled even during a fast run.
        }
        printf("Emission spacing %.1f: head coverage %.2f..%.2f\n",spacing,low,high);
        assert(low>50&&high-low<3); // No birth-dependent brightness pulse.
    }
    puffs={{{0,0,0},.2f,{1,1,1,1}},{{0,-1,0},.2f,{1,1,1,.8f}},
           {{0,-30,0},.2f,{1,1,1,.6f}},{{0,-31,0},.2f,{1,1,1,.4f}}};
    mesh(puffs,identity,vertices);assert(coverage(0,-15)==0); // An outlier teleport stays disconnected.
    {
        float origin[]={1,2,3};native_refinement::Scope anchored(&model,origin);
        origin[0]=10;assert(anchored.context.hasOrigin&&anchored.context.origin[0]==1);
    }
    if(argc>1){
        auto image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);assert(image);
        native_patch::imageOverride=image;assert(native_binding::prepare(image));
        auto **table=reinterpret_cast<void **>(native_binding::cached.address(0x7b73f4));
        assert(table);void *before=table[19];
        for(int repeat=0;repeat<3;++repeat){
            native_refinement::request(&model,true);assert(table[19]==reinterpret_cast<void*>(&native_refinement::renderHook));
            assert(native_refinement::positionAvailable);
            native_refinement::request(&nested,true);native_refinement::request(&model,false);
            assert(table[19]==reinterpret_cast<void*>(&native_refinement::renderHook));
            native_refinement::request(&nested,false);assert(table[19]==before);
        }
        native_patch::imageOverride=nullptr;FreeLibrary(image);
    }
    puts("PASS: refinement scopes, paired merge, gap resampling, bounded geometry, UP validation and isolation");
}
