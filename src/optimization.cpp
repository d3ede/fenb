#include "optimization.hpp"
#include "native_binding.hpp"
#include "actor_visibility.hpp"
#include <map>
#include <sstream>

namespace optimization_detail {
template<class T> T &field(void *p,size_t offset) { return *reinterpret_cast<T *>(static_cast<unsigned char *>(p)+offset); }
using Render=void(__thiscall *)(void *,void *);
using Resolve=BOOL(__thiscall *)(void *,uint32_t,void **);
struct Binding {
    unsigned char *call=nullptr;
    Render render=nullptr;
    void ***data=nullptr;
    int *size=nullptr;
    void **player=nullptr;
    explicit operator bool() const { return call&&render&&data&&size&&player; }
};
Binding discover() {
    const auto module=native_patch::image();
    // FPS adapters may already have installed their verified substitutions.
    // Reuse their validated binding instead of rescanning patched instructions.
    if((native_binding::cached.module!=module||!native_binding::cached.valid)&&!native_binding::prepare(module))return {};
    auto **player=reinterpret_cast<void **>(native_binding::cached.address(0x842168));
    if(!player)return {};
    client_detail::Image image(module);
    // WndWorld::OnDraw calls the singleton dialog renderer, immediately before
    // positioning the native caption at 184/768 of the client height.
    const DWORD site=image.find("57 B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 47 3C 2B 47 34 69 C8 B8 00 00 00");
    // The vendor-only loop reads its CPtrArray separately from expiring chat.
    // Verify the CUSTOMTEXT owner and CObj cull-state fields used by that loop.
    const DWORD loop=image.find("A1 ?? ?? ?? ?? 33 C9 89 8D 0C F6 FF FF 85 C0 0F 8E ?? ?? ?? ?? 85 C9 0F 88 ?? ?? ?? ?? 3B C8 0F 8D ?? ?? ?? ?? A1 ?? ?? ?? ?? 6A 00 8B 34 88");
    if(!site || !loop || !image.bytes(loop+70,"8B 7E 78 83 BF B8 02 00 00 02"))return {};
    const DWORD render=DWORD(site+11+int32_t(image.value(site+7)));
    if(loop<render || loop-render>8192 || !image.bytes(render,"55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 81 EC 38 0A 00 00"))return {};
    const DWORD size=image.relative(image.value(loop+1)),data=image.relative(image.value(loop+38));
    if(!size || !data || size!=data+4 || !image.section(data,8,IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE))return {};
    auto *base=reinterpret_cast<unsigned char *>(module);
    return {base+site+6,reinterpret_cast<Render>(base+render),reinterpret_cast<void ***>(base+data),
            reinterpret_cast<int *>(base+size),player};
}
struct EffectsBinding {
    std::array<unsigned char *,2> calls{};
    Render render=nullptr;
    int *decorative=nullptr,*skills=nullptr;
    unsigned char *sfxCall=nullptr;
    Resolve resolve=nullptr;
    void *ctrlMap=nullptr;
    explicit operator bool() const { return calls[0]&&calls[1]&&render&&decorative&&skills; }
    bool owners() const { return bool(*this)&&sfxCall&&resolve&&ctrlMap; }
};
EffectsBinding discoverEffects() {
    const auto module=native_patch::image();
    if((native_binding::cached.module!=module||!native_binding::cached.valid)&&!native_binding::prepare(module))return {};
    client_detail::Image scan(module);
    // Both world-render paths call the same RenderObject method. Hook the calls,
    // leaving the application/environment vtable and simulation dispatch alone.
    const DWORD a=scan.find("FF 75 B4 8B CF E8 ?? ?? ?? ?? 8B 4D FC 5F 5E 33 CD 5B");
    const DWORD b=scan.find("FF 74 24 1C 8B CE E8 ?? ?? ?? ?? 8B 46 6C 89 44 24 38 E8");
    const DWORD flags=scan.find("83 3D ?? ?? ?? ?? 00 74 09 83 BE E8 05 00 00 00 74 12 83 3D ?? ?? ?? ?? 00 74 49 83 BE E8 05 00 00 00 74 40");
    if(!a||!b||!flags)return {};
    const DWORD render=DWORD(a+10+int32_t(scan.value(a+6)));
    if(render!=DWORD(b+11+int32_t(scan.value(b+7))) || flags<render || flags-render>8192 ||
       !scan.bytes(render,"53 8B DC 83 EC 08 83 E4 F8 83 C4 04 55 8B 6B 04 89 6C 24 04 8B EC 6A FF") ||
       // Supported client rebuilds use either local-stack size; the guarded
       // thiscall entry, both callers and all accessed fields are identical.
       !(scan.bytes(render+37,"81 EC 68 02 00 00") || scan.bytes(render+37,"81 EC 60 02 00 00")) ||
       !scan.bytes(flags+0x5b,"83 B9 1C 03 00 00 00 75 29"))return {};
    const DWORD decorative=scan.relative(scan.value(flags+2)),skills=scan.relative(scan.value(flags+20));
    if(!decorative || skills!=decorative+4 || !scan.section(decorative,8,IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE))return {};
    auto *base=reinterpret_cast<unsigned char *>(module);
    EffectsBinding result{{base+a+5,base+b+6},reinterpret_cast<Render>(base+render),
            reinterpret_cast<int *>(base+decorative),reinterpret_cast<int *>(base+skills)};
    // Replace only the complete five-byte virtual render dispatch. ECX and
    // the device argument have already been set by the native SFX-only loop.
    const DWORD draw=scan.find("8B 85 ?? ?? ?? ?? 8B CE FF 70 6C 8B 06 FF 50 24 47 3B 3D");
    // Native pet movement distinguishes egg/system pets (9) and following
    // pets (5) through CObj::m_dwAIInterface, not a model-name/asset heuristic.
    const DWORD egg=scan.find("83 BE 8C 01 00 00 09 75 66 8B 86 88 01 00 00 85 C0 74 5C 8D 4D F8");
    const DWORD pet=scan.find("83 BE 1C 03 00 00 00 75 60 83 BE 8C 01 00 00 05 75 57 8D 45 F8 B9 ?? ?? ?? ?? 50 FF B6 40 03 00 00 E8");
    auto *resolve=native_binding::cached.address(0x2d7180),*map=native_binding::cached.address(0x86c12c);
    if(draw>=render && draw-render<8192 && egg && pet>egg && pet-egg<512 && resolve && map &&
       base+DWORD(pet+38+int32_t(scan.value(pet+34)))==resolve &&
       reinterpret_cast<uintptr_t>(map)==scan.value(pet+22)) {
        result.sfxCall=base+draw+11;result.resolve=reinterpret_cast<Resolve>(resolve);result.ctrlMap=map;
    }
    return result;
}
enum class Owner { Unknown,Other,Player,Pet };
void *resolveActor(const EffectsBinding &binding,uint32_t id) {
    void *actor=nullptr;
    if(id==UINT32_MAX || !binding.resolve || !binding.ctrlMap ||
       !binding.resolve(binding.ctrlMap,id,&actor) || !actor || (field<DWORD>(actor,4)&1u))return nullptr;
    return actor;
}
Owner owner(const void *object) {
    if(!object)return Owner::Unknown;
    auto *actor=const_cast<void *>(object);
    if(field<DWORD>(actor,0x170)!=5)return Owner::Other;
    if(field<int>(actor,0x31c))return Owner::Player;
    const auto ai=field<DWORD>(actor,0x18c);
    return ai==5 || ai==9 ? Owner::Pet : Owner::Other;
}
bool hideSfx(const EffectsBinding &binding,const Settings &settings,void *effect,void *player=nullptr,bool actors=false) {
    if(!effect || !binding.owners() || field<DWORD>(effect,0x170)!=3)return false;
    if(!settings.hidePetEffects && !settings.hideDecorativeEffects && !settings.hideSkillEffects &&
       !(actors&&actor_visibility::changed(settings)))return false;
    auto *actor=resolveActor(binding,field<uint32_t>(effect,0x5f8));
    // Detached/retargeted effects can retain a destination after their source
    // is unavailable. A valid monster source must not become a player effect.
    if(!actor)actor=resolveActor(binding,field<uint32_t>(effect,0x5fc));
    if(actors && !actor_visibility::visible(settings,actor,player))return true;
    const auto kind=owner(actor);
    if(kind==Owner::Pet)return settings.hidePetEffects;
    if(kind!=Owner::Player)return false;
    return field<int>(effect,0x5e8)?settings.hideSkillEffects:settings.hideDecorativeEffects;
}
struct EffectContext {
    const EffectsBinding &binding;
    const Settings &settings;
    void *player=nullptr;
    bool actors=false;
};
thread_local const EffectContext *effectContext=nullptr;
struct OwnerScope {
    const EffectContext *previous=effectContext;
    EffectContext context;
    OwnerScope(const EffectsBinding &binding,const Settings &settings,void *player=nullptr,bool actors=false):
        context{binding,settings,player,actors} { effectContext=&context; }
    ~OwnerScope(){effectContext=previous;}
};
void __fastcall sfxRenderHook(void *effect,void *,void *device) {
    if(effectContext && hideSfx(effectContext->binding,effectContext->settings,effect,effectContext->player,effectContext->actors))return;
    auto *table=field<void **>(effect,0);
    reinterpret_cast<Render>(table[9])(effect,device);
}
struct EffectsScope {
    int *decorative=nullptr,*skills=nullptr;
    int previousDecorative=0,previousSkills=0;
    EffectsScope(EffectsBinding binding,const Settings &settings) {
        if(!binding)return;
        if(settings.hideDecorativeEffects){decorative=binding.decorative;previousDecorative=*decorative;*decorative=1;}
        if(settings.hideSkillEffects){skills=binding.skills;previousSkills=*skills;*skills=1;}
    }
    ~EffectsScope(){if(skills)*skills=previousSkills;if(decorative)*decorative=previousDecorative;}
};
using Compare=int(__cdecl *)(const void *,const void *);
using Sort=void(__cdecl *)(void *,size_t,size_t,Compare);
struct ActorBinding {
    unsigned char *sortCall=nullptr;
    Sort sort=nullptr;
    Compare compare=nullptr;
    void **data=nullptr,**player=nullptr;
    int *size=nullptr;
    explicit operator bool() const { return sortCall&&sort&&compare&&data&&player&&size; }
};
DWORD findInside(const client_detail::Image &scan,DWORD start,DWORD length,const char *pattern) {
    if(!start || !length || length>8192 || !scan.section(start,length,IMAGE_SCN_MEM_EXECUTE))return 0;
    std::vector<unsigned char> code(length);if(!scan.read(start,code.data(),code.size()))return 0;
    std::istringstream input(pattern);std::string token;std::vector<int> bytes;
    while(input>>token)bytes.push_back(token=="??"?-1:std::stoi(token,nullptr,16));
    if(bytes.empty()||bytes.size()>code.size())return 0;
    DWORD result=0;
    for(size_t i=0;i+bytes.size()<=code.size();++i) {
        size_t j=0;while(j<bytes.size()&&(bytes[j]<0||code[i+j]==bytes[j]))++j;
        if(j==bytes.size()){if(result)return 0;result=start+DWORD(i);}
    }
    return result;
}
ActorBinding discoverActors(const EffectsBinding &effects) {
    if(!effects.owners())return {};
    const auto module=native_patch::image();auto *base=reinterpret_cast<unsigned char *>(module);
    client_detail::Image scan(module);
    const DWORD render=DWORD(reinterpret_cast<unsigned char *>(effects.render)-base);
    DWORD limit=findInside(scan,render,8192,"81 3D ?? ?? ?? ?? 88 13 00 00 0F 8D ?? ?? ?? ?? 8B 86 78 01 00 00");
    if(!limit)limit=findInside(scan,render,8192,"81 3D ?? ?? ?? ?? 88 13 00 00 C7 86 B8 02 00 00 00 00 00 00 0F 8D ?? ?? ?? ?? 8B 86 78 01 00 00");
    const DWORD sort=findInside(scan,limit,512,"6A 04 FF 35 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 35 ?? ?? ?? ?? 83 C4 10 33 D2 85 F6");
    const DWORD publish=sort?findInside(scan,sort+18,64,"8B 04 95 ?? ?? ?? ?? 8D 0C 95 ?? ?? ?? ?? 42 89 88 94 01 00 00 3B D6 7C E7"):0;
    const DWORD peaceful=scan.find("33 C0 83 B9 E8 06 00 00 01 0F 94 C0 C3 CC CC CC");
    if(sort<render || sort-render>8192 || !limit || limit<render || limit>=sort || sort-limit>512 ||
       !peaceful || !scan.bytes(sort-5,"68") || publish<=sort || publish-sort>64)return {};
    const DWORD count=scan.relative(scan.value(sort+4)),data=scan.relative(scan.value(sort+9));
    const DWORD target=DWORD(sort+18+int32_t(scan.value(sort+14))),compare=scan.relative(scan.value(sort-4));
    if(!data || !count || !compare || scan.relative(scan.value(limit+2))!=count ||
       scan.relative(scan.value(sort+20))!=count || scan.relative(scan.value(publish+3))!=data ||
       scan.relative(scan.value(publish+10))!=data ||
       !scan.section(data,5000*sizeof(void *),IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE) ||
       !scan.section(count,sizeof(int),IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE) ||
       !scan.bytes(compare,"55 8B EC 8B 45 08 8B 08 8B 45 0C 8B 00 F3 0F 10 80 CC 02 00 00") ||
       !scan.bytes(target,"8B FF 55 8B EC 81 EC 18 01 00 00 A1 ?? ?? ?? ?? 33 C5 89 45 FC 8B 45 08"))return {};
    return {base+sort+13,reinterpret_cast<Sort>(base+target),reinterpret_cast<Compare>(base+compare),
        reinterpret_cast<void **>(base+data),reinterpret_cast<void **>(native_binding::cached.address(0x842168)),
        reinterpret_cast<int *>(base+count)};
}
struct Filter {
    Binding binding;
    void **original=nullptr;
    int count=0;
    std::array<void *,4096> visible{};
    Filter(Binding b,const Settings &s,bool actors=false):binding(b) {
        if(!b || (!s.limitShopSigns&&!(actors&&actor_visibility::changed(s))) ||
           !*b.player || *b.size<0 || *b.size>4096 || (*b.size&&!*b.data))return;
        auto *player=*b.player;
        const auto *eye=reinterpret_cast<const float *>(static_cast<unsigned char *>(player)+0x160);
        const bool range=s.limitShopSigns&&std::isfinite(eye[0])&&std::isfinite(eye[1])&&std::isfinite(eye[2]);
        const double radius2=double(s.shopSignDistance)*s.shopSignDistance;
        size_t visibleCount=0;
        for(int i=0;i<*b.size;++i) {
            auto *text=(*b.data)[i];
            auto *object=text?field<void *>(text,0x78):nullptr;
            bool keep=true;
            if(object && object!=player) {
                if(actors)keep=actor_visibility::visible(s,object,player);
                if(range&&keep) {
                const auto *p=reinterpret_cast<const float *>(static_cast<unsigned char *>(object)+0x160);
                const double x=double(p[0])-eye[0],y=double(p[1])-eye[1],z=double(p[2])-eye[2];
                const double distance=x*x+y*y+z*z;
                keep=!std::isfinite(distance)||distance<=radius2;
                }
            }
            if(keep)visible[visibleCount++]=text;
        }
        if(visibleCount==size_t(*b.size))return;
        original=*b.data;count=*b.size;
        *b.data=visible.data();*b.size=int(visibleCount);
    }
    ~Filter() { if(original){*binding.data=original;*binding.size=count;} }
};
struct State {
    Binding binding=discover();
    EffectsBinding effects=discoverEffects();
    ActorBinding actors=binding?discoverActors(effects):ActorBinding{};
    native_patch::Set patches,effectPatches;
    std::recursive_mutex mutex;
    std::map<const Optimization *,Settings> owners;
    DWORD thread=0;
    bool installed=false,rendering=false,effectsInstalled=false,effectsRendering=false;
};
State &state() { static State value;return value; }
Settings combined(const std::map<const Optimization *,Settings> &owners) {
    Settings result;
    const auto distance=[](float &value,float next){if(next>0)value=value>0?std::min(value,next):next;};
    for(const auto &[key,s]:owners) {
        if(s.limitShopSigns){result.shopSignDistance=result.limitShopSigns?std::min(result.shopSignDistance,s.shopSignDistance):s.shopSignDistance;result.limitShopSigns=true;}
        result.hideDecorativeEffects|=s.hideDecorativeEffects;result.hideSkillEffects|=s.hideSkillEffects;result.hidePetEffects|=s.hidePetEffects;
        result.renderPlayers&=s.renderPlayers;result.renderMonsters&=s.renderMonsters;result.renderPets&=s.renderPets;
        distance(result.playerDrawDistance,s.playerDrawDistance);distance(result.monsterDrawDistance,s.monsterDrawDistance);distance(result.petDrawDistance,s.petDrawDistance);
    }
    return result;
}
void __cdecl actorSortHook(void *objects,size_t count,size_t stride,Compare compare) {
    const auto &binding=state().actors;
    if(effectContext&&effectContext->actors&&objects==binding.data&&stride==sizeof(void *)&&
       count<=5000&&*binding.size==int(count)&&compare==binding.compare) {
        count=actor_visibility::compact(binding.data,count,effectContext->settings,effectContext->player);
        *binding.size=int(count);
    }
    binding.sort(objects,count,stride,compare);
}
void __fastcall renderHook(void *dialog,void *,void *render2D) {
    auto &shared=state();
    Settings settings;
    bool filter=false;
    {
        std::lock_guard lock(shared.mutex);
        settings=combined(shared.owners);
        filter=!shared.rendering && shared.thread==GetCurrentThreadId()&&
            (settings.limitShopSigns||(shared.actors&&actor_visibility::changed(settings)));
        if(filter)shared.rendering=true;
    }
    if(filter) {
        { Filter scope(shared.binding,settings,bool(shared.actors));shared.binding.render(dialog,render2D); }
        std::lock_guard lock(shared.mutex);shared.rendering=false;
    } else shared.binding.render(dialog,render2D);
}
void __fastcall effectsHook(void *world,void *,void *font) {
    auto &shared=state();Settings settings;bool apply=false;
    {
        std::lock_guard lock(shared.mutex);
        settings=combined(shared.owners);
        apply=!shared.effectsRendering&&shared.thread==GetCurrentThreadId()&&
            (settings.hideDecorativeEffects||settings.hideSkillEffects||settings.hidePetEffects||(shared.actors&&actor_visibility::changed(settings)));
        if(apply)shared.effectsRendering=true;
    }
    if(apply) {
        {EffectsScope scope(shared.effects,settings);OwnerScope owners(shared.effects,settings,
            shared.actors?*shared.actors.player:nullptr,bool(shared.actors));shared.effects.render(world,font);}
        std::lock_guard lock(shared.mutex);shared.effectsRendering=false;
    } else shared.effects.render(world,font);
}
}
Optimization::~Optimization() { Settings disabled;update(disabled); }
bool Optimization::supported() { return bool(optimization_detail::state().binding); }
bool Optimization::effectsSupported() { return bool(optimization_detail::state().effects); }
bool Optimization::petEffectsSupported() { return optimization_detail::state().effects.owners(); }
bool Optimization::actorVisibilitySupported() { return bool(optimization_detail::state().actors); }
void Optimization::update(const Settings &settings) {
    auto &s=optimization_detail::state();if(!s.binding&&!s.effects)return;
    std::lock_guard lock(s.mutex);
    if((settings.limitShopSigns&&s.binding)||((settings.hideDecorativeEffects||settings.hideSkillEffects)&&s.effects)||
       (settings.hidePetEffects&&s.effects.owners())||(s.actors&&actor_visibility::changed(settings)))
        s.owners[this]=settings;
    else s.owners.erase(this);
    const auto merged=optimization_detail::combined(s.owners);
    const bool models=s.actors&&actor_visibility::changed(merged);
    const bool shops=merged.limitShopSigns||models;
    const bool effects=merged.hideDecorativeEffects||merged.hideSkillEffects||merged.hidePetEffects||models;
    if(shops&&s.binding) {
        if(!s.installed) {
            unsigned char expected[5];std::memcpy(expected,s.binding.call,5);
            int32_t displacement=0;std::memcpy(&displacement,expected+1,4);
            const bool original=expected[0]==0xe8&&reinterpret_cast<uintptr_t>(s.binding.call)+5+displacement==reinterpret_cast<uintptr_t>(s.binding.render);
            if(original&&s.patches.call(s.binding.call,expected,reinterpret_cast<void *>(&optimization_detail::renderHook))&&s.patches.apply()) {
                s.installed=true;s.thread=GetCurrentThreadId();
            } else s.patches.clear();
        }
    }
    if(s.installed&&!shops&&!s.rendering){s.patches.clear();s.installed=false;}
    if(effects&&s.effects&&!s.effectsInstalled) {
        bool valid=true;
        for(auto *call:s.effects.calls) {
            unsigned char expected[5];std::memcpy(expected,call,5);
            int32_t displacement=0;std::memcpy(&displacement,expected+1,4);
            valid=valid&&expected[0]==0xe8&&reinterpret_cast<uintptr_t>(call)+5+displacement==reinterpret_cast<uintptr_t>(s.effects.render);
            if(!valid||!s.effectPatches.call(call,expected,reinterpret_cast<void *>(&optimization_detail::effectsHook))){valid=false;break;}
        }
        if(valid&&s.effects.owners()) {
            const unsigned char expected[5]={0x8b,0x06,0xff,0x50,0x24};
            valid=s.effectPatches.call(s.effects.sfxCall,expected,reinterpret_cast<void *>(&optimization_detail::sfxRenderHook));
        }
        if(valid&&s.actors) {
            unsigned char expected[5];std::memcpy(expected,s.actors.sortCall,5);
            int32_t displacement=0;std::memcpy(&displacement,expected+1,4);
            valid=expected[0]==0xe8&&reinterpret_cast<uintptr_t>(s.actors.sortCall)+5+displacement==reinterpret_cast<uintptr_t>(s.actors.sort)&&
                s.effectPatches.call(s.actors.sortCall,expected,reinterpret_cast<void *>(&optimization_detail::actorSortHook));
        }
        if(valid&&s.effectPatches.apply()){s.effectsInstalled=true;s.thread=GetCurrentThreadId();}
        else s.effectPatches.clear();
    }
    if(s.effectsInstalled&&!effects&&!s.effectsRendering){s.effectPatches.clear();s.effectsInstalled=false;}
}
