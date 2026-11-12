#include "client_fixture.hpp"
#include "fps_limiter.hpp"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <bit>

void Log(const char *) {}
namespace {
int64_t counter = 0;
unsigned moves = 0;
bool badStep = false, failMove = false;
FpsLimiter *disableDuringMove = nullptr;
double distance = 0;
int64_t Counter() {
    return counter;
}
HRESULT __fastcall Simulate(void *application, void *) {
    float dt = *reinterpret_cast<float *>(static_cast<unsigned char *>(application) + 0xf4);
    badStep |= std::abs(dt - .016f) > .000001f;
    ++moves;
    distance += dt * 5;
    if (disableDuringMove)
        disableDuringMove->update(false);
    return failMove ? E_FAIL : S_OK;
}
} // namespace
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            printf("FAIL %d: %s\n", __LINE__, #x);                                                           \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)

int main() {
    for (int fps : {30, 60, 120, 144, 240, 500, 1000}) {
        FixedStep clock;
        unsigned ticks = 0;
        for (int i = 0; i <= fps * 10; ++i)
            ticks += clock.advance(int64_t(i) * 1000000 / fps, 1000000);
        CHECK(ticks == 626); // One initial tick plus 625 ticks in ten real seconds.
    }
    FixedStep clock;
    CHECK(clock.advance(0, 1000000) == 1);
    CHECK(clock.advance(15999, 1000000) == 0);
    CHECK(clock.advance(16000, 1000000) == 1);
    CHECK(clock.advance(216000, 1000000) == 8);  // Bounded catch-up.
    CHECK(clock.advance(1216000, 1000000) == 1); // No replay after long stalls.
    CHECK(clock.advance(0, 1000000) == 1);       // Clock discontinuity starts a fresh cadence.
    CHECK(clock.advance(0, 0) == 0);
    // Disposable mapped image: exercise relocated x86 operands without writing to a game.
    constexpr DWORD size = 11538432;
    auto *image =
        static_cast<unsigned char *>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    CHECK(image);
    auto module = reinterpret_cast<HMODULE>(image);
    auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(image);
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto *nt = reinterpret_cast<IMAGE_NT_HEADERS32 *>(image + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.TimeDateStamp = 1794424444;
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.SizeOfImage = size;
    nt->OptionalHeader.AddressOfEntryPoint = 5918976;
    auto pointer = [&](DWORD offset, DWORD target) {
        DWORD relocated = DWORD(reinterpret_cast<uintptr_t>(image)) + target;
        memcpy(image + offset, &relocated, 4);
    };
    image[0xebdcf] = image[0xebe57] = 0x83;
    image[0xebdd0] = image[0xebe58] = 0x3d;
    pointer(0xebdd1, 0x829c4c);
    pointer(0xebe59, 0x829c4c);
    image[0xebdd6] = 0x8b;
    image[0xebdd7] = 0x3d;
    pointer(0xebdd8, 0x6d5c10);
    image[0xebddc] = image[0xebe5e] = 0x74;
    image[0xebddd] = 0x36;
    image[0xebe5f] = 0x73;
    const unsigned char wait[] = {0x83, 0xf9, 0x10, 0x73, 0x2d, 0xb8, 0x10, 0,
                                  0,    0,    0x2b, 0xc1, 0x50, 0xff, 0x15};
    memcpy(image + 0xebe99, wait, sizeof(wait));
    pointer(0xebea8, 0x6d5368);
    pointer(0x866860, 0x794bec);
    pointer(0x794c00, 0xda330);
    const unsigned char moveStart[] = {0x53, 0x8b, 0xdc, 0x83, 0xec, 0x08,
                                       0x83, 0xe4, 0xf8, 0x83, 0xc4, 0x04};
    memcpy(image + 0xda330, moveStart, sizeof(moveStart));
    const unsigned char moveCall[] = {0xff, 0x50, 0x14}, renderCall[] = {0xff, 0x50, 0x18};
    memcpy(image + 0xebdc1, moveCall, sizeof(moveCall));
    memcpy(image + 0xebe23, renderCall, sizeof(renderCall));
    auto *flag = reinterpret_cast<LONG *>(image + 0x829c4c);
    *flag = 1;
    DWORD previous = 0;
    CHECK(VirtualProtect(image + 0xeb000, 0x1000, PAGE_EXECUTE_READ, &previous));
    CHECK(VirtualProtect(image + 0xda000, 0x1000, PAGE_EXECUTE_READ, &previous));
    CHECK(VirtualProtect(image + 0x794000, 0x1000, PAGE_READONLY, &previous));
    auto binding = fps_detail::InspectImage(module, client_detail::Reference);
    CHECK(binding && binding.enabled == flag);
    CHECK(*flag == 1); // Detection is strictly read-only.
    ++nt->FileHeader.TimeDateStamp;
    CHECK(fps_detail::InspectImage(module, client_detail::Reference)); // Metadata is not an ABI change.
    --nt->FileHeader.TimeDateStamp;
    CHECK(VirtualProtect(image + 0xeb000, 0x1000, PAGE_READWRITE, &previous));
    CHECK(!fps_detail::InspectImage(module, client_detail::Reference)); // Code must belong to executable image memory.
    image[0xebe9b] = 0x11;
    CHECK(VirtualProtect(image + 0xeb000, 0x1000, PAGE_EXECUTE_READ, &previous));
    CHECK(!fps_detail::InspectImage(module, client_detail::Reference)); // Changed frame interval is unsupported.
    CHECK(VirtualProtect(image + 0xeb000, 0x1000, PAGE_READWRITE, &previous));
    image[0xebe9b] = 0x10;
    pointer(0xebe59, 0x829c50);
    CHECK(VirtualProtect(image + 0xeb000, 0x1000, PAGE_EXECUTE_READ, &previous));
    CHECK(!fps_detail::InspectImage(module, client_detail::Reference)); // Both gates must reference the same verified flag.
    CHECK(VirtualProtect(image + 0xeb000, 0x1000, PAGE_READWRITE, &previous));
    pointer(0xebe59, 0x829c4c);
    CHECK(VirtualProtect(image + 0xeb000, 0x1000, PAGE_EXECUTE_READ, &previous));
    *flag = 2;
    CHECK(!fps_detail::InspectImage(module, client_detail::Reference));
    *flag = 1;
    CHECK(VirtualProtect(image + 0x829000, 0x1000, PAGE_READONLY, &previous));
    CHECK(!fps_detail::InspectImage(module, client_detail::Reference));
    CHECK(VirtualProtect(image + 0x829000, 0x1000, PAGE_READWRITE, &previous));
    CHECK(fps_detail::InspectImage(module, client_detail::Reference));

    // Invoke the actual x86 hook through a read-only vtable, using a disposable simulation.
    binding.originalMove = std::bit_cast<fps_detail::FrameMove>(&Simulate);
    image[0x866860 + 0xa8] = 1;
    CHECK(VirtualProtect(binding.frameMove, 4, PAGE_READWRITE, &previous));
    *binding.frameMove = binding.originalMove;
    CHECK(VirtualProtect(binding.frameMove, 4, PAGE_READONLY, &previous));
    fps_detail::State state(binding);
    state.frequency = 1000000;
    state.counter = Counter;
    for (int fps : {30, 60, 144, 500, 1000}) {
        moves = 0;
        distance = 0;
        badStep = false;
        counter = 0;
        FpsLimiter limiter(state);
        CHECK(limiter.supported());
        limiter.update(true);
        CHECK(limiter.active() && *flag == 0 && *binding.frameMove != binding.originalMove);
        MEMORY_BASIC_INFORMATION protection{};
        CHECK(VirtualQuery(binding.frameMove, &protection, sizeof(protection)));
        CHECK(protection.Protect == PAGE_READONLY);
        for (int i = 0; i <= fps * 10; ++i) {
            counter = int64_t(i) * 1000000 / fps;
            *binding.frameElapsed = 1.f / fps;
            CHECK(SUCCEEDED((*binding.frameMove)(binding.application)));
            CHECK(*binding.frameElapsed == 1.f / fps); // Render delta is not replaced by the simulation step.
        }
        CHECK(moves == 626 && !badStep);
        CHECK(std::abs(distance - 626 * .016 * 5) < .0001);
        printf("PASS: %d render FPS, %u simulation ticks / 10s, distance=%.3f\n", fps, moves, distance);
        limiter.update(false);
        CHECK(*flag == 1 && *binding.frameMove == binding.originalMove);
    }
    {
        FpsLimiter first(state), second(state);
        first.update(false);
        first.update(true);
        first.update(true);
        second.update(true);
        CHECK(state.owners == 2 && *flag == 0);
        first.update(false);
        CHECK(state.owners == 1 && *flag == 0);
        second.update(false);
        CHECK(state.owners == 0 && *flag == 1);
        second.update(true);
        *flag = 1; // Scene changes can re-enable the native flag.
        second.update(true);
        CHECK(*flag == 0);
        counter += 1000000;
        failMove = true;
        *binding.frameElapsed = .004f;
        CHECK((*binding.frameMove)(binding.application) == E_FAIL);
        CHECK(*binding.frameElapsed == .004f);
        failMove = false;
    }
    CHECK(*flag == 1 && state.owners == 0 && *binding.frameMove == binding.originalMove);
    {
        FpsLimiter limiter(state);
        counter = 0;
        limiter.update(true);
        *binding.frameElapsed = .004f;
        CHECK(SUCCEEDED((*binding.frameMove)(binding.application)));
        auto before = moves;
        counter = 1000;
        CHECK(SUCCEEDED((*binding.frameMove)(binding.application)) && moves == before);
        image[0x866860 + 0xa8] = 0;
        limiter.update(true);
        CHECK(SUCCEEDED((*binding.frameMove)(binding.application)) && moves == before + 1);
        CHECK(*binding.frameElapsed == .004f); // A paused single step retains its native delta.
        image[0x866860 + 0xa8] = 1;
        CHECK(SUCCEEDED((*binding.frameMove)(binding.application)) && moves == before + 2);
        // A callback may release the last device. Uninstall must not deadlock or keep catching up.
        counter += 128000;
        disableDuringMove = &limiter;
        CHECK(SUCCEEDED((*binding.frameMove)(binding.application)));
        disableDuringMove = nullptr;
        CHECK(moves == before + 3 && !limiter.active());
        CHECK(*binding.frameMove == binding.originalMove && *flag == 1);
    }
    *flag = 0;
    {
        FpsLimiter limiter(state);
        limiter.update(true);
    }
    CHECK(*flag == 0 && *binding.frameMove == binding.originalMove);
    fps_detail::State unknown;
    FpsLimiter unsupported(unknown);
    unsupported.update(true);
    CHECK(!unsupported.supported() && !unsupported.active());
    FpsLimiter production;
    production.update(true);
    CHECK(!production.supported() && !production.active());
    CHECK(!fps_detail::InspectImage(GetModuleHandleW(nullptr)));
    CHECK(!fps_detail::InspectImage(nullptr));
    CHECK(VirtualFree(image, 0, MEM_RELEASE));
    puts("PASS: simulation cadence independent of render FPS, x86 callback ABI, ASLR, bounded catch-up, "
         "native pacing restored, unknown clients untouched.");
}
