#include "fps_limiter.hpp"
#include "client_layout.hpp"
#include "visual_animation.hpp"
#include "visual_camera.hpp"
#include "presentation_clock.hpp"
#include "native_hybrid.hpp"
#include <mmsystem.h>
#include <array>
#include <cstring>
#include <cstdint>
#include <bit>

namespace {
static_assert(sizeof(void *) == 4, "The Neuz FrameMove adapter requires Windows x86.");

bool Read(const void *address, void *out, size_t size) {
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), address, out, size, &read) && read == size;
}
bool Region(HMODULE image, DWORD offset, bool executable) {
    MEMORY_BASIC_INFORMATION info{};
    auto *p = reinterpret_cast<unsigned char *>(image) + offset;
    if (!VirtualQuery(p, &info, sizeof(info)) || info.AllocationBase != image || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
        return false;
    const DWORD protection = info.Protect & 0xff;
    return executable ? protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE
                      : protection == PAGE_READWRITE;
}
bool ReadOnlyData(HMODULE image, DWORD offset) {
    MEMORY_BASIC_INFORMATION info{};
    return VirtualQuery(reinterpret_cast<unsigned char *>(image) + offset, &info, sizeof(info)) &&
           info.AllocationBase == image && info.State == MEM_COMMIT && info.Protect == PAGE_READONLY;
}
bool Headers(HMODULE image) {
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS32 nt{};
    return image && Read(image, &dos, sizeof(dos)) && dos.e_magic == IMAGE_DOS_SIGNATURE &&
           dos.e_lfanew > 0 && dos.e_lfanew < 4096 &&
           Read(reinterpret_cast<unsigned char *>(image) + dos.e_lfanew, &nt, sizeof(nt)) &&
           nt.Signature == IMAGE_NT_SIGNATURE && nt.FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
           nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
           nt.OptionalHeader.SizeOfImage >= 4096 && nt.OptionalHeader.SizeOfImage <= 128*1024*1024;
}
bool Bytes(HMODULE image, DWORD offset, const unsigned char *expected, size_t count) {
    std::array<unsigned char, 32> actual{};
    return count <= actual.size() &&
           Read(reinterpret_cast<unsigned char *>(image) + offset, actual.data(), count) &&
           !memcmp(actual.data(), expected, count);
}
bool Address(HMODULE image, DWORD offset, DWORD target) {
    DWORD actual = 0;
    const auto base = reinterpret_cast<uintptr_t>(image);
    return base <= UINT32_MAX - target &&
           Read(reinterpret_cast<unsigned char *>(image) + offset, &actual, sizeof(actual)) &&
           actual == base + target;
}
fps_detail::Binding Discover() {
    HMODULE image = GetModuleHandleW(nullptr);
    if (!Headers(image))
        return {};
    return fps_detail::InspectImage(image, client_detail::Current());
}

std::mutex hookMutex;
std::atomic<fps_detail::State *> hookedState = nullptr;
std::atomic<fps_detail::FrameMove> fallbackMove = nullptr;

HRESULT __fastcall MoveHook(void *application, void *) {
    auto *s = hookedState.load();
    auto original = fallbackMove.load();
    unsigned steps = 1;
    float *elapsed = nullptr;
    bool resetPresentation=false;
    bool nativeFrame=false;
    int64_t nativeNow=0;
    {
        std::lock_guard lock(hookMutex);
        if (s && s->owners && s->binding.application == application) {
            if (!s->thread)
                s->thread = GetCurrentThreadId();
        }
        if (s && s->owners && s->binding.application == application && s->thread == GetCurrentThreadId()) {
            original = s->binding.originalMove;
            if (*(static_cast<unsigned char *>(application) + 0xa8)) {
                const auto now=s->counter();
                if(s->nativeHybrid && native_hybrid::installed) {
                    nativeFrame=true;nativeNow=now;elapsed=s->binding.frameElapsed;
                } else {
                    steps = s->scheduler.advance(now, s->frequency);
                    visual_animation::renderPhase = s->scheduler.fraction(s->frequency);
                    presentation_clock::sample(now,s->frequency,visual_animation::renderPhase,s->scheduler.discontinuous());
                    if(presentation_clock::frame.discontinuity) visual_animation::renderPhase=0;
                    resetPresentation=presentation_clock::frame.discontinuity;
                    elapsed = s->binding.frameElapsed;
                }
            } else {
                s->scheduler.reset(); // Preserve native paused/single-step calls.
                presentation_clock::reset();
                visual_animation::renderPhase=0;
                resetPresentation=true;
            }
        }
    }
    if(resetPresentation) {
        visual_camera::resetTimeline();
        visual_animation::resetTimeline();
    }
    if (!original)
        return E_FAIL;
    if (!elapsed) {
        if(s && s->nativeHybrid) native_hybrid::reset();
        visual_camera::finishFrame();
        visual_animation::finishFrame();
        return original(application);
    }
    if(nativeFrame) return native_hybrid::run(application,elapsed,nativeNow,s->frequency,original,
        [&]() { return hookedState.load()==s; });
    // No lock across game code: FrameMove can receive packets and recreate DX9 devices.
    const float renderElapsed = *elapsed;
    HRESULT result = S_OK;
    for (unsigned i = 0; i < steps; ++i) {
        visual_camera::finishFrame();
        visual_animation::finishFrame();
        visual_animation::beginTick();
        *elapsed = .016f;
        result = original(application);
        if (FAILED(result) || hookedState.load() != s)
            break;
    }
    *elapsed = renderElapsed;
    return result;
}

bool ReplaceMove(fps_detail::Binding &b, fps_detail::FrameMove expected, fps_detail::FrameMove replacement) {
    DWORD protection = 0, ignored = 0;
    if (!VirtualProtect(b.frameMove, sizeof(*b.frameMove), PAGE_READWRITE, &protection))
        return false;
    auto previous = InterlockedCompareExchangePointer(reinterpret_cast<void *volatile *>(b.frameMove),
                                                      reinterpret_cast<void *>(replacement),
                                                      reinterpret_cast<void *>(expected));
    if (!VirtualProtect(b.frameMove, sizeof(*b.frameMove), protection, &ignored))
        Log("FPS unlock: could not restore vtable page protection.");
    return previous == reinterpret_cast<void *>(expected);
}
} // namespace

fps_detail::Binding fps_detail::InspectImage(HMODULE image, const client_detail::Layout &layout) {
    const auto l = layout.frames() ? layout : client_detail::Discover(image);
    if (!l.frames()) return {};
    const DWORD Flag=l.flag, Tick=l.tick, Elapsed=l.elapsed, TimeImport=l.timeImport, SleepImport=l.sleepImport;
    const DWORD Application=l.application, Vtable=l.vtable, Move=l.move, MoveEntry=Vtable+20;
    const DWORD gate=l.gate;
    if (!Headers(image) || !Region(image, gate, true) || !Region(image, gate+215, true) ||
        !Region(image, Flag, false) || !Region(image, Tick, false) || !Region(image, Elapsed, false) ||
        !Region(image, Application, false) || !Region(image, Application + 0xf4, false) ||
        !Region(image, Move, true) || !ReadOnlyData(image, MoveEntry) ||
        !Address(image, Application, Vtable) || !Address(image, MoveEntry, Move))
        return {};
    // Both frame gates must refer to the same flag, including ASLR relocations.
    const unsigned char compare[] = {0x83, 0x3d}, zero[] = {0}, clock[] = {0x8b, 0x3d};
    const unsigned char gateJump[] = {0x74, 0x36}, afterGate[] = {0x74, 0x73};
    const unsigned char wait[] = {0x83, 0xf9, 0x10, 0x73, 0x2d, 0xb8, 0x10, 0,
                                  0,    0,    0x2b, 0xc1, 0x50, 0xff, 0x15};
    const unsigned char moveStart[] = {0x53, 0x8b, 0xdc, 0x83, 0xec, 0x08,
                                       0x83, 0xe4, 0xf8, 0x83, 0xc4, 0x04};
    const unsigned char moveCall[] = {0xff, 0x50, 0x14}, renderCall[] = {0xff, 0x50, 0x18};
    if (!Bytes(image, gate, compare, sizeof(compare)) || !Address(image, gate+2, Flag) ||
        !Bytes(image, gate+6, zero, 1) || !Bytes(image, gate+7, clock, sizeof(clock)) ||
        !Address(image, gate+9, TimeImport) || !Bytes(image, gate+13, gateJump, sizeof(gateJump)) ||
        !Bytes(image, gate+136, compare, sizeof(compare)) || !Address(image, gate+138, Flag) ||
        !Bytes(image, gate+142, zero, 1) || !Bytes(image, gate+143, afterGate, sizeof(afterGate)) ||
        !Bytes(image, gate+202, wait, sizeof(wait)) || !Address(image, gate+217, SleepImport) ||
        !Bytes(image, Move, moveStart, sizeof(moveStart)) ||
        !Bytes(image, gate-14, moveCall, sizeof(moveCall)) ||
        !Bytes(image, gate+84, renderCall, sizeof(renderCall)))
        return {};
    auto *base = reinterpret_cast<unsigned char *>(image);
    LONG enabled = 0;
    if (!Read(base + Flag, &enabled, sizeof(enabled)) || (enabled != 0 && enabled != 1))
        return {};
    return {reinterpret_cast<LONG *>(base + Flag),
            reinterpret_cast<DWORD *>(base + Tick),
            reinterpret_cast<LONG *>(base + Elapsed),
            base + Application,
            reinterpret_cast<FrameMove *>(base + MoveEntry),
            reinterpret_cast<FrameMove>(base + Move),
            reinterpret_cast<float *>(base + Application + 0xf4)};
}

int64_t fps_detail::ReadCounter() {
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}
fps_detail::State::State(Binding value) : binding(value) {
    LARGE_INTEGER f{};
    if (QueryPerformanceFrequency(&f))
        frequency = f.QuadPart;
}
fps_detail::State &FpsLimiter::state() {
    if (!state_) {
        static fps_detail::State shared(Discover());
        state_ = &shared;
    }
    return *state_;
}
bool FpsLimiter::supported() {
    return bool(state().binding) && state().frequency > 0;
}
FpsLimiter::~FpsLimiter() {
    update(false);
}
void FpsLimiter::update(bool unlock) {
    if (!unlock && !active_)
        return;
    auto &s = state();
    if (!s.binding || s.frequency <= 0)
        return;
    std::lock_guard lock(hookMutex);
    auto &b = s.binding;
    // x86 thiscall supplies ECX; the fastcall adapter ignores EDX and consumes no stack arguments.
    const auto hook = std::bit_cast<fps_detail::FrameMove>(&MoveHook);
    if (unlock) {
        if (!active_) {
            if (!s.owners) {
                if (hookedState.load() && hookedState.load() != &s)
                    return;
                fallbackMove.store(b.originalMove);
                // Install the simulation gate BEFORE permitting additional render frames.
                if (!ReplaceMove(b, b.originalMove, hook))
                    return;
                s.originalFlag = *b.enabled;
                s.scheduler.reset();
                presentation_clock::reset();
                s.thread = 0;
                hookedState.store(&s);
                s.nativeHybrid=native_hybrid::install();
                if(!s.nativeHybrid) {
                    // Actor transforms stay native: prediction breaks shadows, picking and SFX origins.
                    visual_animation::install();
                    visual_camera::install();
                }
                Log(s.nativeHybrid ? "FPS unlock: native hybrid delta-time loop installed; render limiter released."
                    : "FPS unlock: native hybrid unavailable; fixed 16 ms simulation with native actor positions installed.");
            }
            ++s.owners;
            active_ = true;
        }
        if (!*(static_cast<unsigned char *>(b.application) + 0xa8)) {
            s.scheduler.reset();
            presentation_clock::reset();
        }
        if (*b.frameMove == hook && *b.enabled != 0)
            InterlockedExchange(b.enabled, 0);
    } else {
        active_ = false;
        if (!--s.owners) {
            // Restore native pacing BEFORE removing the simulation gate.
            visual_camera::uninstall();
            visual_animation::uninstall();
            if(s.nativeHybrid) { native_hybrid::uninstall();s.nativeHybrid=false; }
            *b.lastTick = timeGetTime();
            *b.elapsed = 0;
            InterlockedExchange(b.enabled, s.originalFlag);
            ReplaceMove(b, hook, b.originalMove);
            hookedState.store(nullptr);
            s.scheduler.reset();
            presentation_clock::reset();
            Log("FPS unlock: native simulation and render pacing restored.");
        }
    }
}
