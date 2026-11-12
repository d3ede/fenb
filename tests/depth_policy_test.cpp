#include "depth_policy.hpp"
#include <cstdio>
#include <cstring>
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
void Log(const char *s) {
    puts(s);
}
int main() {
    Com<IDirect3D9> root;
    root.p = Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(root.p);
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FENBDepthPolicyTest";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"Depth policy fixture", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64,
                                nullptr, nullptr, wc.hInstance, nullptr);
    D3DPRESENT_PARAMETERS original{};
    original.Windowed = TRUE;
    original.hDeviceWindow = window;
    original.BackBufferWidth = original.BackBufferHeight = 64;
    original.BackBufferFormat = D3DFMT_X8R8G8B8;
    original.SwapEffect = D3DSWAPEFFECT_DISCARD;
    original.EnableAutoDepthStencil = TRUE;
    original.AutoDepthStencilFormat = D3DFMT_D16;
    for (auto msaa : {D3DMULTISAMPLE_NONE, D3DMULTISAMPLE_8_SAMPLES}) {
        auto p = original;
        p.MultiSampleType = msaa;
        if (PreciseDepthFormat(root, 0, D3DDEVTYPE_HAL, p) == D3DFMT_D16) {
            puts("SKIP: D24 precision upgrade unavailable for this MSAA mode");
            continue;
        }
        auto requested = p;
        unsigned calls = 0;
        Com<IDirect3DDevice9> device;
        CHECK(SUCCEEDED(WithDepthFormat(root, 0, D3DDEVTYPE_HAL, &p, true, [&](auto *adjusted) {
            ++calls;
            return root->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                      adjusted, device.out());
        })));
        CHECK(calls == 1 &&
              (p.AutoDepthStencilFormat == D3DFMT_D24X8 || p.AutoDepthStencilFormat == D3DFMT_D24S8));
        Com<IDirect3DSurface9> depth;
        device->GetDepthStencilSurface(depth.out());
        D3DSURFACE_DESC desc{};
        depth->GetDesc(&desc);
        CHECK(desc.Format == p.AutoDepthStencilFormat && desc.MultiSampleType == msaa);
        CHECK(p.BackBufferWidth == requested.BackBufferWidth &&
              p.BackBufferHeight == requested.BackBufferHeight &&
              p.BackBufferFormat == requested.BackBufferFormat &&
              p.MultiSampleQuality == requested.MultiSampleQuality);
        depth.out();
        auto reset = requested;
        CHECK(SUCCEEDED(WithDepthFormat(root, 0, D3DDEVTYPE_HAL, &reset, true,
                                        [&](auto *parameters) { return device->Reset(parameters); })));
        CHECK(reset.AutoDepthStencilFormat != D3DFMT_D16 && reset.MultiSampleType == msaa);
        device.out();
        p = requested;
        calls = 0;
        CHECK(SUCCEEDED(WithDepthFormat(root, 0, D3DDEVTYPE_HAL, &p, true, [&](auto *adjusted) -> HRESULT {
            ++calls;
            return adjusted->AutoDepthStencilFormat == D3DFMT_D16 ? D3D_OK : D3DERR_OUTOFVIDEOMEMORY;
        })));
        CHECK(calls == 2 && memcmp(&p, &requested, sizeof(p)) == 0);
        p = requested;
        calls = 0;
        CHECK(WithDepthFormat(root, 0, D3DDEVTYPE_HAL, &p, true, [&](auto *) {
                  ++calls;
                  return D3DERR_DEVICELOST;
              }) == D3DERR_DEVICELOST);
        CHECK(calls == 1 && memcmp(&p, &requested, sizeof(p)) == 0);
        calls = 0;
        CHECK(SUCCEEDED(WithDepthFormat(root, 0, D3DDEVTYPE_HAL, &p, false, [&](auto *unchanged) {
            ++calls;
            return unchanged->AutoDepthStencilFormat == D3DFMT_D16 ? D3D_OK : D3DERR_INVALIDCALL;
        })));
        CHECK(calls == 1);
    }
    for (int samples : {-1, 2, 4, 8}) {
        auto p = original;
        Com<IDirect3DDevice9> device;
        CHECK(SUCCEEDED(WithMultisample(root, 0, D3DDEVTYPE_HAL, &p, samples, [&](auto *parameters) {
            return root->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                      parameters, device.out());
        })));
        CHECK(p.MultiSampleType == (samples < 0 ? D3DMULTISAMPLE_NONE : D3DMULTISAMPLE_TYPE(samples)));
        CHECK(p.BackBufferWidth == original.BackBufferWidth &&
              p.BackBufferFormat == original.BackBufferFormat &&
              p.AutoDepthStencilFormat == original.AutoDepthStencilFormat);
    }
    auto p = original;
    unsigned calls = 0;
    CHECK(SUCCEEDED(WithMultisample(root, 0, D3DDEVTYPE_HAL, &p, 4, [&](auto *parameters) -> HRESULT {
        ++calls;
        return parameters->MultiSampleType == D3DMULTISAMPLE_NONE ? D3D_OK : D3DERR_OUTOFVIDEOMEMORY;
    })));
    CHECK(calls == 2 && p.MultiSampleType == D3DMULTISAMPLE_NONE);
    calls = 0;
    CHECK(WithMultisample(root, 0, D3DDEVTYPE_HAL, &p, 4, [&](auto *) {
              ++calls;
              return D3DERR_DEVICELOST;
          }) == D3DERR_DEVICELOST);
    CHECK(calls == 1);
    CHECK(!ConfiguredPreciseDepth()); // This fixture executable is not the supported game host.
    DestroyWindow(window);
    puts("PASS: D24 allocation, unchanged color/MSAA, allocation fallback, Lost Device, opt-out and host "
         "guard.");
}
