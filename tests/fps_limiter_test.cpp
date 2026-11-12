#include "fps_limiter.hpp"
#include <cstdio>
#include <cstring>
#include <cstdint>

void Log(const char *) {}
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            printf("FAIL %d: %s\n", __LINE__, #x);                                                           \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)

int main() {
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
    auto *flag = reinterpret_cast<LONG *>(image + 0x829c4c);
    *flag = 1;
    DWORD previous = 0;
    CHECK(VirtualProtect(image + 0xeb000, 0x1000, PAGE_EXECUTE_READ, &previous));
    auto binding = fps_detail::InspectImage(module);
    CHECK(binding && binding.enabled == flag);
    CHECK(*flag == 1); // Detection is strictly read-only.
    ++nt->FileHeader.TimeDateStamp;
    CHECK(!fps_detail::InspectImage(module));
    --nt->FileHeader.TimeDateStamp;
    CHECK(VirtualProtect(image + 0xeb000, 0x1000, PAGE_READWRITE, &previous));
    CHECK(!fps_detail::InspectImage(module)); // Code must belong to executable image memory.
    image[0xebe9b] = 0x11;
    CHECK(VirtualProtect(image + 0xeb000, 0x1000, PAGE_EXECUTE_READ, &previous));
    CHECK(!fps_detail::InspectImage(module)); // Changed frame interval is unsupported.
    CHECK(VirtualProtect(image + 0xeb000, 0x1000, PAGE_READWRITE, &previous));
    image[0xebe9b] = 0x10;
    pointer(0xebe59, 0x829c50);
    CHECK(VirtualProtect(image + 0xeb000, 0x1000, PAGE_EXECUTE_READ, &previous));
    CHECK(!fps_detail::InspectImage(module)); // Both gates must reference the same verified flag.
    CHECK(VirtualProtect(image + 0xeb000, 0x1000, PAGE_READWRITE, &previous));
    pointer(0xebe59, 0x829c4c);
    CHECK(VirtualProtect(image + 0xeb000, 0x1000, PAGE_EXECUTE_READ, &previous));
    *flag = 2;
    CHECK(!fps_detail::InspectImage(module));
    *flag = 1;
    CHECK(VirtualProtect(image + 0x829000, 0x1000, PAGE_READONLY, &previous));
    CHECK(!fps_detail::InspectImage(module));
    CHECK(VirtualProtect(image + 0x829000, 0x1000, PAGE_READWRITE, &previous));
    CHECK(fps_detail::InspectImage(module));

    fps_detail::State state(binding);
    *binding.lastTick = 1234;
    *binding.elapsed = 5678;
    // Even a fully verified client must be rejected. Protect both data pages so
    // any regression that writes the old limiter flag or clocks fails immediately.
    CHECK(VirtualProtect(image + 0x829000, 0x1000, PAGE_READONLY, &previous));
    CHECK(VirtualProtect(image + 0x844000, 0x1000, PAGE_READONLY, &previous));
    {
        FpsLimiter first(state), second(state);
        CHECK(!first.supported() && !second.supported());
        first.update(false);
        first.update(true);
        first.update(true);
        second.update(true);
        first.update(false);
        second.update(false);
        second.update(true);
        CHECK(!first.active() && !second.active());
    }
    CHECK(*flag == 1 && *binding.lastTick == 1234 && *binding.elapsed == 5678);
    CHECK(VirtualProtect(image + 0x829000, 0x1000, PAGE_READWRITE, &previous));
    CHECK(VirtualProtect(image + 0x844000, 0x1000, PAGE_READWRITE, &previous));
    *flag = 0;
    {
        FpsLimiter limiter(state);
        limiter.update(true);
    }
    CHECK(*flag == 0 && *binding.lastTick == 1234 && *binding.elapsed == 5678);
    fps_detail::State unknown;
    FpsLimiter unsupported(unknown);
    unsupported.update(true);
    CHECK(!unsupported.supported() && !unsupported.active());
    FpsLimiter production;
    production.update(true);
    CHECK(!production.supported() && !production.active());
    CHECK(!fps_detail::InspectImage(GetModuleHandleW(nullptr)));
    CHECK(!fps_detail::InspectImage(nullptr));
    CHECK(!fps_detail::KnownClientFile(L"build/fps-missing.exe"));
    wchar_t ownPath[32768]{};
    GetModuleFileNameW(nullptr, ownPath, 32768);
    CHECK(!fps_detail::KnownClientFile(ownPath));
    // Optional read-only verification against the locally inspected revision.
    if (GetFileAttributesW(L"X:/Games/Homesickness/Neuz.exe") != INVALID_FILE_ATTRIBUTES)
        printf("INFO: installed client matches pinned SHA-256: %s\n",
               fps_detail::KnownClientFile(L"X:/Games/Homesickness/Neuz.exe") ? "yes"
                                                                              : "no (different revision)");
    CHECK(VirtualFree(image, 0, MEM_RELEASE));
    puts("PASS: read-only client detection; FPS unlock refused for verified and unknown clients; "
         "protected limiter flag and clocks unchanged.");
}
