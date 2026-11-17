#include "optimization.cpp"
#include <cassert>
#include <cstdio>
void Log(const char *) {}
namespace {
alignas(4) std::array<unsigned char,0x320> actors[5]{};
unsigned resolves=0,draws=0;
int mapToken=0,deviceToken=0;
BOOL __thiscall resolveOwner(void *map,uint32_t id,void **out) {
    assert(map==&mapToken);++resolves;
    if(id<1||id>5)return FALSE;
    *out=actors[id-1].data();return TRUE;
}
void __thiscall drawEffect(void *effect,void *device) { assert(effect&&device==&deviceToken);++draws; }
}
int main() {
    using namespace optimization_detail;
    alignas(4) std::array<unsigned char,0x16c> player{},nearby{},distant{},invalid{};
    field<float>(nearby.data(),0x160)=10;
    field<float>(distant.data(),0x160)=80;
    field<float>(invalid.data(),0x160)=std::numeric_limits<float>::quiet_NaN();
    alignas(4) std::array<unsigned char,0x7c> texts[4]{};
    void *objects[]={nearby.data(),distant.data(),player.data(),invalid.data()};
    void *entries[4]{};
    for(int i=0;i<4;++i){field<void *>(texts[i].data(),0x78)=objects[i];entries[i]=texts[i].data();}
    void **data=entries;int count=4;void *active=player.data();unsigned char instruction=0;
    Binding binding{&instruction,reinterpret_cast<Render>(1),&data,&count,&active};
    Settings settings;
    {Filter scope(binding,settings);assert(data==entries&&count==4);}
    settings.limitShopSigns=true;settings.shopSignDistance=40;
    {Filter scope(binding,settings);assert(data!=entries&&count==3);assert(data[0]==entries[0]&&data[1]==entries[2]&&data[2]==entries[3]);}
    assert(data==entries&&count==4);
    assert(field<float>(distant.data(),0x160)==80); // only the render list changes
    settings.shopSignDistance=100;
    {Filter scope(binding,settings);assert(data==entries&&count==4);}
    active=nullptr;
    {Filter scope(binding,settings);assert(data==entries&&count==4);}
    active=player.data();count=4097;
    {Filter scope(binding,settings);assert(data==entries&&count==4097);}
    count=4;field<float>(player.data(),0x160)=std::numeric_limits<float>::infinity();
    {Filter scope(binding,settings);assert(data==entries&&count==4);}
    for(int i=0;i<4;++i)assert(field<void *>(texts[i].data(),0x78)==objects[i]);
    int decorative=3,skills=4;
    EffectsBinding effects{{&instruction,&instruction},reinterpret_cast<Render>(1),&decorative,&skills};
    Settings visual;
    {EffectsScope scope(effects,visual);assert(decorative==3&&skills==4);}
    visual.hideDecorativeEffects=true;
    {EffectsScope scope(effects,visual);assert(decorative==1&&skills==4);}
    assert(decorative==3&&skills==4);
    visual.hideSkillEffects=true;
    {EffectsScope scope(effects,visual);assert(decorative==1&&skills==1);}
    assert(decorative==3&&skills==4);
    visual.hideDecorativeEffects=false;
    {EffectsScope scope(effects,visual);assert(decorative==3&&skills==1);}
    assert(decorative==3&&skills==4);
    // The native SFX dispatch must skip only the selected owner category;
    // owner lookup failures and monster/player targets remain independent.
    effects.sfxCall=&instruction;effects.resolve=&resolveOwner;effects.ctrlMap=&mapToken;
    for(auto &actor:actors)field<DWORD>(actor.data(),0x170)=5;
    field<int>(actors[0].data(),0x31c)=1;
    field<DWORD>(actors[1].data(),0x18c)=5;
    field<DWORD>(actors[2].data(),0x18c)=9;
    field<DWORD>(actors[3].data(),0x18c)=2;
    field<DWORD>(actors[4].data(),4)=1;
    alignas(4) std::array<unsigned char,0x600> effect{};
    std::array<void *,10> table{};table[9]=reinterpret_cast<void *>(&drawEffect);
    field<void **>(effect.data(),0)=table.data();field<DWORD>(effect.data(),0x170)=3;
    field<uint32_t>(effect.data(),0x5f8)=2;field<uint32_t>(effect.data(),0x5fc)=UINT32_MAX;
    Settings petSettings;
    assert(!hideSfx(effects,petSettings,effect.data())&&!resolves);
    petSettings.hidePetEffects=true;
    for(int skill:{0,1,-1}) { field<int>(effect.data(),0x5e8)=skill;assert(hideSfx(effects,petSettings,effect.data())); }
    field<uint32_t>(effect.data(),0x5f8)=3;assert(hideSfx(effects,petSettings,effect.data()));
    field<uint32_t>(effect.data(),0x5f8)=1;assert(!hideSfx(effects,petSettings,effect.data()));
    field<uint32_t>(effect.data(),0x5f8)=4;field<uint32_t>(effect.data(),0x5fc)=2;
    assert(!hideSfx(effects,petSettings,effect.data()));
    field<uint32_t>(effect.data(),0x5f8)=UINT32_MAX;assert(hideSfx(effects,petSettings,effect.data()));
    field<uint32_t>(effect.data(),0x5fc)=UINT32_MAX;assert(!hideSfx(effects,petSettings,effect.data()));
    field<uint32_t>(effect.data(),0x5f8)=5;assert(!hideSfx(effects,petSettings,effect.data()));
    petSettings.hidePetEffects=false;petSettings.hideDecorativeEffects=true;
    field<int>(effect.data(),0x5e8)=0;field<uint32_t>(effect.data(),0x5f8)=1;
    assert(hideSfx(effects,petSettings,effect.data()));
    field<int>(effect.data(),0x5e8)=1;assert(!hideSfx(effects,petSettings,effect.data()));
    petSettings.hideSkillEffects=true;assert(hideSfx(effects,petSettings,effect.data()));
    field<uint32_t>(effect.data(),0x5f8)=2;assert(!hideSfx(effects,petSettings,effect.data()));
    field<uint32_t>(effect.data(),0x5f8)=UINT32_MAX;field<uint32_t>(effect.data(),0x5fc)=1;
    assert(hideSfx(effects,petSettings,effect.data())); // Destination can identify a detached player effect.
    field<uint32_t>(effect.data(),0x5f8)=4;assert(!hideSfx(effects,petSettings,effect.data()));
    petSettings=Settings{};petSettings.hidePetEffects=true;
    field<uint32_t>(effect.data(),0x5f8)=2;
    const auto originalEffect=effect;
    const auto originalPet=actors[1];
    assert(!effectContext);
    { OwnerScope scope(effects,petSettings);
      sfxRenderHook(effect.data(),nullptr,&deviceToken);assert(!draws);
      { Settings show;OwnerScope nested(effects,show);sfxRenderHook(effect.data(),nullptr,&deviceToken);assert(draws==1); }
      sfxRenderHook(effect.data(),nullptr,&deviceToken);assert(draws==1);
    }
    assert(!effectContext);sfxRenderHook(effect.data(),nullptr,&deviceToken);assert(draws==2);
    assert(effect==originalEffect&&actors[1]==originalPet&&decorative==3&&skills==4);
    puts("PASS: pet/system-pet ownership, player/monster isolation, missing-source fallback, skill independence, scoped virtual rendering and unchanged models/state.");
    puts("PASS: independent native decoration/skill switches, disabled no-op and exact option restoration.");
    puts("PASS: shop-sign range, own sign, stable order, invalid-position fallback, bounded list and exact restoration.");
}
