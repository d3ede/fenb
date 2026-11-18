#include "refinement_trail.hpp"
#include <cassert>
#include <cstdio>
void Log(const char*){}
namespace {
int renderCalls=0;
BOOL __thiscall nativeRender(void *model,void *device,void *matrix){
    assert(native_refinement::current&&native_refinement::current->model==model);
    assert(device==reinterpret_cast<void*>(2)&&matrix==reinterpret_cast<void*>(3));
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
    mesh(separated,identity,vertices);assert(vertices.size()==12); // No teleport bridge.
    std::vector<Puff> fading={{{0,0,0},.16f,{1,1,1,1}}};
    mesh(fading,identity,vertices);const float freshWidth=vertices[1].x-vertices[0].x;
    const DWORD freshAlpha=vertices[0].color>>24;
    fading[0].color[3]=.3f;mesh(fading,identity,vertices);
    assert(vertices[1].x-vertices[0].x>freshWidth && (vertices[0].color>>24)<freshAlpha);
    if(argc>1){
        auto image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);assert(image);
        native_patch::imageOverride=image;assert(native_binding::prepare(image));
        auto **table=reinterpret_cast<void **>(native_binding::cached.address(0x7b73f4));
        assert(table);void *before=table[19];
        for(int repeat=0;repeat<3;++repeat){
            native_refinement::request(&model,true);assert(table[19]==reinterpret_cast<void*>(&native_refinement::renderHook));
            native_refinement::request(&nested,true);native_refinement::request(&model,false);
            assert(table[19]==reinterpret_cast<void*>(&native_refinement::renderHook));
            native_refinement::request(&nested,false);assert(table[19]==before);
        }
        native_patch::imageOverride=nullptr;FreeLibrary(image);
    }
    puts("PASS: refinement scopes, paired merge, gap resampling, bounded geometry, UP validation and isolation");
}
