#include "effects.hpp"
#include "state_guard.hpp"
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
struct SceneFXFixture {
    static IDirect3DVertexShader9 *skyProgram(SceneFX &fx) {
        return fx.skyVS_;
    }
    static float lightMaximum(SceneFX &fx, IDirect3DDevice9 *d) {
        Com<IDirect3DSurface9> read;
        if (FAILED(d->CreateOffscreenPlainSurface(fx.localField_.width, fx.localField_.height,
                                                  D3DFMT_A16B16G16R16F, D3DPOOL_SYSTEMMEM, read.out(),
                                                  nullptr)) ||
            FAILED(d->GetRenderTargetData(fx.localField_.surface, read)))
            return -1;
        D3DLOCKED_RECT lock{};
        if (FAILED(read->LockRect(&lock, nullptr, D3DLOCK_READONLY)))
            return -1;
        auto half = [](unsigned h) {
            float v = (h & 1023) / 1024.f;
            int e = (h >> 10) & 31;
            return (h & 0x8000 ? -1.f : 1.f) * (e ? std::ldexp(1 + v, e - 15) : std::ldexp(v, -14));
        };
        float maximum = 0;
        for (UINT y = 0; y < fx.localField_.height; ++y)
            for (UINT x = 0; x < fx.localField_.width; ++x) {
                auto *p = (unsigned short *)((BYTE *)lock.pBits + y * lock.Pitch) + x * 4;
                maximum = std::max(maximum, half(p[0]) + half(p[1]) + half(p[2]));
            }
        read->UnlockRect();
        return maximum;
    }
};
namespace {
constexpr UINT W = 640, H = 400;
std::vector<DWORD> Read(IDirect3DDevice9 *d) {
    Com<IDirect3DSurface9> rt, read, resolved;
    d->GetRenderTarget(0, rt.out());
    D3DSURFACE_DESC desc{};
    rt->GetDesc(&desc);
    IDirect3DSurface9 *source = rt;
    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
        if (FAILED(d->CreateRenderTarget(W, H, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, resolved.out(),
                                         nullptr)) ||
            FAILED(d->StretchRect(rt, nullptr, resolved, nullptr, D3DTEXF_NONE)))
            return {};
        source = resolved;
    }
    if (FAILED(d->CreateOffscreenPlainSurface(W, H, desc.Format, D3DPOOL_SYSTEMMEM, read.out(), nullptr)) ||
        FAILED(d->GetRenderTargetData(source, read)))
        return {};
    D3DLOCKED_RECT l{};
    if (FAILED(read->LockRect(&l, nullptr, D3DLOCK_READONLY)))
        return {};
    std::vector<DWORD> out(W * H);
    for (UINT y = 0; y < H; ++y)
        memcpy(out.data() + y * W, (BYTE *)l.pBits + y * l.Pitch, W * 4);
    read->UnlockRect();
    return out;
}
bool Save(const std::vector<DWORD> &p, const wchar_t *path) {
    FILE *f = _wfopen(path, L"wb");
    if (!f)
        return false;
    BITMAPFILEHEADER h{};
    h.bfType = 0x4d42;
    h.bfOffBits = sizeof(h) + sizeof(BITMAPINFOHEADER);
    h.bfSize = h.bfOffBits + W * H * 4;
    BITMAPINFOHEADER i{};
    i.biSize = sizeof(i);
    i.biWidth = W;
    i.biHeight = -LONG(H);
    i.biPlanes = 1;
    i.biBitCount = 32;
    bool ok = fwrite(&h, 1, sizeof(h), f) == sizeof(h) && fwrite(&i, 1, sizeof(i), f) == sizeof(i) &&
              fwrite(p.data(), 4, p.size(), f) == p.size();
    fclose(f);
    return ok;
}
unsigned Difference(const std::vector<DWORD> &a, const std::vector<DWORD> &b) {
    unsigned n = 0;
    for (size_t i = 0; i < a.size(); ++i)
        n += a[i] != b[i];
    return n;
}
int Luma(DWORD p) {
    return int(p & 255) + int((p >> 8) & 255) + int((p >> 16) & 255);
}
} // namespace
int main(int argc, char **) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FENBSceneFixture";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"Scene FX fixture", WS_OVERLAPPEDWINDOW, 0, 0, W, H,
                                nullptr, nullptr, wc.hInstance, nullptr);
    Com<IDirect3D9> root;
    root.p = Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(root.p);
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.hDeviceWindow = window;
    pp.BackBufferWidth = W;
    pp.BackBufferHeight = H;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24X8;
    if (argc > 1)
        pp.MultiSampleType = D3DMULTISAMPLE_8_SAMPLES;
    Com<IDirect3DDevice9> d;
    CHECK(SUCCEEDED(
        root->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, d.out())));
    D3DMATRIX id{};
    id._11 = id._22 = id._33 = id._44 = 1;
    D3DMATRIX view{};
    view._11 = 1;
    view._22 = .9486833f;
    view._23 = -.3162278f;
    view._32 = .3162278f;
    view._33 = .9486833f;
    view._42 = -1.264911f;
    view._43 = 8.854377f;
    view._44 = 1;
    D3DMATRIX projection{};
    projection._11 = 1.1f;
    projection._22 = 1.76f;
    projection._33 = 200 / 199.9f;
    projection._34 = 1;
    projection._43 = -.1f * 200 / 199.9f;
    Effects fx(d);
    Settings settings;
    settings.bloom = settings.color = false;
    Com<IDirect3DTexture9> emitter;
    CHECK(SUCCEEDED(d->CreateTexture(2, 2, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, emitter.out(), nullptr)));
    D3DLOCKED_RECT lock{};
    CHECK(SUCCEEDED(emitter->LockRect(0, &lock, nullptr, 0)));
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
            *reinterpret_cast<DWORD *>((BYTE *)lock.pBits + y * lock.Pitch + x * 4) = 0xffffa040;
    emitter->UnlockRect(0);
    DWORD tag = 1;
    emitter->SetPrivateData(EmitterTag, &tag, 4, 0); // Synthetic recognized source.
    bool emit = false, nativePoint = true, occluded = false;
    float emitterFade = 1;
    DWORD solidColor = 0xff303030;
    auto disableEffect = [&](int group) {
        settings.resetSceneSection(group);
        switch (group) {
        case 0:
            settings.ssao = false;
            break;
        case 1:
            settings.sunRays = false;
            break;
        case 3:
            settings.fog = false;
            break;
        case 4:
            settings.dof = false;
            break;
        case 5:
            settings.adaptation = false;
            break;
        case 6:
            settings.localLighting = false;
            break;
        case 8:
            settings.fxaa = false;
            break;
        }
    };
    bool darkScene = false, balanceScene = false;
    struct Vertex {
        float x, y, z;
        DWORD color;
    };
    auto render = [&](bool solid = false) {
        fx.sceneFX().beginFrame();
        d->SetVertexShader(nullptr);
        d->SetPixelShader(nullptr);
        d->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        d->SetTransform(D3DTS_WORLD, &id);
        d->SetTransform(D3DTS_VIEW, &view);
        d->SetTransform(D3DTS_PROJECTION, &projection);
        d->SetRenderState(D3DRS_ZENABLE, TRUE);
        d->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        d->SetRenderState(D3DRS_LIGHTING, FALSE);
        d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        d->SetRenderState(D3DRS_FOGENABLE, FALSE);
        d->SetTexture(0, nullptr);
        d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        d->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        D3DLIGHT9 sun{};
        sun.Type = D3DLIGHT_DIRECTIONAL;
        sun.Direction = {-.1f, -.1f, -1};
        sun.Diffuse = darkScene ? D3DCOLORVALUE{.04f, .04f, .06f, 1} : D3DCOLORVALUE{.8f, .75f, .65f, 1};
        d->SetLight(0, &sun);
        d->LightEnable(0, TRUE);
        d->SetRenderState(D3DRS_AMBIENT, darkScene ? 0x08080c : 0x303030);
        D3DLIGHT9 light{};
        light.Type = D3DLIGHT_POINT;
        light.Position = {0, 3, 10};
        light.Diffuse = {2, .8f, .2f, 1};
        light.Range = 12;
        light.Attenuation0 = 1;
        d->SetLight(1, &light);
        d->LightEnable(1, nativePoint);
        fx.sceneFX().observe(settings);
        d->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                 solid ? solidColor : (balanceScene ? 0xff101827 : 0xff8faec4), 1, 0);
        if (FAILED(d->BeginScene()))
            return false;
        if (!solid) {
            std::vector<Vertex> floor;
            for (int z = -5; z < 100; ++z)
                for (int x = -15; x < 15; ++x) {
                    DWORD c = darkScene ? 0xff25211c : ((x + z) & 1 ? 0xff7c725a : 0xffb2a581);
                    for (auto v : std::initializer_list<Vertex>{{float(x), 0, float(z), c},
                                                                {float(x), 0, float(z + 1), c},
                                                                {float(x + 1), 0, float(z), c},
                                                                {float(x + 1), 0, float(z), c},
                                                                {float(x), 0, float(z + 1), c},
                                                                {float(x + 1), 0, float(z + 1), c}})
                        floor.push_back(v);
                }
            d->DrawPrimitiveUP(D3DPT_TRIANGLELIST, UINT(floor.size() / 3), floor.data(), sizeof(Vertex));
            Vertex wall[] = {{-2, 0, 18, 0xff948674},
                             {2, 0, 18, 0xff948674},
                             {-2, 7, 18, 0xff948674},
                             {2, 7, 18, 0xff948674}};
            if (balanceScene)
                for (auto &v : wall)
                    v.color = 0xffc0c0c0;
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, wall, sizeof(Vertex));
        }
        if (emit) {
            struct E {
                float x, y, z, u, v;
            };
            float z = occluded ? 24.f : 9.f;
            E quad[] = {
                {-.6f, 2.5f, z, 0, 1}, {-.6f, 3.5f, z, 0, 0}, {.6f, 2.5f, z, 1, 1}, {.6f, 3.5f, z, 1, 0}};
            d->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
            d->SetTexture(0, emitter);
            d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
            d->SetRenderState(D3DRS_TEXTUREFACTOR, (DWORD(emitterFade * 255) << 24) | 0xffffff);
            d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
            D3DMATERIAL9 mat{};
            mat.Diffuse = {1, 1, 1, 1};
            d->SetMaterial(&mat);
            fx.sceneFX().captureEmitter(
                settings, [&]() { return d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(E)); });
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(E));
        }
        Com<IDirect3DSurface9> bb;
        d->GetRenderTarget(0, bb.out());
        bool ok = true;
        {
            StateGuard state(d);
            if (fx.prepare(bb, settings)) {
                state.isolateTargets();
                fx.draw(bb, settings);
            } else if (settings.postEnabled())
                ok = false;
        }
        // Simulated HUD after the world processing must retain its exact RGB.
        D3DRECT rect{8, 8, 40, 28};
        d->Clear(1, &rect, D3DCLEAR_TARGET, 0xff40a0e0, 1, 0);
        d->EndScene();
        return ok;
    };
    CHECK(render());
    auto original = Read(d);
    CHECK(original.size() == W * H);
    CHECK(Save(original, L"build/scene-original.bmp"));
    settings.fog = true;
    settings.fogDensity = .025f;
    settings.fogStart = 0;
    settings.fogFalloff = 0;
    CHECK(render());
    if (!fx.sceneFX().depthReady()) {
        puts("SKIP: optional depth backend unavailable");
        return 0;
    }
    auto fog = Read(d);
    CHECK(Difference(original, fog) > 1000);
    CHECK(fog[12 * W + 12] == original[12 * W + 12]);
    CHECK(Save(fog, L"build/scene-fog.bmp"));
    disableEffect(3);
    settings.ssao = true;
    settings.aoStrength = 2;
    settings.aoRadius = 3;
    CHECK(render());
    auto ao = Read(d);
    CHECK(Difference(original, ao) > 100);
    CHECK(Save(ao, L"build/scene-ssao.bmp"));
    disableEffect(0);
    settings.dof = true;
    settings.focusDistance = 8;
    settings.focusRange = 4;
    settings.dofRadius = 10;
    CHECK(render());
    auto dof = Read(d);
    CHECK(Difference(original, dof) > 1000);
    CHECK(dof[12 * W + 12] == original[12 * W + 12]);
    CHECK(Save(dof, L"build/scene-dof.bmp"));
    disableEffect(4);
    settings.sunRays = true;
    settings.rayStrength = 2;
    CHECK(render());
    auto rays = Read(d);
    CHECK(Difference(original, rays) > 100);
    CHECK(Save(rays, L"build/scene-rays.bmp"));
    darkScene = true;
    CHECK(render());
    CHECK(!fx.sceneFX().sunOnScreen()); // A bright blue channel does not make a night light the sun.
    darkScene = false;
    disableEffect(1);
    settings.localLighting = true;
    CHECK(render());
    auto lit = Read(d);
    CHECK(fx.sceneFX().lightCount() == 1);
    CHECK(Difference(original, lit) > 100);
    CHECK(lit[50 * W + 20] == original[50 * W + 20]);
    CHECK(Save(lit, L"build/scene-lighting.bmp"));
    emit = true;
    nativePoint = false;
    settings.localLighting = false;
    CHECK(render());
    auto emitterOff = Read(d);
    settings.localLighting = true;
    CHECK(render());
    auto emitterOn = Read(d);
    unsigned illuminated = 0;
    for (UINT y = H / 2; y < H; ++y)
        for (UINT x = W / 4; x < W * 3 / 4; ++x)
            illuminated += Luma(emitterOn[y * W + x]) > Luma(emitterOff[y * W + x]) + 3;
    printf("Emissive geometry receiver pixels: %u\n", illuminated);
    CHECK(illuminated > 100);
    CHECK(Save(emitterOn, L"build/scene-emissive-lighting.bmp"));
    float fullEmission = SceneFXFixture::lightMaximum(fx.sceneFX(), d);
    emitterFade = .25f;
    CHECK(render());
    float fadedEmission = SceneFXFixture::lightMaximum(fx.sceneFX(), d);
    CHECK(fadedEmission > 0 && fadedEmission < fullEmission * .4f);
    emitterFade = 0;
    CHECK(render());
    CHECK(SceneFXFixture::lightMaximum(fx.sceneFX(), d) == 0);
    emitterFade = 1;
    occluded = true;
    settings.localLighting = true;
    settings.localStrength = 1;
    CHECK(render());
    CHECK(SceneFXFixture::lightMaximum(fx.sceneFX(), d) == 0);
    // A lamp must visibly light a dark night receiver, not just multiply its nearly black color.
    occluded = false;
    darkScene = true;
    settings.localLighting = false;
    CHECK(render());
    auto nightOff = Read(d);
    settings.localLighting = true;
    settings.localStrength = 2;
    settings.localRange = 1.55f;
    CHECK(render());
    auto nightOn = Read(d);
    unsigned nightLit = 0;
    for (UINT y = H / 2; y < H; ++y)
        for (UINT x = W / 4; x < W * 3 / 4; ++x)
            nightLit += Luma(nightOn[y * W + x]) > Luma(nightOff[y * W + x]) + 30;
    CHECK(nightLit > 100);
    CHECK(nightOn[50 * W + 20] == nightOff[50 * W + 20]); // Empty sky is not a receiver.
    CHECK(nightOn[12 * W + 12] == nightOff[12 * W + 12]); // HUD remains exact.
    CHECK(Save(nightOff, L"build/scene-night-light-off.bmp"));
    CHECK(Save(nightOn, L"build/scene-night-light-on.bmp"));
    darkScene = false;
    emit = occluded = false;
    nativePoint = true;
    disableEffect(6);
    settings.fxaa = true;
    CHECK(render());
    auto antialias = Read(d);
    CHECK(Difference(original, antialias) > 1000);
    CHECK(antialias[12 * W + 12] == original[12 * W + 12]);
    CHECK(Save(antialias, L"build/scene-fxaa.bmp"));
    settings.fog = true;
    settings.fogDensity = .01f;
    CHECK(render());
    CHECK(Save(Read(d), L"build/scene-fog-fxaa.bmp"));
    disableEffect(3);
    disableEffect(8);
    settings.smaa = true;
    CHECK(render());
    auto morphological = Read(d);
    CHECK(Difference(original, morphological) > 100);
    CHECK(morphological[12 * W + 12] == original[12 * W + 12]);
    CHECK(morphological[50 * W + 20] == original[50 * W + 20]);
    CHECK(Save(morphological, L"build/scene-smaa.bmp"));
    settings.fog = true;
    CHECK(render());
    CHECK(Save(Read(d), L"build/scene-fog-smaa.bmp"));
    disableEffect(3);
    settings.smaa = false;
    disableEffect(6);
    settings.adaptation = true;
    settings.adaptDark = 5;
    settings.adaptBright = 5;
    CHECK(render(true));
    auto first = Read(d);
    CHECK(Luma(first[H / 2 * W + W / 2]) >= 144 && Luma(first[H / 2 * W + W / 2]) <= 156);
    for (int frame = 0; frame < 80; ++frame) {
        Sleep(16);
        CHECK(render(true));
    }
    auto adapted = Read(d);
    CHECK(Luma(adapted[H / 2 * W + W / 2]) > Luma(first[H / 2 * W + W / 2]) + 15);
    CHECK(Luma(adapted[H / 2 * W + W / 2]) < 3 * 75);
    CHECK(adapted[12 * W + 12] == original[12 * W + 12]);
    CHECK(Save(adapted, L"build/scene-adaptation.bmp"));
    solidColor = 0xffc0c0c0;
    CHECK(render(true));
    auto brightFirst = Read(d);
    for (int frame = 0; frame < 80; ++frame) {
        Sleep(16);
        CHECK(render(true));
    }
    auto brightAdapted = Read(d);
    CHECK(Luma(brightAdapted[H / 2 * W + W / 2]) < Luma(brightFirst[H / 2 * W + W / 2]) - 30);
    CHECK(Luma(brightAdapted[H / 2 * W + W / 2]) > 300);
    solidColor = 0xff303030;
    disableEffect(5);
    CHECK(render(true));
    auto neutral = Read(d);
    CHECK((neutral[H / 2 * W + W / 2] & 0xffffff) == 0x303030);
    balanceScene = darkScene = true;
    nativePoint = false;
    CHECK(render());
    auto balanceOff = Read(d);
    settings.lightingBalance = true;
    settings.lightingBalanceStrength = .8f;
    CHECK(render());
    auto balanceOn = Read(d);
    unsigned lifted = 0, restrained = 0;
    for (UINT i = 0; i < W * H; ++i) {
        int before = Luma(balanceOff[i]), after = Luma(balanceOn[i]);
        lifted += before > 0 && before < 150 && after > before + 5;
        restrained += before > 400 && after < before - 20;
    }
    CHECK(lifted > 1000 && restrained > 100);
    CHECK(balanceOn[12 * W + 12] == balanceOff[12 * W + 12]);
    CHECK(Save(balanceOff, L"build/scene-balance-off.bmp"));
    CHECK(Save(balanceOn, L"build/scene-balance-on.bmp"));
    settings.lightingBalanceStrength = 0;
    CHECK(render());
    auto zeroBalance = Read(d);
    for (UINT i = 0; i < W * H; ++i)
        CHECK(std::abs(Luma(zeroBalance[i]) - Luma(balanceOff[i])) <= 3);
    settings.lightingBalance = false;
    balanceScene = darkScene = false;
    nativePoint = true;
    Com<IDirect3DTexture9> skyArt;
    CHECK(SUCCEEDED(d->CreateTexture(64, 64, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, skyArt.out(), nullptr)));
    CHECK(SUCCEEDED(skyArt->LockRect(0, &lock, nullptr, 0)));
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            ((DWORD *)((BYTE *)lock.pBits + y * lock.Pitch))[x] =
                ((x / 8 + y / 8) % 3) ? 0xff3098db : 0xffe0e8eb;
    skyArt->UnlockRect(0);
    auto materialFrame = [&](bool sky, bool improve, bool day, bool blueNight = false) {
        fx.sceneFX().beginFrame();
        d->SetVertexShader(nullptr);
        d->SetPixelShader(nullptr);
        d->SetTransform(D3DTS_WORLD, &id);
        d->SetTransform(D3DTS_VIEW, sky ? &id : &view);
        d->SetTransform(D3DTS_PROJECTION, &projection);
        D3DVIEWPORT9 vp{0, 0, W, H, 0, 1};
        d->SetViewport(&vp);
        d->SetRenderState(D3DRS_ZENABLE, sky ? FALSE : TRUE);
        d->SetRenderState(D3DRS_ZWRITEENABLE, sky ? FALSE : TRUE);
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        d->SetRenderState(D3DRS_FOGENABLE, FALSE);
        d->SetRenderState(D3DRS_LIGHTING, sky ? FALSE : TRUE);
        d->SetRenderState(D3DRS_AMBIENT, 0xff080810);
        d->SetTexture(0, sky ? skyArt.p : emitter.p);
        d->SetTextureStageState(0, D3DTSS_COLOROP, sky ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE);
        d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        d->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        d->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        d->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        d->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        D3DMATERIAL9 material{};
        material.Ambient = material.Diffuse = {1, 1, 1, 1};
        d->SetMaterial(&material);
        D3DLIGHT9 sun{};
        sun.Type = D3DLIGHT_DIRECTIONAL;
        sun.Direction = {0, 0, 1};
        float value = day ? .8f : .04f;
        sun.Diffuse = {value, value, value, 1};
        if (blueNight)
            sun.Diffuse = {.04f, .04f, .6f, 1};
        d->SetLight(0, &sun);
        d->LightEnable(0, TRUE);
        Settings s;
        s.sky = improve;
        s.nightFoliage = .8f;
        fx.sceneFX().observe(s);
        d->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1, 0);
        d->BeginScene();
        bool ok = true;
        if (sky) {
            struct V {
                float x, y, z;
                DWORD color;
                float u, v;
            };
            V q[] = {{-2, -1, 1, 0xff000000, 0, 1},
                     {-2, 1, 1, 0xff000000, 0, 0},
                     {2, -1, 1, 0xff000000, 1, 1},
                     {2, 1, 1, 0xff000000, 1, 0}};
            d->SetFVF(0x142);
            if (improve)
                ok = fx.sceneFX().beginSky(s, 3);
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(V));
            fx.sceneFX().endSky();
        } else {
            struct V {
                float x, y, z, nx, ny, nz, u, v;
            };
            V q[] = {{-3, 1, 10, 0, 0, -1, 0, 1},
                     {-3, 7, 10, 0, 0, -1, 0, 0},
                     {3, 1, 10, 0, 0, -1, 1, 1},
                     {3, 7, 10, 0, 0, -1, 1, 0}};
            d->SetFVF(0x112);
            DWORD foliage = 2;
            emitter->SetPrivateData(EmitterTag, &foliage, 4, 0);
            if (improve)
                fx.sceneFX().beginFoliage(s);
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(V));
            fx.sceneFX().endFoliage();
            DWORD restored = 0;
            d->GetRenderState(D3DRS_AMBIENT, &restored);
            ok = restored == 0xff080810;
        }
        d->EndScene();
        return ok;
    };
    CHECK(materialFrame(true, false, false));
    auto oldSky = Read(d);
    CHECK(materialFrame(true, true, false));
    auto newSky = Read(d);
    CHECK(Difference(oldSky, newSky) > 1000);
    CHECK(Save(newSky, L"build/scene-skyfx.bmp"));
    auto skyProgram = SceneFXFixture::skyProgram(fx.sceneFX());
    Settings skyOnly;
    skyOnly.sky = true;
    skyOnly.bloom = skyOnly.color = false;
    fx.idle(skyOnly);
    CHECK(fx.bytes() == 0 && skyProgram == SceneFXFixture::skyProgram(fx.sceneFX()));
    CHECK(materialFrame(false, false, false));
    auto darkLeaves = Read(d);
    CHECK(materialFrame(false, true, false));
    auto liftedLeaves = Read(d);
    CHECK(Difference(darkLeaves, liftedLeaves) > 1000);
    CHECK(Save(liftedLeaves, L"build/scene-night-foliage.bmp"));
    CHECK(materialFrame(false, false, false, true));
    auto blueLeaves = Read(d);
    CHECK(materialFrame(false, true, false, true));
    CHECK(Difference(blueLeaves, Read(d)) > 1000);
    CHECK(materialFrame(false, false, true));
    auto dayLeaves = Read(d);
    CHECK(materialFrame(false, true, true));
    CHECK(dayLeaves == Read(d));
    fx.invalidate();
    CHECK(fx.bytes() == 0);
    CHECK(SUCCEEDED(d->Reset(&pp)));
    settings.fog = true;
    CHECK(render());
    fx.invalidate();
    DestroyWindow(window);
    puts("PASS: depth SSAO, fog, DoF, sun occlusion, native point-light receiver shading, GPU exposure, HUD, "
         "Reset.");
}
