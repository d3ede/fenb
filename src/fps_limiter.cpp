#include "fps_limiter.hpp"
#include <bcrypt.h>
#include <mmsystem.h>
#include <array>
#include <cstring>
#include <cstdint>
#include <bit>

namespace {
static_assert(sizeof(void *) == 4, "The Neuz FrameMove adapter requires Windows x86.");
constexpr DWORD Timestamp = 1794424444, ImageSize = 11538432, EntryPoint = 5918976;
constexpr DWORD Flag = 0x829c4c, Tick = 0x844ae0, Elapsed = 0x844ae4;
constexpr DWORD TimeImport = 0x6d5c10, SleepImport = 0x6d5368;
constexpr DWORD Application = 0x866860, Vtable = 0x794bec, Move = 0xda330;
constexpr DWORD MoveEntry = Vtable + 5 * 4;
// SHA-256 of the inspected Homesickness Neuz revision, not a general Flyff signature.
constexpr unsigned char Digest[] = {0xce, 0x32, 0xe3, 0x50, 0xc6, 0x00, 0x24, 0x96, 0xac, 0xf9, 0xe8,
                                    0x77, 0x42, 0x75, 0x78, 0x49, 0x71, 0x18, 0x6e, 0x61, 0x44, 0x90,
                                    0x1f, 0x7c, 0x2c, 0x7d, 0x9b, 0x37, 0xa4, 0xaa, 0x41, 0xa1};

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
           nt.FileHeader.TimeDateStamp == Timestamp &&
           nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
           nt.OptionalHeader.SizeOfImage == ImageSize && nt.OptionalHeader.AddressOfEntryPoint == EntryPoint;
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
    return base <= UINT32_MAX - ImageSize &&
           Read(reinterpret_cast<unsigned char *>(image) + offset, &actual, sizeof(actual)) &&
           actual == base + target;
}
fps_detail::Binding Discover() {
    HMODULE image = GetModuleHandleW(nullptr);
    if (!Headers(image))
        return {};
    wchar_t path[32768]{};
    DWORD length = GetModuleFileNameW(image, path, DWORD(std::size(path)));
    if (!length || length >= std::size(path) || !fps_detail::KnownClientFile(path))
        return {};
    return fps_detail::InspectImage(image);
}

std::mutex hookMutex;
std::atomic<fps_detail::State *> hookedState = nullptr;
std::atomic<fps_detail::FrameMove> fallbackMove = nullptr;

HRESULT __fastcall MoveHook(void *application, void *) {
    auto *s = hookedState.load();
    auto original = fallbackMove.load();
    unsigned steps = 1;
    float *elapsed = nullptr;
    {
        std::lock_guard lock(hookMutex);
        if (s && s->owners && s->binding.application == application) {
            if (!s->thread)
                s->thread = GetCurrentThreadId();
        }
        if (s && s->owners && s->binding.application == application && s->thread == GetCurrentThreadId()) {
            original = s->binding.originalMove;
            if (*(static_cast<unsigned char *>(application) + 0xa8)) {
                steps = s->scheduler.advance(s->counter(), s->frequency);
                elapsed = s->binding.frameElapsed;
            } else {
                s->scheduler.reset(); // Preserve native paused/single-step calls.
            }
        }
    }
    if (!original)
        return E_FAIL;
    if (!elapsed)
        return original(application);
    // No lock across game code: FrameMove can receive packets and recreate DX9 devices.
    const float renderElapsed = *elapsed;
    HRESULT result = S_OK;
    for (unsigned i = 0; i < steps; ++i) {
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

fps_detail::Binding fps_detail::InspectImage(HMODULE image) {
    if (!Headers(image) || !Region(image, 0xebdcf, true) || !Region(image, 0xebea6, true) ||
        !Region(image, Flag, false) || !Region(image, Tick, false) || !Region(image, Elapsed, false) ||
        !Region(image, Application, false) || !Region(image, Application + 0xf4, false) ||
        !Region(image, Move, true) || !ReadOnlyData(image, MoveEntry) ||
        !Address(image, Application, Vtable) || !Address(image, MoveEntry, Move))
        return {};
    // Both frame gates must refer to the same flag, including ASLR relocations.
    const unsigned char compare[] = {0x83, 0x3d}, zero[] = {0}, clock[] = {0x8b, 0x3d};
    const unsigned char gate[] = {0x74, 0x36}, afterGate[] = {0x74, 0x73};
    const unsigned char wait[] = {0x83, 0xf9, 0x10, 0x73, 0x2d, 0xb8, 0x10, 0,
                                  0,    0,    0x2b, 0xc1, 0x50, 0xff, 0x15};
    const unsigned char moveStart[] = {0x53, 0x8b, 0xdc, 0x83, 0xec, 0x08,
                                       0x83, 0xe4, 0xf8, 0x83, 0xc4, 0x04};
    const unsigned char moveCall[] = {0xff, 0x50, 0x14}, renderCall[] = {0xff, 0x50, 0x18};
    if (!Bytes(image, 0xebdcf, compare, sizeof(compare)) || !Address(image, 0xebdd1, Flag) ||
        !Bytes(image, 0xebdd5, zero, 1) || !Bytes(image, 0xebdd6, clock, sizeof(clock)) ||
        !Address(image, 0xebdd8, TimeImport) || !Bytes(image, 0xebddc, gate, sizeof(gate)) ||
        !Bytes(image, 0xebe57, compare, sizeof(compare)) || !Address(image, 0xebe59, Flag) ||
        !Bytes(image, 0xebe5d, zero, 1) || !Bytes(image, 0xebe5e, afterGate, sizeof(afterGate)) ||
        !Bytes(image, 0xebe99, wait, sizeof(wait)) || !Address(image, 0xebea8, SleepImport) ||
        !Bytes(image, Move, moveStart, sizeof(moveStart)) ||
        !Bytes(image, 0xebdc1, moveCall, sizeof(moveCall)) ||
        !Bytes(image, 0xebe23, renderCall, sizeof(renderCall)))
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

bool fps_detail::KnownClientFile(const wchar_t *path) {
    HANDLE file =
        CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart != 9274368) {
        CloseHandle(file);
        return false;
    }
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    std::array<unsigned char, 65536> buffer{};
    unsigned char digest[32]{};
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
              BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    DWORD count = 0;
    size_t total = 0;
    while (ok) {
        if (!ReadFile(file, buffer.data(), DWORD(buffer.size()), &count, nullptr)) {
            ok = false;
            break;
        }
        if (!count)
            break;
        total += count;
        if (total > 9274368) {
            ok = false;
            break;
        }
        ok = BCryptHashData(hash, buffer.data(), count, 0) >= 0;
    }
    if (ok)
        ok =
            BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0 && !memcmp(digest, Digest, sizeof(Digest));
    if (hash)
        BCryptDestroyHash(hash);
    if (algorithm)
        BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    return ok;
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
                s.thread = 0;
                hookedState.store(&s);
                Log("FPS unlock: fixed 16 ms simulation gate installed; render limiter released.");
            }
            ++s.owners;
            active_ = true;
        }
        if (!*(static_cast<unsigned char *>(b.application) + 0xa8))
            s.scheduler.reset();
        if (*b.frameMove == hook && *b.enabled != 0)
            InterlockedExchange(b.enabled, 0);
    } else {
        active_ = false;
        if (!--s.owners) {
            // Restore native pacing BEFORE removing the simulation gate.
            *b.lastTick = timeGetTime();
            *b.elapsed = 0;
            InterlockedExchange(b.enabled, s.originalFlag);
            ReplaceMove(b, hook, b.originalMove);
            hookedState.store(nullptr);
            s.scheduler.reset();
            Log("FPS unlock: native simulation and render pacing restored.");
        }
    }
}
