#include "environment.hpp"
#include "fps_limiter.hpp"
#include <bit>
#include <cstring>
#include <map>
#include <optional>

namespace {
constexpr size_t Mode = 0x20, Texture = 0x40, Particles = 0xa4, Count = 0x2b024;
template <class T> T &Field(unsigned char *p, size_t offset) {
    return *reinterpret_cast<T *>(p + offset);
}
bool Region(HMODULE image, DWORD offset, size_t bytes, bool executable = false) {
    auto start = reinterpret_cast<uintptr_t>(image) + offset;
    auto end = start + bytes;
    if (end < start)
        return false;
    // SEC_IMAGE data can span private writable pages and untouched copy-on-write pages.
    // Validate every region: a single VirtualQuery extent need not contain the whole object.
    for (auto cursor = start; cursor < end;) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void *>(cursor), &info, sizeof(info)) ||
            info.AllocationBase != image || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return false;
        bool accessible = executable
                              ? info.Protect == PAGE_EXECUTE_READ || info.Protect == PAGE_EXECUTE_READWRITE
                              : info.Protect == PAGE_READWRITE || info.Protect == PAGE_WRITECOPY;
        auto next = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (!accessible || next <= cursor)
            return false;
        cursor = next;
    }
    return true;
}
template <size_t N> bool Bytes(HMODULE image, DWORD offset, const unsigned char (&expected)[N]) {
    return Region(image, offset, N, true) &&
           !memcmp(reinterpret_cast<unsigned char *>(image) + offset, expected, N);
}
environment_detail::Binding Discover() {
    auto image = GetModuleHandleW(nullptr);
    return environment_detail::InspectImage(image, client_detail::Current());
}
struct Shared {
    std::recursive_mutex mutex;
    environment_detail::Binding binding = Discover();
    environment_detail::FrameState frame;
    std::map<const Environment *, Settings> owners;
    bool installed = false;
    bool rendering = false;
};
Shared &State() {
    static Shared state;
    return state;
}
std::atomic<environment_detail::Render> fallback = nullptr;
bool Replace(environment_detail::Binding &b, environment_detail::Render from, environment_detail::Render to) {
    DWORD protection = 0, ignored = 0;
    if (!VirtualProtect(b.entry, sizeof(*b.entry), PAGE_READWRITE, &protection))
        return false;
    auto previous =
        InterlockedCompareExchangePointer(reinterpret_cast<void *volatile *>(b.entry),
                                          reinterpret_cast<void *>(to), reinterpret_cast<void *>(from));
    VirtualProtect(b.entry, sizeof(*b.entry), protection, &ignored);
    return previous == reinterpret_cast<void *>(from);
}
HRESULT __fastcall RenderHook(void *application, void *) {
    auto original = fallback.load();
    if (!original)
        return E_FAIL;
    auto &state = State();
    std::optional<environment_detail::FrameScope> scope;
    {
        std::lock_guard lock(state.mutex);
        if (!state.rendering && !state.owners.empty() && application == state.binding.application) {
            state.rendering = true;
            // Snapshot settings: the panel can edit them during this very Render call.
            const auto settings = state.owners.begin()->second;
            scope.emplace(state.binding, state.frame, settings, GetTickCount64());
        }
    }
    // No environment lock across game code: device recovery can lock Runtime before calling us.
    HRESULT result = original(application);
    if (scope) {
        std::lock_guard lock(state.mutex);
        scope.reset();
        if (state.owners.empty())
            state.frame.reset();
        state.rendering = false;
    }
    return result;
}
} // namespace

double environment_detail::Timeline::sample(float minutes, bool freeze, ULONGLONG now) {
    if (selected != minutes) {
        seconds = minutes * 60.0;
        selected = minutes;
    } else if (!frozen) {
        // Neuz advances one game second per 10 ms. Freeze never pauses simulation.
        seconds = std::fmod(seconds + double(now - tick) * .1, 86400.0);
    }
    tick = now;
    frozen = freeze;
    return seconds;
}
void environment_detail::Timeline::reset() {
    *this = Timeline{};
}
void environment_detail::FrameState::reset() {
    time.reset();
    Drop(texture.p);
    player = nullptr;
    mode = -1;
    capacity = 0;
    weatherTick = 0;
}

environment_detail::Binding environment_detail::InspectImage(HMODULE image, const client_detail::Layout &layout) {
    const auto l = layout.environment() ? layout : client_detail::Discover(image);
    if (!l.environment()) return {};
    const DWORD ClockRva=l.clock, SkyRva=l.sky, PlayerRva=l.player, RenderRva=l.render, RenderEntry=l.vtable+24;
    if (!image)
        return {};
    auto *base = reinterpret_cast<unsigned char *>(image);
    IMAGE_DOS_HEADER dos{};
    SIZE_T read = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), base, &dos, sizeof(dos), &read) || read != sizeof(dos) ||
        dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 || dos.e_lfanew > 4096)
        return {};
    IMAGE_NT_HEADERS32 nt{};
    if (!ReadProcessMemory(GetCurrentProcess(), base + dos.e_lfanew, &nt, sizeof(nt), &read) ||
        read != sizeof(nt) || nt.Signature != IMAGE_NT_SIGNATURE ||
        nt.FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt.OptionalHeader.SizeOfImage < 4096 || nt.OptionalHeader.SizeOfImage > 128*1024*1024 ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        return {};
    MEMORY_BASIC_INFORMATION entry{};
    if (!VirtualQuery(base + RenderEntry, &entry, sizeof(entry)) || entry.AllocationBase != image ||
        entry.State != MEM_COMMIT || entry.Protect != PAGE_READONLY ||
        !Region(image, ClockRva, sizeof(Clock)) || !Region(image, SkyRva, 0x2b03c) ||
        !Region(image, PlayerRva, sizeof(void *)) || !Region(image, l.weather, sizeof(int)) ||
        !Region(image, l.application, sizeof(void *)))
        return {};
    const unsigned char render[] = {0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x83, 0xec, 0x20};
    const unsigned char init[] = {0x53, 0x8b, 0xdc, 0x83, 0xec, 0x08};
    const unsigned char compute[] = {0x53, 0x8b, 0xd9, 0x56, 0x57, 0x83, 0x7b, 0x10, 0x00};
    if (!Bytes(image, RenderRva, render) || !Bytes(image, l.initFall, init) ||
        !Bytes(image, l.compute, compute) ||
        *reinterpret_cast<Render *>(base + RenderEntry) != reinterpret_cast<Render>(base + RenderRva) ||
        *reinterpret_cast<void **>(base + l.application) != base + l.vtable)
        return {};
    return {base + l.application,
            reinterpret_cast<Render *>(base + RenderEntry),
            reinterpret_cast<Render>(base + RenderRva),
            reinterpret_cast<Clock *>(base + ClockRva),
            base + SkyRva,
            reinterpret_cast<int *>(base + l.weather),
            reinterpret_cast<void **>(base + PlayerRva),
            reinterpret_cast<InitFall>(base + l.initFall)};
}

bool environment_detail::PlayerPosition(const Binding &binding,float (&position)[3],uint64_t *worldKey) {
    void *player=nullptr;SIZE_T read=0;
    if(!binding.player||!ReadProcessMemory(GetCurrentProcess(),binding.player,&player,sizeof(player),&read)||
        read!=sizeof(player)||!player)return false;
    float sample[3]{};
    if(!ReadProcessMemory(GetCurrentProcess(),static_cast<unsigned char *>(player)+0x160,sample,sizeof(sample),&read)||
        read!=sizeof(sample))return false;
    for(float v:sample)if(!std::isfinite(v)||std::abs(v)>1e7f)return false;
    if(worldKey){
        *worldKey=0;void *world=nullptr;int id=0;
        if(ReadProcessMemory(GetCurrentProcess(),static_cast<unsigned char *>(player)+0x16c,&world,sizeof(world),&read)&&
            read==sizeof(world)&&world&&ReadProcessMemory(GetCurrentProcess(),world,&id,sizeof(id),&read)&&read==sizeof(id))
            *worldKey=(uint64_t(reinterpret_cast<uintptr_t>(world))<<32)|uint32_t(id);
    }
    memcpy(position,sample,sizeof(sample));return true;
}
bool Environment::playerPosition(float (&position)[3],uint64_t *worldKey) const {
    auto &s=State();std::lock_guard lock(s.mutex);
    return environment_detail::PlayerPosition(s.binding,position,worldKey);
}
environment_detail::FrameScope::FrameScope(const Binding &binding, FrameState &state, const Settings &s,
                                           ULONGLONG now)
    : b_(binding), state_(state) {
    if (!b_)
        return;
    if (s.timeOverride) {
        memcpy(clock_.data(), &b_.clock->fixed, sizeof(clock_));
        int seconds = int(state_.time.sample(s.timeMinutes, s.freezeTime, now));
        b_.clock->fixed = TRUE;
        b_.clock->hour = seconds / 3600; // The renderer accepts midnight as 0, and the native clock's 24.
        b_.clock->minute = seconds / 60 % 60;
        b_.clock->second = seconds % 60;
        time_ = true;
    } else
        state_.time.reset();
    if (!s.enabled || !s.weatherOverride || !*b_.player || !Field<void *>(b_.sky, 0xa0))
        return;
    float center[3]{};
    SIZE_T read = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), static_cast<unsigned char *>(*b_.player) + 0x160, center,
                           sizeof(center), &read) ||
        read != sizeof(center) || !std::isfinite(center[0]) || !std::isfinite(center[1]) ||
        !std::isfinite(center[2]))
        return;
    // Never change COM ownership or game particle history outside the render scope.
    mode_ = Field<int>(b_.sky, Mode);
    count_ = Field<int>(b_.sky, Count);
    option_ = *b_.weatherOption;
    texture_ = Field<IDirect3DTexture9 *>(b_.sky, Texture);
    weather_ = true;
    memcpy(state_.savedParticles.data(), b_.sky + Particles, ParticleBytes);
    Field<IDirect3DTexture9 *>(b_.sky, Texture) = state_.texture.p;
    state_.texture.p = nullptr;
    int mode = s.weatherType == 1 ? 2 : s.weatherType == 2 ? 3 : 0;
    Field<int>(b_.sky, Mode) = mode;
    *b_.weatherOption = TRUE;
    if (mode && (state_.mode != mode || state_.player != *b_.player)) {
        // InitFall loads the native rain/snow artwork into our separately owned texture.
        Field<int>(b_.sky, Count) = count_ > 0 ? std::min(count_, 400) : 400;
        b_.initFall(b_.sky);
        state_.capacity = std::clamp(Field<int>(b_.sky, Count), 0, 400);
        memcpy(state_.particles.data(), b_.sky + Particles, ParticleBytes);
        state_.weatherTick = now;
    }
    if (mode && !Field<IDirect3DTexture9 *>(b_.sky, Texture)) {
        state_.mode = -1; // A failed texture load leaves the native scene intact; retry next frame.
        restoreWeather();
        return;
    }
    if (mode) {
        auto *velocity = reinterpret_cast<float *>(state_.particles.data());
        auto *position = velocity + 6000;
        float steps = float(std::min<ULONGLONG>(now - state_.weatherTick, 128)) / 16.f;
        for (int i = 0; i < state_.capacity; ++i) {
            for (int axis = 0; axis < 3; ++axis)
                position[i * 3 + axis] += velocity[i * 3 + axis] * steps;
            // Wrap around the player without allocating objects or calling world simulation.
            for (int axis : {0, 2}) {
                float &v = position[i * 3 + axis];
                v = center[axis] + std::remainder(v - center[axis], 25.f);
            }
            float &y = position[i * 3 + 1];
            if (y < center[1] - 2 || y > center[1] + 20)
                y = center[1] + 10 + float(i % 101) * .1f;
        }
        memcpy(b_.sky + Particles, state_.particles.data(), ParticleBytes);
        Field<int>(b_.sky, Count) = int(std::round(state_.capacity * s.weatherIntensity / 100.f));
    } else
        Field<int>(b_.sky, Count) = 0;
    state_.mode = mode;
    state_.player = *b_.player;
    state_.weatherTick = now;
}
void environment_detail::FrameScope::restoreWeather() {
    if (weather_) {
        state_.texture.p = Field<IDirect3DTexture9 *>(b_.sky, Texture);
        Field<IDirect3DTexture9 *>(b_.sky, Texture) = texture_;
        memcpy(b_.sky + Particles, state_.savedParticles.data(), ParticleBytes);
        Field<int>(b_.sky, Mode) = mode_;
        Field<int>(b_.sky, Count) = count_;
        *b_.weatherOption = option_;
        weather_ = false;
    }
}
environment_detail::FrameScope::~FrameScope() {
    restoreWeather();
    if (time_)
        memcpy(&b_.clock->fixed, clock_.data(), sizeof(clock_));
}

Environment::~Environment() {
    Settings s;
    s.enabled = false;
    update(s);
}
bool Environment::supported() {
    return bool(State().binding);
}
void Environment::update(const Settings &settings, bool compare) {
    auto &s = State();
    if (!s.binding)
        return;
    std::lock_guard lock(s.mutex);
    auto hook = std::bit_cast<environment_detail::Render>(&RenderHook);
    // The world clock is independent of the image-effects master and comparison mode.
    Settings effective = settings;
    effective.weatherOverride = effective.weatherOverride && settings.enabled && !compare;
    if (effective.timeOverride || effective.weatherOverride) {
        s.owners[this] = effective;
        if (!s.installed) {
            fallback = s.binding.render;
            s.installed = Replace(s.binding, s.binding.render, hook);
        }
    } else
        s.owners.erase(this);
    if (s.installed && s.owners.empty() && Replace(s.binding, hook, s.binding.render)) {
        s.installed = false;
        s.frame.reset();
    }
}
void Environment::reset() {
    auto &s = State();
    std::lock_guard lock(s.mutex);
    s.frame.reset();
}
float Environment::minutes(const Settings &settings) const {
    auto &s = State();
    std::lock_guard lock(s.mutex);
    return settings.timeOverride && s.frame.time.selected == settings.timeMinutes
               ? float(s.frame.time.seconds / 60.0)
               : settings.timeMinutes;
}
