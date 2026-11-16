#include "effects.hpp"
#include "state_guard.hpp"
#include <cstdio>
#include <cstring>

#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

std::wstring ModuleDirectory() { return L"build\\"; }
void Log(const char *message) { puts(message); }

namespace {
constexpr UINT Width = 160, Height = 96;
std::vector<DWORD> Read(IDirect3DDevice9 *device, IDirect3DSurface9 *surface) {
    Com<IDirect3DSurface9> resolved, read;
    if (FAILED(device->CreateRenderTarget(Width, Height, D3DFMT_A8R8G8B8,
            D3DMULTISAMPLE_NONE, 0, FALSE, resolved.out(), nullptr)) ||
        FAILED(device->StretchRect(surface, nullptr, resolved, nullptr, D3DTEXF_NONE)) ||
        FAILED(device->CreateOffscreenPlainSurface(Width, Height, D3DFMT_A8R8G8B8,
            D3DPOOL_SYSTEMMEM, read.out(), nullptr)) ||
        FAILED(device->GetRenderTargetData(resolved, read))) return {};
    D3DLOCKED_RECT lock{};
    if (FAILED(read->LockRect(&lock, nullptr, D3DLOCK_READONLY))) return {};
    std::vector<DWORD> pixels(Width * Height);
    for (UINT y = 0; y < Height; ++y)
        memcpy(pixels.data() + y * Width, (BYTE *)lock.pBits + y * lock.Pitch, Width * 4);
    read->UnlockRect();
    return pixels;
}
int Channel(DWORD pixel, int shift = 0) { return (pixel >> shift) & 255; }
}

int main(int argc, char **) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FENBSharpeningTest";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"Sharpening fixture", WS_OVERLAPPEDWINDOW,
        0, 0, Width, Height, nullptr, nullptr, wc.hInstance, nullptr);
    Com<IDirect3D9> root;
    root.p = Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(root.p);
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.hDeviceWindow = window;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.BackBufferWidth = Width;
    pp.BackBufferHeight = Height;
    if (argc > 1) {
        pp.MultiSampleType = D3DMULTISAMPLE_8_SAMPLES;
        if (FAILED(root->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, pp.BackBufferFormat,
                TRUE, pp.MultiSampleType, nullptr))) {
            puts("SKIP: 8x MSAA unavailable");
            return 0;
        }
    }
    Com<IDirect3DDevice9> device;
    CHECK(SUCCEEDED(root->CreateDevice(0, D3DDEVTYPE_HAL, window,
        D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, device.out())));
    Com<IDirect3DSurface9> backbuffer;
    CHECK(SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, backbuffer.out())));
    Com<IDirect3DTexture9> texture;
    CHECK(SUCCEEDED(device->CreateTexture(Width, Height, 1, 0, D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED, texture.out(), nullptr)));
    D3DLOCKED_RECT lock{};
    CHECK(SUCCEEDED(texture->LockRect(0, &lock, nullptr, 0)));
    for (UINT y = 0; y < Height; ++y) for (UINT x = 0; x < Width; ++x) {
        int value = y < 16 ? 110 : y < 32 ? (x < Width / 2 ? 0 : 255) :
            y < 48 ? 127 + (x + y) % 2 :
            int(128 + 14 * std::cos(x * .8) * std::cos(y * .7));
        // Keep chroma constant in the textured region to detect colored fringes.
        ((DWORD *)((BYTE *)lock.pBits + y * lock.Pitch))[x] = y < 48 ?
            D3DCOLOR_ARGB(173, value, value, value) : D3DCOLOR_ARGB(173, value + 20, value, value - 20);
    }
    texture->UnlockRect(0);
    Effects effects(device);
    Settings settings;
    settings.bloom = false;
    // Neutral color keeps the same composite path alive for off/zero comparisons.
    settings.color = true;
    auto render = [&]() -> std::vector<DWORD> {
        StateGuard guard(device);
        if (!guard.valid()) return {};
        guard.isolateTargets();
        device->SetRenderTarget(0, backbuffer);
        D3DVIEWPORT9 viewport{0, 0, Width, Height, 0, 1};
        device->SetViewport(&viewport);
        device->SetVertexShader(nullptr);
        device->SetPixelShader(nullptr);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetTexture(0, texture);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        struct Vertex { float x, y, z, rhw, u, v; };
        Vertex vertices[] = {{-.5f,-.5f,0,1,0,0},{Width-.5f,-.5f,0,1,1,0},
            {-.5f,Height-.5f,0,1,0,1},{Width-.5f,Height-.5f,0,1,1,1}};
        if (FAILED(device->BeginScene())) return {};
        auto drawn = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex));
        device->EndScene();
        if (FAILED(drawn) || !effects.prepare(backbuffer, settings) || FAILED(device->BeginScene())) return {};
        effects.draw(backbuffer, settings);
        device->EndScene();
        return Read(device, backbuffer);
    };
    auto baseline = render();
    CHECK(baseline.size() == Width * Height);
    const size_t bytes = effects.bytes();
    settings.sharpening = true;
    settings.sharpeningStrength = 0;
    CHECK(render() == baseline);
    settings.sharpeningStrength = .45f;
    auto mild = render();
    settings.sharpeningStrength = 1;
    auto strong = render();
    CHECK(mild.size() == baseline.size() && strong.size() == baseline.size());
    CHECK(effects.bytes() == bytes); // No sharpening render target or history.
    int mildChange = 0, strongChange = 0;
    for (UINT y = 2; y + 2 < Height; ++y) for (UINT x = 2; x + 2 < Width; ++x) {
        auto i = y * Width + x;
        if (y < 14 || (y > 17 && y < 30) || (y > 33 && y < 46))
            CHECK(strong[i] == baseline[i]); // Flat fields, silhouettes and tiny noise.
        if (y > 50) {
            int difference = Channel(strong[i]) - Channel(baseline[i]);
            mildChange += std::abs(Channel(mild[i]) - Channel(baseline[i]));
            strongChange += std::abs(difference);
            CHECK(std::abs(difference) <= 11);
            for (int shift : {8, 16})
                CHECK(std::abs(Channel(strong[i], shift) - Channel(baseline[i], shift) - difference) <= 1);
        }
        CHECK((strong[i] >> 24) == (baseline[i] >> 24));
    }
    CHECK(mildChange > 100 && strongChange > mildChange);
    settings.color = false;
    CHECK(render() == strong); // Sharpening works as the only image effect.
    settings.bloom = true;
    settings.bloomStrength = 0;
    for (int scale : {2, 4}) {
        settings.bloomScale = scale;
        CHECK(render() == strong); // Blur must not leave bloom-sized sharpening offsets.
    }
    settings.bloom = false;
    for (int method : {1, 2}) {
        settings.fxaa = method == 1;
        settings.smaa = method == 2;
        settings.sharpening = false;
        auto off = render();
        const size_t aaBytes = effects.bytes();
        settings.sharpening = true;
        auto on = render();
        CHECK(off.size() == baseline.size() && on.size() == baseline.size() && off != on);
        CHECK(effects.bytes() == aaBytes);
    }
    settings.fxaa = settings.smaa = false;
    effects.invalidate();
    CHECK(render() == strong);
    puts("PASS: sharpening detail, strength, flat/noise/edge limiting, chroma, alpha, bloom offsets, AA, memory and recreation.");
    return 0;
}
