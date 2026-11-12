#include "sfx.hpp"
#include <cstdio>
#include <array>

#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            printf("FAIL %d: %s\n", __LINE__, #x);                                                           \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
namespace {
constexpr UINT W = 320, H = 240;
std::vector<DWORD> Read(IDirect3DDevice9 *d) {
    Com<IDirect3DSurface9> target, resolved, read;
    d->GetRenderTarget(0, target.out());
    D3DSURFACE_DESC desc{};
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
void Save(const std::vector<DWORD> &pixels, const wchar_t *path) {
    FILE *f = _wfopen(path, L"wb");
    if (!f || pixels.size() != W * H) {
        if (f)
            fclose(f);
        return;
    }
    BITMAPFILEHEADER file{};
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
    file.bfSize = file.bfOffBits + W * H * 4;
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info);
    info.biWidth = W;
    info.biHeight = -int(H);
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    fwrite(&file, sizeof(file), 1, f);
    fwrite(&info, sizeof(info), 1, f);
    fwrite(pixels.data(), 4, pixels.size(), f);
    fclose(f);
}
unsigned Difference(const std::vector<DWORD> &a, const std::vector<DWORD> &b, int tolerance = 3) {
    if (a.size() != W * H || b.size() != W * H)
        return ~0u;
    unsigned n = 0;
    for (size_t i = 0; i < a.size(); ++i)
        for (int shift : {0, 8, 16})
            if (abs(int((a[i] >> shift) & 255) - int((b[i] >> shift) & 255)) > tolerance) {
                ++n;
                break;
            }
    return n;
}
} // namespace
int main(int argc, char **) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FENBSfxTest";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"SFX material fixture", WS_OVERLAPPEDWINDOW, 0, 0, W, H,
                                nullptr, nullptr, wc.hInstance, nullptr);
    Com<IDirect3D9> root;
    root.p = Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(root.p);
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth = W;
    pp.BackBufferHeight = H;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.hDeviceWindow = window;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D16;
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
    SfxFX sfx(d);
    Settings settings;
    Com<IDirect3DTexture9> texture;
    CHECK(
        SUCCEEDED(d->CreateTexture(64, 64, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, texture.out(), nullptr)));
    D3DLOCKED_RECT lock{};
    CHECK(SUCCEEDED(texture->LockRect(0, &lock, nullptr, 0)));
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            float r = std::hypot((x - 31.5f) / 31.5f, (y - 31.5f) / 31.5f);
            unsigned c = unsigned(std::exp(-std::pow((r - .6f) * 22, 2)) * 255);
            ((DWORD *)((BYTE *)lock.pBits + y * lock.Pitch))[x] = 0xff000000 | (c << 16) | (c << 8) | c;
        }
    texture->UnlockRect(0);
    DWORD tag = 1;
    texture->SetPrivateData(SfxTag, &tag, 4, 0);
    struct V {
        float x, y, z;
        DWORD color;
        float u, v;
    };
    V quad[] = {{-1, 1, 0, 0x80ff8040, 0, 0},
                {1, 1, 0, 0x80ff8040, 1, 0},
                {-1, -1, 0, 0x80ff8040, 0, 1},
                {1, -1, 0, 0x80ff8040, 1, 1}};
    struct WakeV {
        float x, y, z, u, v;
    };
    WakeV wake[] = {{-1, 1, 0, 0, 0}, {1, 1, 0, 1, 0}, {-1, -1, 0, 0, 1}, {1, -1, 0, 1, 1}};
    D3DMATRIX id{};
    id._11 = id._22 = id._33 = id._44 = 1;
    D3DMATRIX world = id, view = id, projection{};
    view._43 = 3;
    projection._11 = 1.5f;
    projection._22 = 2;
    projection._33 = 100.f / 99.9f;
    projection._34 = 1;
    projection._43 = -.1f * projection._33;
    bool water = false, occlude = false;
    DWORD fade = 0x80ff8040;
    auto setup = [&] {
        d->SetVertexShader(nullptr);
        d->SetPixelShader(nullptr);
        d->SetFVF(water ? 0x102 : 0x142);
        d->SetTransform(D3DTS_WORLD, &world);
        d->SetTransform(D3DTS_VIEW, &view);
        d->SetTransform(D3DTS_PROJECTION, &projection);
        d->SetTransform(D3DTS_TEXTURE0, &id);
        d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        d->SetRenderState(D3DRS_LIGHTING, FALSE);
        d->SetRenderState(D3DRS_FOGENABLE, FALSE);
        d->SetRenderState(D3DRS_ZENABLE, TRUE);
        d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        d->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        d->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
        d->SetTexture(0, texture);
        d->SetTexture(1, nullptr);
        d->SetTextureStageState(0, D3DTSS_COLOROP, water ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE);
        d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        d->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        d->SetTextureStageState(0, D3DTSS_ALPHAARG1, water ? D3DTA_DIFFUSE : D3DTA_TEXTURE);
        d->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        d->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        d->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        d->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        for (auto key : {D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER})
            d->SetSamplerState(0, key, D3DTEXF_LINEAR);
    };
    auto background = [&] {
        d->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff304050, 1, 0);
        std::vector<D3DRECT> tiles;
        for (int y = 0; y < int(H); y += 12)
            for (int x = 0; x < int(W); x += 12)
                if ((x / 12 + y / 12) % 2)
                    tiles.push_back({x, y, x + 12, y + 12});
        d->Clear(UINT(tiles.size()), tiles.data(), D3DCLEAR_TARGET, 0xff506060, 1, 0);
        if (occlude) {
            D3DRECT r{0, 0, int(W / 2), int(H)};
            d->Clear(1, &r, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff202020, 0, 0);
        }
    };
    auto render = [&](bool enhance, bool canSnapshot = true) {
        sfx.beginFrame(settings.sfx);
        background();
        setup();
        d->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        sfx.observe();
        d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        d->BeginScene();
        bool applied = enhance && sfx.begin(settings, water, canSnapshot);
        for (auto &v : quad)
            v.color = fade;
        HRESULT result = water ? d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, wake, sizeof(WakeV))
                               : d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(V));
        sfx.end(true);
        d->EndScene();
        if (FAILED(result))
            puts("Draw failed");
        return applied;
    };
    settings.sfxIntensity = 1;
    settings.sfxGlow = 0;
    settings.sfxSaturation = 1;
    settings.sfxSoftness = 0;
    render(false);
    auto native = Read(d);
    CHECK(render(true));
    auto neutral = Read(d);
    printf("Neutral parity pixels: %u\n", Difference(native, neutral));
    CHECK(Difference(native, neutral) == 0);
    settings.sfxIntensity = 1.7f;
    settings.sfxGlow = .8f;
    settings.sfxSaturation = 1.2f;
    settings.sfxSoftness = .5f;
    CHECK(render(true));
    auto styled = Read(d);
    CHECK(Difference(native, styled) > 100);
    Save(native, L"build/sfx-native.bmp");
    Save(styled, L"build/sfx-styled.bmp");
    DWORD value = 0;
    d->GetRenderState(D3DRS_DESTBLEND, &value);
    CHECK(value == D3DBLEND_ONE);
    d->GetFVF(&value);
    CHECK(value == 0x142);
    Com<IDirect3DVertexShader9> vs;
    Com<IDirect3DPixelShader9> ps;
    d->GetVertexShader(vs.out());
    d->GetPixelShader(ps.out());
    CHECK(!vs.p && !ps.p);
    fade = 0x00ff8040;
    render(false);
    auto faded = Read(d);
    CHECK(render(true));
    CHECK(Difference(faded, Read(d)) == 0);
    fade = 0x80ff8040;
    tag = 4;
    texture->SetPrivateData(SfxTag, &tag, 4, 0);
    CHECK(!render(true));
    tag = 1;
    texture->SetPrivateData(SfxTag, &tag, 4, 0);
    setup();
    sfx.beginFrame(true);
    d->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    sfx.observe();
    d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    d->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    CHECK(!sfx.begin(settings, false));
    setup();
    auto portrait = view;
    portrait._11 = .9f;
    d->SetTransform(D3DTS_VIEW, &portrait);
    CHECK(!sfx.begin(settings, false));
    occlude = true;
    render(false);
    auto blocked = Read(d);
    CHECK(render(true));
    auto occluded = Read(d);
    for (UINT y = 0; y < H; ++y)
        for (UINT x = 0; x < W / 2; ++x)
            CHECK(blocked[y * W + x] == occluded[y * W + x]);
    occlude = false;
    water = true;
    tag = 3;
    texture->SetPrivateData(SfxTag, &tag, 4, 0);
    world = id;
    world._22 = 0;
    world._23 = 1;
    world._32 = -1;
    world._33 = 0;
    view = id;
    view._22 = 0;
    view._23 = -1;
    view._32 = 1;
    view._33 = 0;
    view._43 = 3;
    render(false);
    auto whiteRings = Read(d);
    CHECK(render(true));
    auto refracted = Read(d);
    CHECK(Difference(whiteRings, refracted) > 500);
    CHECK(sfx.bytes() == W * H * 4);
    Save(whiteRings, L"build/wake-native.bmp");
    Save(refracted, L"build/wake-refracted.bmp");
    // Verify the actual native water-ring material, without requiring assets in clean checkouts.
    if (FILE *file = _wfopen(L"X:\\Games\\Homesickness\\SFX\\Texture\\water.dds", L"rb")) {
        DWORD header[32]{};
        CHECK(fread(header, 1, 128, file) == 128);
        CHECK(header[4] == 128 && header[3] == 128 && header[21] == D3DFMT_DXT1);
        std::array<BYTE, 8192> pixels{};
        CHECK(fread(pixels.data(), 1, pixels.size(), file) == pixels.size());
        fclose(file);
        CHECK(SUCCEEDED(
            d->CreateTexture(128, 128, 1, 0, D3DFMT_DXT1, D3DPOOL_MANAGED, texture.out(), nullptr)));
        CHECK(SUCCEEDED(texture->LockRect(0, &lock, nullptr, 0)));
        for (unsigned row = 0; row < 32; ++row)
            memcpy(static_cast<BYTE *>(lock.pBits) + row * lock.Pitch, pixels.data() + row * 256, 256);
        texture->UnlockRect(0);
        CHECK(render(true));
        DWORD actualKind = 0, size = 4;
        CHECK(SUCCEEDED(texture->GetPrivateData(SfxTag, &actualKind, &size)) && actualKind == 3);
        Save(Read(d), L"build/wake-native-material-refracted.bmp");
        puts("Native water DDS fingerprint and replacement PASS");
    }
    d->GetRenderState(D3DRS_DESTBLEND, &value);
    CHECK(value == D3DBLEND_ONE);
    // A late draw must never take its refraction source from the already drawn HUD.
    CHECK(!render(true, false));
    settings.sfxWake = 0;
    CHECK(!render(true));
    settings.sfxWake = .7f;
    sfx.invalidate();
    CHECK(sfx.bytes() == 0);
    CHECK(render(true));
    sfx.invalidate();
    CHECK(sfx.bytes() == 0);
    sfx.beginFrame(false);
    CHECK(sfx.bytes() == 0);
    puts(
        "SFX: neutral parity, style, fade, HUD/camera/depth guards, refractive wake and resource reset PASS");
    return 0;
}
