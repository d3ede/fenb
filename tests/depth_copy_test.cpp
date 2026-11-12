#include "depth_copy.hpp"
#include <cstdio>
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            printf("FAIL %d: %s\n", __LINE__, #x);                                                           \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
std::wstring ModuleDirectory() {
    return L"build\\";
}
void Log(const char *) {}
int main() {
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FENBSharedDepthFixture";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"Shared depth fixture", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64,
                                nullptr, nullptr, wc.hInstance, nullptr);
    Com<IDirect3D9> root;
    root.p = Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(root.p);
    for (auto format : {D3DFMT_D16, D3DFMT_D24X8})
        for (auto msaa : {D3DMULTISAMPLE_NONE, D3DMULTISAMPLE_8_SAMPLES}) {
            D3DPRESENT_PARAMETERS p{};
            p.Windowed = TRUE;
            p.hDeviceWindow = window;
            p.BackBufferWidth = p.BackBufferHeight = 64;
            p.SwapEffect = D3DSWAPEFFECT_DISCARD;
            p.EnableAutoDepthStencil = TRUE;
            p.AutoDepthStencilFormat = format;
            p.MultiSampleType = msaa;
            Com<IDirect3DDevice9> d;
            CHECK(SUCCEEDED(root->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                               &p, d.out())));
            d->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, .5f, 0);
            auto water = std::make_unique<DepthCopy>(d), scene = std::make_unique<DepthCopy>(d);
            if (!water->capture(64, 64)) {
                puts("SKIP: optional NVIDIA depth backend unavailable");
                return 0;
            }
            CHECK(scene->capture(64, 64));
            for (int toggle = 0; toggle < 4; ++toggle) {
                scene->beginFrame(false);
                water->beginFrame(true);
                CHECK(water->capture(64, 64));
                scene->beginFrame(true);
                CHECK(scene->capture(64, 64));
                water->beginFrame(false);
                scene->beginFrame(true);
                CHECK(scene->capture(64, 64));
                water->beginFrame(true);
                CHECK(water->capture(64, 64));
            }
            water.reset();
            scene->beginFrame(true);
            CHECK(scene->capture(64, 64)); // API must remain initialized.
            scene->invalidate();
            CHECK(SUCCEEDED(d->Reset(&p)));
            CHECK(scene->capture(64, 64));
            scene.reset(); // Last backend reference releases the API, allowing a fresh device next iteration.
        }
    DestroyWindow(window);
    puts("PASS: shared NVIDIA registration, independent toggles/destruction, D16/D24, zero/8x MSAA, Reset.");
}
