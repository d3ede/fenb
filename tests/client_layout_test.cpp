#include "client_layout.hpp"
#include "fps_limiter.hpp"
#include "environment.hpp"
#include <cstdio>
void Log(const char *) {}
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            printf("FAIL %d: %s\n", __LINE__, #x);                                                           \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
int main() {
    CHECK(!client_detail::Discover(nullptr).frames());
    CHECK(!client_detail::Discover(GetModuleHandleW(nullptr)).frames());
    for (DWORD shift : {0u, 0x1000u}) {
        constexpr DWORD size = 0x50000;
        auto *image = (BYTE *)VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        CHECK(image);
        auto *dos = (IMAGE_DOS_HEADER *)image;
        dos->e_magic = IMAGE_DOS_SIGNATURE;
        dos->e_lfanew = 0x80;
        auto *nt = (IMAGE_NT_HEADERS32 *)(image + 0x80);
        nt->Signature = IMAGE_NT_SIGNATURE;
        nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
        nt->FileHeader.NumberOfSections = 3;
        nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER32);
        nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
        nt->OptionalHeader.SizeOfImage = size;
        nt->FileHeader.TimeDateStamp = 123 + shift; // Neither timestamp nor original image size is pinned.
        auto *sections = (IMAGE_SECTION_HEADER *)(image + 0x80 + sizeof(*nt));
        const DWORD starts[] = {0x1000, 0x10000, 0x20000}, sizes[] = {0xf000, 0x10000, 0x30000};
        for (unsigned i = 0; i < 3; ++i) {
            sections[i].VirtualAddress = starts[i];
            sections[i].Misc.VirtualSize = sizes[i];
            sections[i].Characteristics = IMAGE_SCN_MEM_READ | (i == 0   ? IMAGE_SCN_MEM_EXECUTE
                                                                : i == 2 ? IMAGE_SCN_MEM_WRITE
                                                                         : 0);
        }
        auto emit = [&](DWORD r, const char *pattern) {
            std::istringstream in(pattern);
            std::string t;
            while (in >> t)
                image[r++] = t == "??" ? 0 : BYTE(std::stoi(t, nullptr, 16));
        };
        auto pointer = [&](DWORD r, DWORD target) {
            DWORD v = DWORD(uintptr_t(image)) + target;
            memcpy(image + r, &v, 4);
        };
        client_detail::Layout expected{0x2000 + shift,  0x20000 + shift, 0x20004 + shift, 0x20008 + shift,
                                       0x11000 + shift, 0x11004 + shift, 0x21000 + shift, 0x10000 + shift,
                                       0x3000 + shift,  0x4000 + shift,  0x22000 + shift, 0x23000 + shift,
                                       0x22040 + shift, 0x22044 + shift, 0x5000 + shift,  0x6000 + shift};
        auto gate = expected.gate;
        emit(gate, "83 3D ?? ?? ?? ?? 00 8B 3D ?? ?? ?? ?? 74 36 FF D7 8B 0D ?? ?? ?? ?? 2B 0D ?? ?? ?? ?? "
                   "03 C8 A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 81 F9 E8 03 00 00");
        pointer(gate + 2, expected.flag);
        pointer(gate + 9, expected.timeImport);
        pointer(gate + 19, expected.elapsed);
        pointer(gate + 25, expected.tick);
        pointer(gate + 32, expected.tick);
        pointer(gate + 38, expected.elapsed);
        emit(gate - 14, "FF 50 14");
        emit(gate + 84, "FF 50 18");
        emit(gate + 136, "83 3D ?? ?? ?? ?? 00 74 73");
        pointer(gate + 138, expected.flag);
        emit(gate + 202, "83 F9 10 73 2D B8 10 00 00 00 2B C1 50 FF 15");
        pointer(gate + 217, expected.sleepImport);
        emit(expected.move, "53 8B DC 83 EC 08 83 E4 F8 83 C4 04 55 8B 6B 04 89 6C 24 04 8B EC 6A FF");
        emit(expected.render, "55 8B EC 83 E4 F8 83 EC 20 A1 00 00 00 00 33 C4 89 44 24 1C 56 8B F1");
        pointer(expected.vtable + 20, expected.move);
        pointer(expected.vtable + 24, expected.render);
        pointer(expected.application, expected.vtable);
        *(LONG *)(image + expected.flag) = 1;
        emit(expected.compute, "53 8B D9 56 57 83 7B 10 00 0F 85 BB 00 00 00 FF 15 ?? ?? ?? ?? 66 0F 6E C0 "
                               "C1 E8 1F F3 0F E6 C0");
        emit(0x7000 + shift,
             "83 3D ?? ?? ?? ?? 00 0F 85 CE 00 00 00 FF 15 ?? ?? ?? ?? 66 0F 6E C0 C1 E8 1F F3 0F E6 C0");
        pointer(0x7002 + shift, expected.clock + 16);
        emit(0x7100 + shift, "B8 ?? ?? ?? ?? C3 CC 55 8B EC 83 B9 38 B0 02 00 00 75 1C 83 7D 0C 01 75 0F");
        pointer(0x7101 + shift, expected.sky);
        emit(expected.initFall, "53 8B DC 83 EC 08 83 E4 F8 83 C4 04 55 8B 6B 04");
        emit(expected.initFall + 62,
             "8B F1 89 75 BC C7 45 C0 00 00 00 00 0F 57 C0 8B 46 20 0F 11 45 CC 0F 11 45 DC 83 F8 04");
        emit(0x7200 + shift, "55 8B EC 8B 15 ?? ?? ?? ?? 53 56 57 0F B6 82 9C 06 00 00 33 FF 8B D9 39 45 08");
        pointer(0x7205 + shift, expected.player);
        emit(0x7300 + shift, "89 84 24 A4 00 00 00 83 3D ?? ?? ?? ?? 00 56 57 8B F9 0F 84 D3 08 00 00");
        pointer(0x7309 + shift, expected.weather);
        DWORD old = 0;
        CHECK(VirtualProtect(image + 0x1000, 0xf000, PAGE_EXECUTE_READ, &old));
        CHECK(VirtualProtect(image + 0x10000, 0x10000, PAGE_READONLY, &old));
        auto found = client_detail::Discover((HMODULE)image);
        client_detail::Image tableScan((HMODULE)image);
        CHECK(tableScan.table(5,expected.move)==expected.vtable);
        CHECK(tableScan.table(6,expected.render)==expected.vtable);
        CHECK(found.environment() && found.gate == expected.gate &&
              found.application == expected.application && found.clock == expected.clock &&
              found.initFall == expected.initFall && found.sky == expected.sky &&
              found.player == expected.player && found.weather == expected.weather);
        CHECK(fps_detail::InspectImage((HMODULE)image));
        CHECK(environment_detail::InspectImage((HMODULE)image));
        CHECK(*(LONG *)(image + expected.flag) == 1); // Discovery never modifies the image.
        CHECK(VirtualProtect(image + 0x1000, 0xf000, PAGE_READWRITE, &old));
        image[gate + 202 + 2] = 0x11;
        CHECK(VirtualProtect(image + 0x1000, 0xf000, PAGE_EXECUTE_READ, &old));
        CHECK(!fps_detail::InspectImage((HMODULE)image)); // Changed frame gate is rejected.
        CHECK(VirtualFree(image, 0, MEM_RELEASE));
    }
    puts("PASS: relocated structural discovery, metadata changes, changed ABI rejection, read-only probing");
}
