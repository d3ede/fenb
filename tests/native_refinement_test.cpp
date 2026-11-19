#include "refinement_trail.hpp"
#include "native_skate.hpp"
#include "native_skate_particles.hpp"
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
    // At rest the model origin can be offset from the visible native particles.
    const std::vector<Puff> attached={{{0,1.4f,2},.2f,{1,1,1,.9f}},
                                      {{0,1.38f,2},.2f,{1,1,1,.8f}}};
    puffs=attached;mesh(puffs,identity,vertices,nullptr,true);
    const auto attachedVertices=vertices;
    const Vec offsetOrigin{0,1.7f,2};
    puffs=attached;mesh(puffs,identity,vertices,&offsetOrigin,true,.4f);
    assert(vertices.size()==12&&vertices.size()==attachedVertices.size());
    assert(!memcmp(vertices.data(),attachedVertices.data(),vertices.size()*sizeof(Vertex)));
    const float centerY=attached.front().position.y;
    const auto [bottom,top]=std::minmax_element(vertices.begin(),vertices.end(),
        [](const Vertex &a,const Vertex &b){return a.y<b.y;});
    assert(top->y-centerY>centerY-bottom->y);
    for(const auto &v:vertices)if(v.color>>24)assert(v.y==centerY);
    const std::vector<Puff> running={{{0,1.4f,2},.2f,{1,1,1,.9f}},
                                     {{-.5f,1.4f,2},.2f,{1,1,1,.8f}}};
    const Vec runningOrigin{.3f,1.4f,2}, raisedOrigin{.3f,1.7f,2};
    puffs=running;mesh(puffs,identity,vertices,&runningOrigin,true);
    assert(std::any_of(vertices.begin(),vertices.end(),[](const Vertex &v){
        return std::abs(v.x-.3f)<.00001f&&(v.color>>24)>0;
    }));
    const auto runningVertices=vertices;
    puffs=running;mesh(puffs,identity,vertices,&raisedOrigin,true);
    assert(vertices.size()==runningVertices.size());
    assert(!memcmp(vertices.data(),runningVertices.data(),vertices.size()*sizeof(Vertex)));
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
    // Keep the curved path through a jump/turn when the next particle appears.
    MotionPath history;
    const std::array<Vec,8> jumpTrace={{{0,-2,4},{0,-1,4},{0,0,4},{0,.4f,4},
                                      {0,.85f,4},{.35f,.85f,4},{.65f,.85f,4},{.9f,.85f,4}}};
    std::span<const Vec> recorded;
    for(size_t i=0;i<jumpTrace.size();++i)recorded=history.observe(jumpTrace[i],100+20*i);
    assert(recorded.size()==jumpTrace.size());
    const std::vector<Puff> beforeBirth={{{0,0,4},.2f,{1,1,1,.72f}},{{0,-1,4},.2f,{1,1,1,.60f}},
                                        {{0,-2,4},.2f,{1,1,1,.48f}},{{0,-3,4},.2f,{1,1,1,.36f}}};
    const Vec liveEmitter=jumpTrace.back();
    puffs=beforeBirth;mesh(puffs,identity,vertices,&liveEmitter,false,0,recorded);
    const float beforeCorner=coverage(.42f,.85f);
    assert(beforeCorner>100);
    puffs=beforeBirth;puffs.insert(puffs.begin(),{liveEmitter,.2f,{1,1,1,.93f}});
    mesh(puffs,identity,vertices,&liveEmitter,false,0,recorded);
    assert(std::abs(beforeCorner-coverage(.42f,.85f))<18);
    assert(coverage(.42f,.34f)<30); // The new particle must not cut across the bend.
    assert(history.observe({100,.85f,4},261).size()==1); // Teleport.
    assert(history.observe({100,.85f,4},500).size()==1); // An expired model slot.
    puffs={{{0,0,0},.2f,{1,1,1,1}},{{0,-1,0},.2f,{1,1,1,.8f}},
           {{0,-30,0},.2f,{1,1,1,.6f}},{{0,-31,0},.2f,{1,1,1,.4f}}};
    mesh(puffs,identity,vertices);assert(coverage(0,-15)==0); // An outlier teleport stays disconnected.
    // Abrupt turns, foreshortening and near-camera samples must never create
    // reversed triangles or a sharp diagonal through the glow.
    std::vector<Puff> jump={{{-.9f,.55f,2},.23f,{1,1,1,1}},{{.05f,.03f,2.4f},.24f,{1,1,1,.9f}},
        {{.22f,-.02f,2.7f},.24f,{1,1,1,.8f}},{{-.2f,.12f,3.0f},.24f,{1,1,1,.7f}},
        {{.05f,-.3f,3.6f},.24f,{1,1,1,.6f}},{{-.6f,-.8f,4.4f},.24f,{1,1,1,.5f}}};
    mesh(jump,identity,vertices,nullptr,true);
    assert(vertices.size()>=36);
    unsigned rounded=0;
    for(size_t i=0;i<vertices.size();i+=3){
        const auto project=[](Vertex v){return trail_geometry::Vec{v.x/v.z,v.y/v.z,0};};
        const auto a=project(vertices[i]),b=project(vertices[i+1]),c=project(vertices[i+2]);
        assert(trail_geometry::cross(b-a,c-a)>0);
        rounded+=vertices[i].v>0||vertices[i+1].v>0||vertices[i+2].v>0;
    }
    assert(rounded>=4);
    std::array<skate_trail::Point,3> yaw={{{{0,0,2},{1,0,0},1},{{0,1,2},{-1,0,0},1},{{1,1,2},{1,0,0},1}}};
    skate_trail::Path skate(yaw);
    assert(skate.at(.5f).across.x>.99f&&skate.at(1.5f).across.x>.99f);
    {
        float origin[]={1,2,3};native_refinement::Scope anchored(&model,origin);
        origin[0]=10;assert(anchored.context.hasOrigin&&anchored.context.origin[0]==1);
    }
    {
        assert(native_skate::boardTrail(1,0x3cf5c28f)&&native_skate::boardTrail(2,0x3cf5c28f));
        assert(!native_skate::boardTrail(2,0x3eb33333)&&!native_skate::boardTrail(3,0x3cf5c28f));
        std::array<native_skate::Node,512> pool{};
        pool[20].left={-.5f,1,4};pool[20].right={.5f,1,4};pool[20].next=&pool[7];
        pool[7].left={-.5f,1,2};pool[7].right={.5f,1,2};pool[7].age=1.5f;
        std::array<DWORD,11> belt{};
        belt[4]=2;belt[5]=reinterpret_cast<uintptr_t>(pool.data());belt[10]=reinterpret_cast<uintptr_t>(&pool[20]);
        std::array<skate_trail::Point,4> decoded{};
        assert(native_skate::points(belt.data(),decoded)==2);
        assert(decoded[0].center.z==4&&decoded[0].across.x==1&&decoded[1].opacity==.5f);
        pool[7].next=&pool[20];assert(native_skate::points(belt.data(),decoded)==0);
    }
    if(argc>1){
        auto image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);assert(image);
        native_patch::imageOverride=image;assert(native_binding::prepare(image));
        auto **table=reinterpret_cast<void **>(native_binding::cached.address(0x7b73f4));
        assert(table);void *before=table[19];
        auto **skate=native_skate::discover(image);assert(skate);
        auto *skateBefore=*skate;
        native_skate::request(&model,true);assert(*skate==reinterpret_cast<void*>(&native_skate::renderHook));
        native_skate::request(&model,false);assert(*skate==skateBefore);
        const auto particles=native_skate_particles::discover(image);
        assert(particles.calls[0]&&particles.calls[1]&&particles.render&&particles.groups);
        std::array<std::array<BYTE,5>,2> originalCalls{};
        for(size_t i=0;i<particles.calls.size();++i)
            memcpy(originalCalls[i].data(),particles.calls[i],originalCalls[i].size());
        native_skate_particles::request(&model,true);
        for(size_t i=0;i<particles.calls.size();++i)
            assert(memcmp(originalCalls[i].data(),particles.calls[i],originalCalls[i].size())!=0);
        native_skate_particles::request(&model,false);
        for(size_t i=0;i<particles.calls.size();++i)
            assert(memcmp(originalCalls[i].data(),particles.calls[i],originalCalls[i].size())==0);
        for(int repeat=0;repeat<3;++repeat){
            native_refinement::request(&model,true);assert(table[19]==reinterpret_cast<void*>(&native_refinement::renderHook));
            assert(native_refinement::positionAvailable);
            native_refinement::request(&nested,true);native_refinement::request(&model,false);
            assert(table[19]==reinterpret_cast<void*>(&native_refinement::renderHook));
            native_refinement::request(&nested,false);assert(table[19]==before);
        }
        native_patch::imageOverride=nullptr;FreeLibrary(image);
    }
    std::array<BYTE,32*0x3c> groups{};
    for(size_t i=0;i<32;++i)
        assert(native_skate_particles::boardBatch(groups.data(),groups.data()+i*0x3c)==(i>=13&&i<25));
    assert(!native_skate_particles::boardBatch(groups.data(),groups.data()+13*0x3c+1));
    std::array<skate_trail::Point,2> nearGrass={{{{0,2,4},{1,0,0},1},{{0,2,6},{1,0,0},1}}};
    skate_trail::Path raised(nearGrass,.5f);
    assert(raised.at(0).center.y==2&&raised.at(.5f).center.y>2&&raised.at(2).center.y==2.5f);
    puts("PASS: refinement scopes, paired merge, gap resampling, bounded geometry, UP validation and isolation");
}
