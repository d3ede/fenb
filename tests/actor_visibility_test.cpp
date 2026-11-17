#include "optimization.cpp"
#include <cassert>
#include <cstdio>
void Log(const char *){}
namespace {
alignas(4) std::array<std::array<unsigned char,0x700>,7> actors{};
int mapToken=0;size_t sortedCount=0,sortedStride=0;
void *sortedData=nullptr;
int __cdecl compare(const void *,const void *){return 0;}
void __cdecl sort(void *data,size_t count,size_t stride,optimization_detail::Compare cmp) {
    sortedData=data;sortedCount=count;sortedStride=stride;assert(cmp==&compare);
}
BOOL __thiscall resolve(void *map,uint32_t id,void **out) {
    assert(map==&mapToken);if(id<1||id>7)return FALSE;*out=actors[id-1].data();return TRUE;
}
}
int main() {
    using optimization_detail::field;
    // The native/default sentinel is displayed only at the right edge. Left,
    // arrows, numeric entry and reset all use the same monotonic scale.
    assert(actor_visibility::displayDistance(0,400)==400);
    assert(actor_visibility::displayDistance(1000,400)==400);
    assert(actor_visibility::savedDistance(400,400)==0);
    assert(actor_visibility::savedDistance(395,400)==395);
    assert(actor_visibility::savedDistance(1,400)==1);
    assert(actor_visibility::displayDistance(0,550)==550&&actor_visibility::savedDistance(550,550)==0);
    for(float value:{1.f,5.f,50.f,125.f,395.f})assert(actor_visibility::savedDistance(actor_visibility::displayDistance(value,400),400)==value);
    for(auto &actor:actors){field<DWORD>(actor.data(),0x170)=5;field<DWORD>(actor.data(),0x6e8)=1;}
    field<int>(actors[0].data(),0x31c)=field<int>(actors[1].data(),0x31c)=1;
    field<DWORD>(actors[2].data(),0x18c)=2;field<DWORD>(actors[2].data(),0x6e8)=2;
    field<DWORD>(actors[4].data(),0x18c)=5;field<DWORD>(actors[5].data(),0x18c)=9;
    field<DWORD>(actors[6].data(),0x170)=0;
    for(size_t i=0;i<7;++i)field<float>(actors[i].data(),0x2cc)=float(10*i);
    void *player=actors[0].data();void *objects[7]{};
    for(size_t i=0;i<7;++i)objects[i]=actors[i].data();
    const auto before=actors;
    Settings s;assert(!actor_visibility::changed(s));
    assert(actor_visibility::compact(objects,7,s,player)==7);
    s.renderPlayers=false;assert(!actor_visibility::visible(s,actors[1].data(),player));
    assert(actor_visibility::visible(s,actors[2].data(),player));
    s.renderMonsters=s.renderPets=false;
    assert(actor_visibility::visible(s,player,player));
    assert(actor_visibility::visible(s,actors[3].data(),player)); // Peaceful NPC.
    assert(!actor_visibility::visible(s,actors[4].data(),player)&&!actor_visibility::visible(s,actors[5].data(),player));
    assert(actor_visibility::visible(s,actors[6].data(),player));
    const auto kept=actor_visibility::compact(objects,7,s,player);
    assert(kept==3&&objects[0]==player&&objects[1]==actors[3].data()&&objects[2]==actors[6].data());
    assert(actor_visibility::compact(objects,5001,s,player)==5001);
    assert(actor_visibility::visible(s,actors[1].data(),nullptr));
    s=Settings{};s.playerDrawDistance=10;s.monsterDrawDistance=19;s.petDrawDistance=45;
    assert(actor_visibility::visible(s,actors[1].data(),player)); // Inclusive limit.
    s.playerDrawDistance=9;assert(!actor_visibility::visible(s,actors[1].data(),player));
    assert(!actor_visibility::visible(s,actors[2].data(),player));
    assert(actor_visibility::visible(s,actors[4].data(),player)&&!actor_visibility::visible(s,actors[5].data(),player));
    field<float>(actors[1].data(),0x2cc)=std::numeric_limits<float>::quiet_NaN();
    assert(actor_visibility::visible(s,actors[1].data(),player));
    field<float>(actors[1].data(),0x2cc)=-1;assert(actor_visibility::visible(s,actors[1].data(),player));
    field<float>(actors[1].data(),0x2cc)=10;
    assert(actors==before);
    // The native sort receives the filtered count and retains its ABI. Other
    // sort calls and a missing render scope pass through unchanged.
    auto &state=optimization_detail::state();const auto saved=state.actors;
    int count=7;unsigned char instruction=0;void *active=player;
    for(size_t i=0;i<7;++i)objects[i]=actors[i].data();
    std::array<float,12> nativeRanges{};nativeRanges.fill(400);
    state.actors={&instruction,&sort,&compare,objects,&active,&count,nativeRanges.data()};
    int flag=0;optimization_detail::EffectsBinding effects{{&instruction,&instruction},reinterpret_cast<optimization_detail::Render>(1),&flag,&flag,&instruction,&resolve,&mapToken};
    s=Settings{};s.renderPlayers=s.renderMonsters=s.renderPets=false;
    {optimization_detail::OwnerScope scope(effects,s,player,true);optimization_detail::actorSortHook(objects,7,4,&compare);}
    assert(count==3&&sortedCount==3&&sortedStride==4&&sortedData==objects);
    optimization_detail::actorSortHook(objects,7,4,&compare);assert(sortedCount==7&&count==3);
    count=7;for(size_t i=0;i<7;++i)objects[i]=actors[i].data();
    void *other[7]{};
    {optimization_detail::OwnerScope scope(effects,s,player,true);optimization_detail::actorSortHook(other,7,4,&compare);}
    assert(sortedCount==7&&sortedData==other&&count==7);
    state.actors=saved;
    // Attached SFX and shop captions follow model visibility, while local
    // character captions, NPCs and unresolved owners remain visible.
    alignas(4) std::array<unsigned char,0x600> effect{};field<DWORD>(effect.data(),0x170)=3;
    field<uint32_t>(effect.data(),0x5fc)=UINT32_MAX;
    for(unsigned id:{2u,3u,5u,6u}){field<uint32_t>(effect.data(),0x5f8)=id;assert(optimization_detail::hideSfx(effects,s,effect.data(),player,true));}
    for(unsigned id:{1u,4u,99u}){field<uint32_t>(effect.data(),0x5f8)=id;assert(!optimization_detail::hideSfx(effects,s,effect.data(),player,true));}
    field<uint32_t>(effect.data(),0x5f8)=5;assert(!optimization_detail::hideSfx(effects,s,effect.data(),player,false));
    alignas(4) std::array<unsigned char,0x7c> captions[3]{};
    void *entries[3]{};for(int i=0;i<3;++i){entries[i]=captions[i].data();field<void *>(entries[i],0x78)=actors[i].data();}
    void **data=entries;count=3;
    optimization_detail::Binding shops{&instruction,reinterpret_cast<optimization_detail::Render>(1),&data,&count,&active};
    {optimization_detail::Filter filter(shops,s,true);assert(count==1&&data[0]==entries[0]);}
    assert(count==3&&data==entries&&actors==before);
    std::map<const Optimization *,Settings> owners;
    Settings a,b;a.playerDrawDistance=100;b.playerDrawDistance=40;b.renderPets=false;
    owners[reinterpret_cast<const Optimization *>(1)]=a;owners[reinterpret_cast<const Optimization *>(2)]=b;
    auto merged=optimization_detail::combined(owners);assert(merged.playerDrawDistance==40&&!merged.renderPets&&merged.renderPlayers);
    owners.erase(reinterpret_cast<const Optimization *>(2));merged=optimization_detail::combined(owners);assert(merged.playerDrawDistance==100&&merged.renderPets);
    puts("PASS: independent actor switches/ranges, camera-distance boundaries, self/NPC isolation, invalid-distance fallback, bounded stable render list, sort ABI, captions/SFX and multi-owner requests; actor data unchanged.");
}
