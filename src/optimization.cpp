#include "optimization.hpp"
#include "native_binding.hpp"
#include <map>

namespace optimization_detail {
template<class T> T &field(void *p,size_t offset) { return *reinterpret_cast<T *>(static_cast<unsigned char *>(p)+offset); }
using Render=void(__thiscall *)(void *,void *);
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
    native_patch::Set patches;
    std::recursive_mutex mutex;
    std::map<const Optimization *,Settings> owners;
    DWORD thread=0;
    bool installed=false,rendering=false;
};
State &state() { static State value;return value; }
void __fastcall renderHook(void *dialog,void *,void *render2D) {
    auto &shared=state();
    Settings settings;
    bool filter=false;
    {
        std::lock_guard lock(shared.mutex);
        filter=!shared.rendering && shared.thread==GetCurrentThreadId()&&!shared.owners.empty();
        if(filter){shared.rendering=true;settings=shared.owners.begin()->second;}
    }
    if(filter) {
        { Filter scope(shared.binding,settings);shared.binding.render(dialog,render2D); }
        std::lock_guard lock(shared.mutex);shared.rendering=false;
    } else shared.binding.render(dialog,render2D);
}
}
Optimization::~Optimization() { Settings disabled;update(disabled); }
bool Optimization::supported() { return bool(optimization_detail::state().binding); }
void Optimization::update(const Settings &settings) {
    auto &s=optimization_detail::state();if(!s.binding)return;
    std::lock_guard lock(s.mutex);
    if(settings.limitShopSigns) {
        s.owners[this]=settings;
        if(!s.installed) {
            unsigned char expected[5];std::memcpy(expected,s.binding.call,5);
            int32_t displacement=0;std::memcpy(&displacement,expected+1,4);
            const bool original=expected[0]==0xe8&&reinterpret_cast<uintptr_t>(s.binding.call)+5+displacement==reinterpret_cast<uintptr_t>(s.binding.render);
            if(original&&s.patches.call(s.binding.call,expected,reinterpret_cast<void *>(&optimization_detail::renderHook))&&s.patches.apply()) {
                s.installed=true;s.thread=GetCurrentThreadId();
            } else s.patches.clear();
        }
    } else s.owners.erase(this);
    if(s.installed&&s.owners.empty()&&!s.rendering){s.patches.clear();s.installed=false;}
}
