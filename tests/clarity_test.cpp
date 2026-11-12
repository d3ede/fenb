#include "common.hpp"
#include <generated/scene_shaders.hpp>
#include <generated/shaders.hpp>
#include <cstdio>
#include <cstring>
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            printf("FAIL %d: %s\n", __LINE__, #x);                                                           \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
namespace {
constexpr UINT W = 128, H = 96, X = 65, Y = 49;
std::vector<DWORD> Read(IDirect3DDevice9 *d) {
    Com<IDirect3DSurface9> target, resolved, read;
    D3DSURFACE_DESC desc{};
    d->GetRenderTarget(0, target.out());
    target->GetDesc(&desc);
    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
        if (FAILED(d->CreateRenderTarget(W, H, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, resolved.out(),
                                         nullptr)) ||
            FAILED(d->StretchRect(target, nullptr, resolved, nullptr, D3DTEXF_NONE)))
            return {};
    }
    if (FAILED(d->CreateOffscreenPlainSurface(W, H, desc.Format, D3DPOOL_SYSTEMMEM, read.out(), nullptr)) ||
        FAILED(d->GetRenderTargetData(resolved.p ? resolved.p : target.p, read)))
        return {};
    D3DLOCKED_RECT lock{};
    if (FAILED(read->LockRect(&lock, nullptr, D3DLOCK_READONLY)))
        return {};
    std::vector<DWORD> pixels(W * H);
    for (UINT y = 0; y < H; ++y)
        memcpy(pixels.data() + y * W, (BYTE *)lock.pBits + y * lock.Pitch, W * 4);
    read->UnlockRect();
    return pixels;
}
} // namespace
int main(int argc, char **) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FENBClarityTest";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"VFX clarity fixture", WS_OVERLAPPEDWINDOW, 0, 0, W, H,
                                nullptr, nullptr, wc.hInstance, nullptr);
    Com<IDirect3D9> root;
    root.p = Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(root.p);
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.hDeviceWindow = window;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = W;
    pp.BackBufferHeight = H;
    if (argc > 1) {
        pp.MultiSampleType = D3DMULTISAMPLE_8_SAMPLES;
        if (FAILED(root->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, pp.BackBufferFormat, TRUE,
                                                    pp.MultiSampleType, nullptr))) {
            puts("SKIP: 8x MSAA unsupported");
            return 0;
        }
    }
    Com<IDirect3DDevice9> d;
    CHECK(SUCCEEDED(
        root->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, d.out())));
    Com<IDirect3DTexture9> image, depth, light;
    CHECK(SUCCEEDED(d->CreateTexture(W, H, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, image.out(), nullptr)));
    CHECK(SUCCEEDED(d->CreateTexture(W, H, 1, 0, D3DFMT_R32F, D3DPOOL_MANAGED, depth.out(), nullptr)));
    CHECK(SUCCEEDED(
        d->CreateTexture(W / 4, H / 4, 1, 0, D3DFMT_A32B32G32R32F, D3DPOOL_MANAGED, light.out(), nullptr)));
    auto fillImage = [&](DWORD marker) {
        D3DLOCKED_RECT lock{};
        image->LockRect(0, &lock, nullptr, 0);
        for (UINT y = 0; y < H; ++y)
            for (UINT x = 0; x < W; ++x)
                ((DWORD *)((BYTE *)lock.pBits + y * lock.Pitch))[x] =
                    (x == X && y == Y) ? marker : 0xff303030;
        image->UnlockRect(0);
    };
    D3DLOCKED_RECT lock{};
    CHECK(SUCCEEDED(depth->LockRect(0, &lock, nullptr, 0)));
    const float a = 100.f / 99.9f, b = -.1f * a;
    for (UINT y = 0; y < H; ++y)
        for (UINT x = 0; x < W; ++x)
            ((float *)((BYTE *)lock.pBits + y * lock.Pitch))[x] = a + b / ((x == X && y == Y) ? 17.f : 20.f);
    depth->UnlockRect(0);
    CHECK(SUCCEEDED(light->LockRect(0, &lock, nullptr, 0)));
    for (UINT y = 0; y < H / 4; ++y)
        for (UINT x = 0; x < W / 4; ++x) {
            auto p = (float *)((BYTE *)lock.pBits + y * lock.Pitch) + x * 4;
            p[0] = 2;
            p[1] = 1;
            p[2] = .3f;
            p[3] = 1;
        }
    light->UnlockRect(0);
    Com<IDirect3DPixelShader9> scenePS, postPS, raySourcePS, dofPS;
    Com<IDirect3DTexture9> sky, meanSky;
    CHECK(SUCCEEDED(d->CreateTexture(W, H, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, sky.out(), nullptr)));
    CHECK(SUCCEEDED(d->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, meanSky.out(), nullptr)));
    CHECK(SUCCEEDED(d->CreatePixelShader(ShaderSceneComposite, scenePS.out())));
    CHECK(SUCCEEDED(d->CreatePixelShader(ShaderComposite, postPS.out())));
    CHECK(SUCCEEDED(d->CreatePixelShader(ShaderSceneRaySource, raySourcePS.out())));
    CHECK(SUCCEEDED(d->CreatePixelShader(ShaderSceneDoF, dofPS.out())));
    float constants[56][4]{};
    auto sceneConstants = [&] {
        memset(constants, 0, sizeof(constants));
        constants[0][0] = 1.5f;
        constants[0][1] = 2;
        constants[0][2] = a;
        constants[0][3] = b;
        constants[1][2] = constants[1][3] = 1;
        constants[2][0] = 1.f / W;
        constants[2][1] = 1.f / H;
        constants[16][0] = constants[16][1] = constants[16][2] = .12f;
        constants[17][2] = 4.f / W;
        constants[17][3] = 4.f / H;
    };
    auto draw = [&](IDirect3DPixelShader9 *ps) {
        d->SetVertexShader(nullptr);
        d->SetPixelShader(ps);
        d->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        d->SetRenderState(D3DRS_ZENABLE, FALSE);
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        d->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        d->SetRenderState(D3DRS_FOGENABLE, FALSE);
        d->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        for (DWORD i = 0; i < 7; ++i) {
            d->SetTexture(i, nullptr);
            d->SetSamplerState(i, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            d->SetSamplerState(i, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            d->SetSamplerState(i, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            d->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            d->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            d->SetSamplerState(i, D3DSAMP_SRGBTEXTURE, FALSE);
        }
        d->SetTexture(0, image);
        d->SetTexture(1, ps == postPS.p ? light.p : depth.p);
        d->SetTexture(4, light);
        d->SetTexture(5, sky);
        d->SetTexture(6, meanSky);
        if (ps == dofPS.p) {
            d->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            d->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        }
        d->SetPixelShaderConstantF(0, constants[0], 56);
        struct V {
            float x, y, z, w, u, v;
        };
        V q[] = {{-.5f, -.5f, 0, 1, 0, 0},
                 {W - .5f, -.5f, 0, 1, 1, 0},
                 {-.5f, H - .5f, 0, 1, 0, 1},
                 {W - .5f, H - .5f, 0, 1, 1, 1}};
        d->BeginScene();
        auto hr = d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(V));
        d->EndScene();
        return SUCCEEDED(hr);
    };
    fillImage(0xff303030);
    sceneConstants();
    CHECK(draw(scenePS));
    auto native = Read(d);
    CHECK(native.size() == W * H);
    constants[19][3] = 1;
    CHECK(draw(scenePS));
    auto lit = Read(d);
    CHECK(lit.size() == W * H);
    CHECK((lit[Y * W + X] & 255) <= (native[Y * W + X] & 255) + 2);
    CHECK(((lit[(Y + 8) * W + X] >> 16) & 255) > ((native[(Y + 8) * W + X] >> 16) & 255) + 20);
    // A one-pixel foreground feature must not inherit a bright background light sample.
    printf("Unmatched foreground pixel: native=%06lx lit=%06lx\n", native[Y * W + X] & 0xffffff,
           lit[Y * W + X] & 0xffffff);
    fillImage(0xffffe040);
    sceneConstants();
    CHECK(draw(scenePS));
    auto sharp = Read(d);
    constants[7][2] = 1;
    constants[6][0] = 5;
    constants[6][1] = 1;
    constants[6][2] = 12;
    CHECK(draw(dofPS));
    auto focused = Read(d);
    CHECK(focused[Y * W + X] == sharp[Y * W + X]);
    auto mix = [](DWORD foreground, DWORD background, float coverage) {
        DWORD result = 0xff000000;
        for (int shift : {0, 8, 16})
            result |= DWORD(std::lround(((foreground >> shift) & 255) * coverage +
                                       ((background >> shift) & 255) * (1 - coverage))) << shift;
        return result;
    };
    const DWORD skyColor = 0xff667799, foregroundColor = 0xff334455;
    auto silhouette = [&](float coverage) {
        D3DLOCKED_RECT data{};
        image->LockRect(0, &data, nullptr, 0);
        for (UINT y = 0; y < H; ++y)
            for (UINT x = 0; x < W; ++x)
                ((DWORD *)((BYTE *)data.pBits + y * data.Pitch))[x] =
                    mix(foregroundColor, skyColor, x < X ? 1.f : x == X ? coverage : 0.f);
        image->UnlockRect(0);
        depth->LockRect(0, &data, nullptr, 0);
        for (UINT y = 0; y < H; ++y)
            for (UINT x = 0; x < W; ++x)
                ((float *)((BYTE *)data.pBits + y * data.Pitch))[x] = x <= X ? a + b / 40.f : 1.f;
        depth->UnlockRect(0);
    };
    // Model resolved 8x-MSAA color with one resolved foreground depth sample.
    sky->LockRect(0, &lock, nullptr, 0);
    for (UINT y = 0; y < H; ++y)
        for (UINT x = 0; x < W; ++x)
            ((DWORD *)((BYTE *)lock.pBits + y * lock.Pitch))[x] = skyColor;
    sky->UnlockRect(0);
    meanSky->LockRect(0, &lock, nullptr, 0);
    *(DWORD *)lock.pBits = 0xff8099bb;
    meanSky->UnlockRect(0);
    for (float coverage : {.125f, .25f, .5f, .75f}) {
        silhouette(coverage);
        sceneConstants();
        constants[7][1] = constants[53][0] = constants[54][0] = 1;
        constants[4][0] = .003f;
        constants[5][0] = .56f; constants[5][1] = .66f; constants[5][2] = .76f;
        constants[54][2] = 80;
        constants[55][0] = 80.f / 255; constants[55][1] = 96.f / 255; constants[55][2] = 128.f / 255;
        CHECK(draw(scenePS));
        auto edge = Read(d);
        DWORD expected = mix(edge[Y * W + X - 2], skyColor, coverage);
        for (int shift : {0, 8, 16})
            CHECK(abs(int((edge[Y * W + X] >> shift) & 255) - int((expected >> shift) & 255)) <= 2);
    }
    // Colors matching native fog are not evidence of geometry: moving sky glows and
    // cloud strips with clear depth must never be recolored as distant opaque objects.
    image->LockRect(0, &lock, nullptr, 0);
    for (UINT y = 0; y < H; ++y)
        for (UINT x = 0; x < W; ++x)
            ((DWORD *)((BYTE *)lock.pBits + y * lock.Pitch))[x] =
                mix(0xff506080, skyColor, (x % 8) / 7.f);
    image->UnlockRect(0);
    depth->LockRect(0, &lock, nullptr, 0);
    for (UINT y = 0; y < H; ++y)
        for (UINT x = 0; x < W; ++x)
            ((float *)((BYTE *)lock.pBits + y * lock.Pitch))[x] = 1;
    depth->UnlockRect(0);
    constants[7][1] = 0;
    CHECK(draw(scenePS)); auto skyGlows = Read(d);
    constants[7][1] = 1;
    CHECK(draw(scenePS)); auto fogSkyGlows = Read(d);
    CHECK(skyGlows == fogSkyGlows);
    silhouette(.5f);
    sceneConstants();
    constants[6][0] = 5; constants[6][1] = 1; constants[6][2] = 6;
    CHECK(draw(dofPS));
    auto blurredSilhouette = Read(d);
    const int spread = int(skyColor & 255) - int(blurredSilhouette[Y * W + X + 1] & 255);
    CHECK(spread > 5); // Defocused leaves spread into the sky, rather than retaining a cut-out edge.
    constants[6][0] = 40;
    CHECK(draw(dofPS));
    auto focusedSilhouette = Read(d);
    const int focusedSpread = int(skyColor & 255) - int(focusedSilhouette[Y * W + X + 1] & 255);
    CHECK(focusedSpread <= 3);
    printf("Skyline coverage preserved; DoF spread=%d focused spread=%d\n", spread, focusedSpread);
    // Bloom can add an outer halo without bleaching an already bright colored core.
    fillImage(0xfff08020);
    memset(constants, 0, sizeof(constants));
    constants[0][0] = 1;
    constants[3][0] = 1;
    CHECK(draw(postPS));
    auto bloom = Read(d);
    CHECK(bloom.size() == W * H);
    CHECK((bloom[Y * W + X] & 255) < 0x20 + 12 && ((bloom[Y * W + X] >> 8) & 255) < 0x80 + 12);
    CHECK((bloom[(Y + 8) * W + X] & 255) > 0x30 + 20);
    // Subpixel leaf gaps contribute fractional coverage, not binary stamps repeated down a ray.
    auto raySource = [&](bool striped, DWORD skyColor) {
        D3DLOCKED_RECT data{};
        image->LockRect(0, &data, nullptr, 0);
        for (UINT y = 0; y < H; ++y)
            for (UINT x = 0; x < W; ++x)
                ((DWORD *)((BYTE *)data.pBits + y * data.Pitch))[x] = skyColor;
        image->UnlockRect(0);
        depth->LockRect(0, &data, nullptr, 0);
        for (UINT y = 0; y < H; ++y)
            for (UINT x = 0; x < W; ++x)
                ((float *)((BYTE *)data.pBits + y * data.Pitch))[x] = striped && (x & 1) ? .5f : 1.f;
        depth->UnlockRect(0);
        sceneConstants();
        constants[8][0] = (X + .5f) / W;
        constants[8][1] = (Y + .5f) / H;
        constants[8][2] = 1;
        constants[18][2] = 4.f / W;
        constants[18][3] = 4.f / H;
        return draw(raySourcePS);
    };
    CHECK(raySource(false, 0xffffffff));
    auto clearSun = Read(d);
    CHECK(raySource(true, 0xffffffff));
    auto leaves = Read(d);
    const unsigned clear = clearSun[Y * W + X] & 255, partial = leaves[Y * W + X] & 255;
    CHECK(clear > 180 && partial > clear * .3f && partial < clear * .7f);
    CHECK(raySource(false, 0xff202030));
    for (DWORD pixel : Read(d))
        CHECK((pixel & 0xffffff) == 0); // No source from an artificial nighttime light spot.
    printf("Filtered sun coverage: clear=%u leaves=%u\n", clear, partial);
    puts("PASS: weak-depth light rejection, sharp emissive DoF cores and colored bloom headroom");
    return 0;
}
