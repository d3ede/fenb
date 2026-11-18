#include "client_fixture.hpp"
#include "environment.hpp"
#include <bit>
#include <cstdio>
#include <cstring>

void Log(const char *) {}
namespace {
IDirect3DDevice9 *device = nullptr;
int initializations = 0;
bool initialized = false;
bool failTexture = false;
void __fastcall Init(void *object, void *) {
    auto *sky = static_cast<unsigned char *>(object);
    auto **texture = reinterpret_cast<IDirect3DTexture9 **>(sky + 0x40);
    Drop(*texture);
    if (failTexture)
        return;
    initialized =
        SUCCEEDED(device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, texture, nullptr));
    auto *velocities = reinterpret_cast<float *>(sky + 0xa4);
    auto *positions = velocities + 6000;
    for (int i = 0; i < 400; ++i) {
        velocities[i * 3] = velocities[i * 3 + 2] = 0;
        velocities[i * 3 + 1] = -.2f;
        positions[i * 3] = 1;
        positions[i * 3 + 1] = 10;
        positions[i * 3 + 2] = 3;
    }
    ++initializations;
}
HRESULT __fastcall SimulateRender(void *, void *) {
    return S_OK;
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
    using namespace environment_detail;
    Timeline time;
    CHECK(time.sample(720, false, 1000) == 43200);
    CHECK(time.sample(720, false, 1600) == 43260);
    CHECK(time.sample(720, true, 2200) == 43320);
    CHECK(time.sample(720, true, 99000) == 43320);
    CHECK(time.sample(1439, false, 100000) == 86340);
    CHECK(time.sample(1439, false, 100600) == 0); // Midnight wraps without changing the game's date.
    CHECK(time.sample(0, true, 100700) == 0);     // Dragging applies immediately while frozen.
    CHECK(!InspectImage(GetModuleHandleW(nullptr)));
    CHECK(!InspectImage(nullptr));
    {std::array<unsigned char,0x180> player{};void *slot=player.data();Binding binding;binding.player=&slot;
        const float expected[]={123,45,678};memcpy(player.data()+0x160,expected,sizeof(expected));float position[3]{};
        CHECK(PlayerPosition(binding,position)&&!memcmp(position,expected,sizeof(expected)));
        const float invalid=std::numeric_limits<float>::quiet_NaN();memcpy(player.data()+0x160,&invalid,4);
        CHECK(!PlayerPosition(binding,position));slot=nullptr;CHECK(!PlayerPosition(binding,position));
        binding.player=reinterpret_cast<void **>(1);CHECK(!PlayerPosition(binding,position));}
    // ASLR-relocated, disposable image: version, code and page protections must all match.
    constexpr DWORD imageSize = 11538432;
    HANDLE mapping =
        CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, imageSize, nullptr);
    CHECK(mapping);
    auto *image = static_cast<unsigned char *>(
        MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE, 0, 0, imageSize));
    CHECK(image);
    auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(image);
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto *nt = reinterpret_cast<IMAGE_NT_HEADERS32 *>(image + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.TimeDateStamp = 1794424444;
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.SizeOfImage = imageSize;
    nt->OptionalHeader.AddressOfEntryPoint = 5918976;
    *reinterpret_cast<void **>(image + 0x866860) = image + 0x794bec;
    *reinterpret_cast<void **>(image + 0x794bec + 24) = image + 0xd9f20;
    const unsigned char render[] = {0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x83, 0xec, 0x20};
    const unsigned char init[] = {0x53, 0x8b, 0xdc, 0x83, 0xec, 0x08};
    const unsigned char compute[] = {0x53, 0x8b, 0xd9, 0x56, 0x57, 0x83, 0x7b, 0x10, 0};
    memcpy(image + 0xd9f20, render, sizeof(render));
    memcpy(image + 0x3863d0, init, sizeof(init));
    memcpy(image + 0x3b7200, compute, sizeof(compute));
    DWORD protection = 0;
    CHECK(VirtualProtect(image, imageSize, PAGE_WRITECOPY, &protection));
    for (DWORD rva : {0x829000, 0x83e000, 0x866000, 0x923000})
        CHECK(VirtualProtect(image + rva, 4096, PAGE_READWRITE, &protection));
    // Match live Neuz: one large sky object crosses several writable and COW image regions.
    CHECK(VirtualProtect(image + 0xa2d000, 8192, PAGE_READWRITE, &protection));
    CHECK(VirtualProtect(image + 0xd9000, 4096, PAGE_EXECUTE_READ, &protection));
    CHECK(VirtualProtect(image + 0x386000, 4096, PAGE_EXECUTE_READ, &protection));
    CHECK(VirtualProtect(image + 0x3b7000, 4096, PAGE_EXECUTE_READ, &protection));
    CHECK(VirtualProtect(image + 0x794000, 4096, PAGE_READONLY, &protection));
    CHECK(InspectImage(reinterpret_cast<HMODULE>(image), client_detail::Reference));
    CHECK(VirtualProtect(image + 0xa40000, 4096, PAGE_READONLY, &protection));
    CHECK(!InspectImage(reinterpret_cast<HMODULE>(image), client_detail::Reference)); // A read-only hole inside the sky is unsafe.
    CHECK(VirtualProtect(image + 0xa40000, 4096, PAGE_READWRITE | PAGE_GUARD, &protection));
    CHECK(!InspectImage(reinterpret_cast<HMODULE>(image), client_detail::Reference));
    CHECK(VirtualProtect(image + 0xa40000, 4096, PAGE_WRITECOPY, &protection));
    CHECK(InspectImage(reinterpret_cast<HMODULE>(image), client_detail::Reference));
    nt->FileHeader.TimeDateStamp++;
    CHECK(InspectImage(reinterpret_cast<HMODULE>(image), client_detail::Reference)); // Metadata is not an ABI change.
    nt->FileHeader.TimeDateStamp--;
    CHECK(VirtualProtect(image + 0x829000, 4096, PAGE_READONLY, &protection));
    CHECK(!InspectImage(reinterpret_cast<HMODULE>(image), client_detail::Reference));
    CHECK(VirtualProtect(image + 0x829000, 4096, PAGE_READWRITE, &protection));
    CHECK(VirtualProtect(image + 0x386000, 4096, PAGE_READWRITE, &protection));
    image[0x3863d0] = 0xcc;
    CHECK(VirtualProtect(image + 0x386000, 4096, PAGE_EXECUTE_READ, &protection));
    CHECK(!InspectImage(reinterpret_cast<HMODULE>(image), client_detail::Reference));
    UnmapViewOfFile(image);
    CloseHandle(mapping);

    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FENBEnvironmentTest";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"Environment tests", WS_OVERLAPPEDWINDOW, 0, 0, 128, 128,
                                nullptr, nullptr, wc.hInstance, nullptr);
    Com<IDirect3D9> root;
    root.p = Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(root.p);
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window;
    Com<IDirect3DDevice9> d;
    CHECK(SUCCEEDED(
        root->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, d.out())));
    device = d;
    auto sky = std::make_unique<unsigned char[]>(0x2b03c);
    auto playerObject = std::make_unique<unsigned char[]>(0x180);
    float center[] = {1, 2, 3};
    memcpy(playerObject.get() + 0x160, center, sizeof(center));
    void *player = playerObject.get();
    *reinterpret_cast<void **>(sky.get() + 0xa0) = player;
    auto &mode = *reinterpret_cast<int *>(sky.get() + 0x20);
    auto &count = *reinterpret_cast<int *>(sky.get() + 0x2b024);
    auto &texture = *reinterpret_cast<IDirect3DTexture9 **>(sky.get() + 0x40);
    mode = 3;
    count = 400;
    CHECK(SUCCEEDED(d->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr)));
    auto *gameTexture = texture;
    auto savedSky = std::make_unique<unsigned char[]>(0x2b03c);
    memcpy(savedSky.get(), sky.get(), 0x2b03c);
    Clock clock{123456, 77, FALSE, 12, 34, 21, 7};
    Clock savedClock = clock;
    auto renderFunction = std::bit_cast<environment_detail::Render>(&SimulateRender);
    int option = 0;
    Binding binding{player,    &renderFunction, renderFunction, &clock,
                    sky.get(), &option,         &player,        std::bit_cast<InitFall>(&Init)};
    FrameState frame;
    Settings s;
    {
        FrameScope scope(binding, frame, s, 1000);
        CHECK(!memcmp(&clock, &savedClock, sizeof(clock)) && !initializations && !option);
    }
    s.timeOverride = s.weatherOverride = true;
    s.timeMinutes = 1110;
    s.weatherType = 1;
    s.weatherIntensity = 25;
    {
        FrameScope scope(binding, frame, s, 2000);
        CHECK(clock.fixed && clock.hour == 18 && clock.minute == 30 && clock.day == 7);
        CHECK(initialized && initializations == 1 && mode == 2 && count == 100 && option == 1);
        CHECK(texture && texture != gameTexture);
    }
    CHECK(!memcmp(&clock, &savedClock, sizeof(clock)));
    CHECK(!memcmp(sky.get(), savedSky.get(), 0x2b03c) && !option);
    auto *localTexture = frame.texture.p;
    {
        FrameScope scope(binding, frame, s, 2016);
        CHECK(initializations == 1 && texture == localTexture);
        CHECK(*reinterpret_cast<float *>(sky.get() + 0x5e64 + 4) < 10); // Precipitation moves live.
    }
    s.timeMinutes = 390;
    s.weatherType = 2;
    s.weatherIntensity = 75;
    {
        FrameScope scope(binding, frame, s, 2032);
        CHECK(clock.hour == 6 && clock.minute == 30 && mode == 3 && count == 300);
        CHECK(initializations == 2);
    }
    CHECK(!memcmp(sky.get(), savedSky.get(), 0x2b03c));
    // A failed native asset load must leave the game visible, with its original texture alive.
    s.weatherType = 1;
    failTexture = true;
    {
        FrameScope scope(binding, frame, s, 2040);
        CHECK(mode == 3 && count == 400 && texture == gameTexture && !option);
    }
    CHECK(!memcmp(sky.get(), savedSky.get(), 0x2b03c));
    failTexture = false;
    s.timeOverride = false;
    s.weatherType = 0;
    {
        FrameScope scope(binding, frame, s, 2048);
        CHECK(clock.hour == 21 && mode == 0 && count == 0);
    }
    CHECK(!memcmp(sky.get(), savedSky.get(), 0x2b03c));
    s.enabled = false;
    s.timeOverride = true;s.timeMinutes = 90;s.freezeTime = true;
    {
        FrameScope scope(binding, frame, s, 2064);
        CHECK(clock.hour == 1 && clock.minute == 30 && clock.fixed && mode == 3 && count == 400 && !option);
    }
    CHECK(!memcmp(&clock,&savedClock,sizeof(clock)));
    s.timeMinutes=120;
    {
        FrameScope scope(binding,frame,s,2080);
        CHECK(clock.hour==2 && clock.minute==0 && mode==3 && !option);
    }
    s.timeOverride=false;
    { FrameScope scope(binding,frame,s,2096);CHECK(!memcmp(&clock,&savedClock,sizeof(clock))); }
    frame.reset();
    CHECK(!frame.texture.p && !memcmp(sky.get(), savedSky.get(), 0x2b03c));
    Drop(texture);
    DestroyWindow(window);
    puts("PASS: native environment version guards, live time, freeze, midnight, rain/snow intensity, "
         "particle motion, independent switches, COM ownership and exact game-state restoration.");
}
