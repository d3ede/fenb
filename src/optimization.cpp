#include "optimization.hpp"
#include "native_binding.hpp"
#include <map>

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
Owner owner(const EffectsBinding &binding,uint32_t id) {
    void *actor=nullptr;
    if(id==UINT32_MAX || !binding.resolve || !binding.ctrlMap ||
       !binding.resolve(binding.ctrlMap,id,&actor) || !actor || (field<DWORD>(actor,4)&1u))return Owner::Unknown;
    if(field<DWORD>(actor,0x170)!=5)return Owner::Other;
    if(field<int>(actor,0x31c))return Owner::Player;
    const auto ai=field<DWORD>(actor,0x18c);
    return ai==5 || ai==9 ? Owner::Pet : Owner::Other;
}
bool hideSfx(const EffectsBinding &binding,const Settings &settings,void *effect) {
    if(!effect || !binding.owners() || field<DWORD>(effect,0x170)!=3)return false;
    if(!settings.hidePetEffects && !settings.hideDecorativeEffects && !settings.hideSkillEffects)return false;
    auto kind=owner(binding,field<uint32_t>(effect,0x5f8));
    // Detached/retargeted effects can retain a destination after their source
    // is unavailable. A valid monster source must not become a player effect.
    if(kind==Owner::Unknown)kind=owner(binding,field<uint32_t>(effect,0x5fc));
    if(kind==Owner::Pet)return settings.hidePetEffects;
    if(kind!=Owner::Player)return false;
    return field<int>(effect,0x5e8)?settings.hideSkillEffects:settings.hideDecorativeEffects;
}
struct EffectContext {
    const EffectsBinding &binding;
    const Settings &settings;
};
thread_local const EffectContext *effectContext=nullptr;
struct OwnerScope {
    const EffectContext *previous=effectContext;
    EffectContext context;
    OwnerScope(const EffectsBinding &binding,const Settings &settings):context{binding,settings} { effectContext=&context; }
    ~OwnerScope(){effectContext=previous;}
};
void __fastcall sfxRenderHook(void *effect,void *,void *device) {
    if(effectContext && hideSfx(effectContext->binding,effectContext->settings,effect))return;
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
struct Filter {
    Binding binding;
    void **original=nullptr;
    int count=0;
    std::array<void *,4096> visible{};
    Filter(Binding b,const Settings &s):binding(b) {
        if(!b || !s.limitShopSigns || !*b.player || *b.size<0 || *b.size>4096 || (*b.size&&!*b.data))return;
        auto *player=*b.player;
        const auto *eye=reinterpret_cast<const float *>(static_cast<unsigned char *>(player)+0x160);
        if(!std::isfinite(eye[0])||!std::isfinite(eye[1])||!std::isfinite(eye[2]))return;
        const double radius2=double(s.shopSignDistance)*s.shopSignDistance;
        size_t visibleCount=0;
        for(int i=0;i<*b.size;++i) {
            auto *text=(*b.data)[i];
            auto *object=text?field<void *>(text,0x78):nullptr;
            bool keep=true;
            if(object && object!=player) {
                const auto *p=reinterpret_cast<const float *>(static_cast<unsigned char *>(object)+0x160);
                const double x=double(p[0])-eye[0],y=double(p[1])-eye[1],z=double(p[2])-eye[2];
                const double distance=x*x+y*y+z*z;
                keep=!std::isfinite(distance)||distance<=radius2;
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
    native_patch::Set patches,effectPatches;
    std::recursive_mutex mutex;
    std::map<const Optimization *,Settings> owners;
    DWORD thread=0;
    bool installed=false,rendering=false,effectsInstalled=false,effectsRendering=false;
};
State &state() { static State value;return value; }
void __fastcall renderHook(void *dialog,void *,void *render2D) {
    auto &shared=state();
    Settings settings;
    bool filter=false;
    {
        std::lock_guard lock(shared.mutex);
        auto owner=std::find_if(shared.owners.begin(),shared.owners.end(),[](const auto &entry){return entry.second.limitShopSigns;});
        filter=!shared.rendering && shared.thread==GetCurrentThreadId()&&owner!=shared.owners.end();
        if(filter){shared.rendering=true;settings=owner->second;}
    }
    if(filter) {
        { Filter scope(shared.binding,settings);shared.binding.render(dialog,render2D); }
        std::lock_guard lock(shared.mutex);shared.rendering=false;
    } else shared.binding.render(dialog,render2D);
}
void __fastcall effectsHook(void *world,void *,void *font) {
    auto &shared=state();Settings settings;bool apply=false;
    {
        std::lock_guard lock(shared.mutex);
        auto owner=std::find_if(shared.owners.begin(),shared.owners.end(),[](const auto &entry){return entry.second.hideDecorativeEffects||entry.second.hideSkillEffects||entry.second.hidePetEffects;});
        apply=!shared.effectsRendering&&shared.thread==GetCurrentThreadId()&&owner!=shared.owners.end();
        if(apply){shared.effectsRendering=true;settings=owner->second;}
    }
    if(apply) {
        {EffectsScope scope(shared.effects,settings);OwnerScope owners(shared.effects,settings);shared.effects.render(world,font);}
        std::lock_guard lock(shared.mutex);shared.effectsRendering=false;
    } else shared.effects.render(world,font);
}
}
Optimization::~Optimization() { Settings disabled;update(disabled); }
bool Optimization::supported() { return bool(optimization_detail::state().binding); }
bool Optimization::effectsSupported() { return bool(optimization_detail::state().effects); }
bool Optimization::petEffectsSupported() { return optimization_detail::state().effects.owners(); }
void Optimization::update(const Settings &settings) {
    auto &s=optimization_detail::state();if(!s.binding&&!s.effects)return;
    std::lock_guard lock(s.mutex);
    if((settings.limitShopSigns&&s.binding)||((settings.hideDecorativeEffects||settings.hideSkillEffects)&&s.effects)||
       (settings.hidePetEffects&&s.effects.owners()))
        s.owners[this]=settings;
    else s.owners.erase(this);
    const bool shops=std::any_of(s.owners.begin(),s.owners.end(),[](const auto &entry){return entry.second.limitShopSigns;});
    const bool effects=std::any_of(s.owners.begin(),s.owners.end(),[](const auto &entry){return entry.second.hideDecorativeEffects||entry.second.hideSkillEffects||entry.second.hidePetEffects;});
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
        if(valid&&s.effectPatches.apply()){s.effectsInstalled=true;s.thread=GetCurrentThreadId();}
        else s.effectPatches.clear();
    }
    if(s.effectsInstalled&&!effects&&!s.effectsRendering){s.effectPatches.clear();s.effectsInstalled=false;}
}
