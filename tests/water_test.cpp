#include "water.hpp"
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
DWORD Pixel(IDirect3DDevice9 *d, IDirect3DSurface9 *bb) {
    D3DSURFACE_DESC desc{};
    bb->GetDesc(&desc);
    Com<IDirect3DSurface9> resolved;
    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
        if (FAILED(d->CreateRenderTarget(320, 240, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, resolved.out(),
                                         nullptr)) ||
            FAILED(d->StretchRect(bb, nullptr, resolved, nullptr, D3DTEXF_NONE)))
            return 0;
        bb = resolved;
    }
    Com<IDirect3DSurface9> read;
    if (FAILED(d->CreateOffscreenPlainSurface(320, 240, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, read.out(),
                                              nullptr)))
        return 0;
    if (FAILED(d->GetRenderTargetData(bb, read)))
        return 0;
    D3DLOCKED_RECT l{};
    if (FAILED(read->LockRect(&l, nullptr, D3DLOCK_READONLY)))
        return 0;
    DWORD pixel = *(DWORD *)((BYTE *)l.pBits + 120 * l.Pitch + 160 * 4);
    read->UnlockRect();
    return pixel & 0xffffff;
}
int main(int argc, char **) {
    FILE *file = _wfopen(L"X:\\Games\\Homesickness\\World\\Texture\\water_HarAni0041.dds", L"rb");
    if (!file) {
        puts("SKIP: optional installed water DDS fixture is absent");
        return 0;
    }
    unsigned char header[128];
    CHECK(fread(header, 1, 128, file) == 128);
    std::vector<unsigned char> pixels(32768);
    CHECK(fread(pixels.data(), 1, pixels.size(), file) == pixels.size());
    fclose(file);
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FENBWaterTest";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"Water material test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240,
                                nullptr, nullptr, wc.hInstance, nullptr);
    Com<IDirect3D9> root;
    root.p = Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(root.p);
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.BackBufferWidth = 320;
    pp.BackBufferHeight = 240;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.hDeviceWindow = window;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D16;
    if (argc > 1) {
        if (FAILED(root->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, pp.BackBufferFormat, TRUE,
                                                    D3DMULTISAMPLE_8_SAMPLES, nullptr))) {
            puts("SKIP: 8x MSAA water fixture is not supported");
            return 0;
        }
        pp.MultiSampleType = D3DMULTISAMPLE_8_SAMPLES;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    }
    Com<IDirect3DDevice9> d;
    CHECK(SUCCEEDED(
        root->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, d.out())));
    Com<IDirect3DTexture9> texture, other;
    CHECK(SUCCEEDED(
        d->CreateTexture(128, 128, 1, 0, D3DFMT_A1R5G5B5, D3DPOOL_MANAGED, texture.out(), nullptr)));
    CHECK(
        SUCCEEDED(d->CreateTexture(128, 128, 1, 0, D3DFMT_A1R5G5B5, D3DPOOL_MANAGED, other.out(), nullptr)));
    D3DLOCKED_RECT lock{};
    CHECK(SUCCEEDED(texture->LockRect(0, &lock, nullptr, 0)));
    for (int y = 0; y < 128; ++y)
        memcpy((BYTE *)lock.pBits + y * lock.Pitch, pixels.data() + y * 256, 256);
    texture->UnlockRect(0);
    Com<IDirect3DTexture9> skyTexture;
    FILE *skyFile = _wfopen(L"X:\\Games\\Homesickness\\Weather\\skybox01.dds", L"rb");
    CHECK(skyFile);
    unsigned char skyHeader[128]{};
    CHECK(fread(skyHeader, 1, 128, skyFile) == 128);
    std::vector<unsigned char> skyPixels(2048 * 512 / 2);
    CHECK(fread(skyPixels.data(), 1, skyPixels.size(), skyFile) == skyPixels.size());
    fclose(skyFile);
    CHECK(SUCCEEDED(
        d->CreateTexture(2048, 512, 1, 0, D3DFMT_DXT1, D3DPOOL_MANAGED, skyTexture.out(), nullptr)));
    CHECK(SUCCEEDED(skyTexture->LockRect(0, &lock, nullptr, 0)));
    for (int y = 0; y < 128; ++y)
        memcpy((BYTE *)lock.pBits + y * lock.Pitch, skyPixels.data() + y * 4096, 4096);
    skyTexture->UnlockRect(0);
    // The client switches weather materials at night. Day-only recognition silently disabled SkyFX.
    {
        WaterFX catalog(d);
        const wchar_t *families[] = {L"skybox01", L"skybox02", L"skybox03", L"skybox04",
                                     L"skybox05", L"skybox06", L"cloud01",  L"cloud02",
                                     L"cloud03",  L"cloud04",  L"cloud05",  L"rartesia_cloud"};
        for (int i = 0; i < 12; ++i) {
            for (bool low : {false, true}) {
                std::wstring path = L"X:\\Games\\Homesickness\\Weather\\";
                path += families[i];
                if (low)
                    path += L"_low";
                path += L".dds";
                FILE *f = _wfopen(path.c_str(), L"rb");
                CHECK(f);
                DWORD header[32]{};
                CHECK(fread(header, 1, 128, f) == 128);
                UINT width = header[4], height = header[3];
                D3DFORMAT format = D3DFORMAT(header[21]);
                UINT rowBytes = ((width + 3) / 4) * (format == D3DFMT_DXT1 ? 8 : 16);
                std::vector<BYTE> data(rowBytes * ((height + 3) / 4));
                CHECK(fread(data.data(), 1, data.size(), f) == data.size());
                fclose(f);
                Com<IDirect3DTexture9> t;
                CHECK(SUCCEEDED(
                    d->CreateTexture(width, height, 1, 0, format, D3DPOOL_MANAGED, t.out(), nullptr)));
                CHECK(SUCCEEDED(t->LockRect(0, &lock, nullptr, 0)));
                for (UINT y = 0; y < (height + 3) / 4; ++y)
                    memcpy((BYTE *)lock.pBits + y * lock.Pitch, data.data() + y * rowBytes, rowBytes);
                t->UnlockRect(0);
                CHECK(catalog.materialTag(t) == (i < 6 ? 3u : i == 8 ? 5u : 4u));
            }
        }
    }
    CHECK(SUCCEEDED(other->LockRect(0, &lock, nullptr, 0)));
    for (int y = 0; y < 128; ++y)
        memset((BYTE *)lock.pBits + y * lock.Pitch, 0xff, 256);
    other->UnlockRect(0);
    struct V {
        float x, y, z, nx, ny, nz;
        DWORD color;
        float u, v;
    };
    static_assert(sizeof(V) == 36);
    V vertices[] = {{-1, 0, -1, 0, 1, 0, 0x00ffffff, 0, 0},
                    {-1, 0, 1, 0, 1, 0, 0x00ffffff, 0, 1},
                    {1, 0, -1, 0, 1, 0, 0x00ffffff, 1, 0},
                    {1, 0, 1, 0, 1, 0, 0x00ffffff, 1, 1}};
    Com<IDirect3DVertexBuffer9> vb;
    CHECK(SUCCEEDED(d->CreateVertexBuffer(sizeof(vertices), D3DUSAGE_WRITEONLY, 0x152, D3DPOOL_MANAGED,
                                          vb.out(), nullptr)));
    void *ptr = nullptr;
    CHECK(SUCCEEDED(vb->Lock(0, 0, &ptr, 0)));
    memcpy(ptr, vertices, sizeof(vertices));
    vb->Unlock();
    D3DMATRIX id{};
    id._11 = id._22 = id._33 = id._44 = 1;
    D3DMATRIX view{};
    view._11 = 1;
    view._23 = -1;
    view._32 = 1;
    view._43 = 3;
    view._44 = 1;
    D3DMATRIX projection = id;
    projection._33 = .1f; // orthographic camera looking down at horizontal water
    Settings settings;
    settings.waterWaves = 0;
    WaterFX water(d);
    for (int iteration = 0; iteration < 2; ++iteration) {
        Com<IDirect3DSurface9> bb;
        CHECK(SUCCEEDED(d->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, bb.out())));
        d->SetTransform(D3DTS_WORLD, &id);
        d->SetTransform(D3DTS_VIEW, &view);
        d->SetTransform(D3DTS_PROJECTION, &projection);
        d->SetFVF(0x152);
        d->SetStreamSource(0, vb, 0, 36);
        d->SetTexture(0, other);
        d->SetVertexShader(nullptr);
        d->SetPixelShader(nullptr);
        d->SetRenderState(D3DRS_ZENABLE, TRUE);
        d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        d->SetRenderState(D3DRS_FOGENABLE, FALSE);
        CHECK(!water.begin(
            settings)); // identical layout and dimensions must never select an unrelated material
        d->SetTexture(0, texture);
        DWORD tag = 0, size = 4;
        texture->FreePrivateData(WaterTag);
        DWORD previousPixel = 0;
        for (float opacity : {.2f, .9f}) {
            water.beginFrame();
            settings.waterOpacity = opacity;
            d->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff204060, 1, 0);
            CHECK(SUCCEEDED(d->BeginScene()));
            CHECK(water.begin(settings));
            CHECK(SUCCEEDED(texture->GetPrivateData(WaterTag, &tag, &size)) && tag == 1);
            CHECK(SUCCEEDED(d->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2)));
            water.end();
            CHECK(SUCCEEDED(d->EndScene()));
            Com<IDirect3DVertexShader9> vs;
            Com<IDirect3DPixelShader9> ps;
            d->GetVertexShader(vs.out());
            d->GetPixelShader(ps.out());
            CHECK(!vs.p && !ps.p);
            DWORD fvf = 0, fog = 1;
            d->GetFVF(&fvf);
            d->GetRenderState(D3DRS_FOGENABLE, &fog);
            CHECK(fvf == 0x152 && !fog);
            DWORD result = Pixel(d, bb);
            CHECK(result != 0x204060 && result != 0);
            CHECK(!previousPixel || result != previousPixel);
            previousPixel = result;
            printf("Water material: iteration=%d opacity=%.2f pixel=%06lx\n", iteration, opacity, result);
        }
        D3DLIGHT9 sun{};
        sun.Type = D3DLIGHT_DIRECTIONAL;
        sun.Direction.y = -1;
        sun.Diffuse = {1, 1, 1, 1};
        CHECK(SUCCEEDED(d->SetLight(0, &sun)));
        CHECK(SUCCEEDED(d->LightEnable(0, TRUE)));
        DWORD unlit = 0;
        for (float strength : {0.f, 1.f}) {
            water.beginFrame();
            settings.waterGlints = strength;
            d->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff204060, 1, 0);
            d->BeginScene();
            CHECK(water.begin(settings));
            CHECK(SUCCEEDED(d->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2)));
            water.end();
            d->EndScene();
            DWORD result = Pixel(d, bb);
            if (strength == 0)
                unlit = result;
            else
                CHECK((result & 255) > (unlit & 255) + 20);
        }
        d->LightEnable(0, FALSE);
        // Copy exactly once per frame, never recursively refract the previous water draw.
        settings.waterGlints = 0;
        settings.waterRefraction = .5f;
        settings.waterTexture = 0;
        settings.waterRipples = 0;
        settings.waterOpacity = .35f;
        water.beginFrame();
        auto renderWater = [&]() {
            d->BeginScene();
            bool ok = water.begin(settings);
            if (ok)
                d->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
            water.end();
            d->EndScene();
            return ok;
        };
        d->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xffc03020, 1, 0);
        CHECK(renderWater());
        constexpr size_t normalBytes = 87380;
        CHECK(water.bytes() == 320 * 240 * 4 + normalBytes);
        DWORD firstFrame = Pixel(d, bb);
        CHECK(renderWater() && Pixel(d, bb) == firstFrame);
        water.beginFrame();
        d->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff2040d0, 1, 0);
        CHECK(renderWater() && Pixel(d, bb) != firstFrame); // Fresh scene, not stale color.
        // Scene sampler state belongs to the game after the replacement returns.
        d->SetTexture(1, other);
        d->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        CHECK(renderWater());
        Com<IDirect3DBaseTexture9> restoredTexture;
        DWORD restoredFilter = 0;
        d->GetTexture(1, restoredTexture.out());
        d->GetSamplerState(1, D3DSAMP_MINFILTER, &restoredFilter);
        CHECK(restoredTexture.p == other.p && restoredFilter == D3DTEXF_POINT);
        water.beginFrame();
        settings.waterRipples = .8f;
        settings.waterSpeed = 0;
        for (LONG x = 0; x < 320; ++x) {
            RECT stripe{x, 0, x + 1, 240};
            CHECK(SUCCEEDED(d->ColorFill(bb, &stripe, (x & 1) ? 0xffffffff : 0xff101010)));
        }
        settings.waterRefraction = .05f;
        CHECK(renderWater());
        DWORD smallDistortion = Pixel(d, bb);
        settings.waterRefraction = 1;
        CHECK(renderWater() && Pixel(d, bb) != smallDistortion); // The normal moves the actual scene sample.
        settings.waterRipples = 0;
        settings.waterRefraction = 0;
        settings.waterDepth = 0;
        settings.waterSSR = 0;
        CHECK(renderWater() &&
              water.bytes() == normalBytes); // Frame target released; small normal map remains.
        Com<IDirect3DIndexBuffer9> ib;
        CHECK(SUCCEEDED(d->CreateIndexBuffer(12, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, ib.out(), nullptr)));
        WORD indices[] = {0, 1, 2, 2, 1, 3};
        for (bool indexed : {false, true}) {
            d->SetStreamSource(0, vb, 0, 36);
            d->SetIndices(ib);
            CHECK(SUCCEEDED(d->BeginScene()));
            CHECK(water.begin(settings, sizeof(V)));
            HRESULT hr = indexed ? d->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, indices,
                                                             D3DFMT_INDEX16, vertices, sizeof(V))
                                 : d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(V));
            water.end(true, indexed);
            CHECK(SUCCEEDED(hr));
            CHECK(SUCCEEDED(d->EndScene()));
            Com<IDirect3DVertexBuffer9> stream;
            UINT offset = 0, stride = 0;
            CHECK(SUCCEEDED(d->GetStreamSource(0, stream.out(), &offset, &stride)));
            CHECK(!stream.p);
            Com<IDirect3DIndexBuffer9> bound;
            CHECK(SUCCEEDED(d->GetIndices(bound.out())));
            CHECK(indexed ? !bound.p : bound.p == ib.p);
        }
        {
            struct SkyVertex {
                float x, y, z;
                DWORD color;
                float u, v;
            };
            SkyVertex skyVertices[] = {{-10, 10, -10, 0xffffffff, 0, 0}, {10, 10, -10, 0xffffffff, 1, 0},
                                       {-10, 10, 10, 0xffffffff, 0, 1},  {-10, 10, 10, 0xffffffff, 0, 1},
                                       {10, 10, -10, 0xffffffff, 1, 0},  {10, 10, 10, 0xffffffff, 1, 1}};
            Com<IDirect3DVertexBuffer9> skyVB;
            CHECK(SUCCEEDED(d->CreateVertexBuffer(sizeof(skyVertices), D3DUSAGE_WRITEONLY, 0x142,
                                                  D3DPOOL_MANAGED, skyVB.out(), nullptr)));
            void *skyData = nullptr;
            CHECK(SUCCEEDED(skyVB->Lock(0, 0, &skyData, 0)));
            memcpy(skyData, skyVertices, sizeof(skyVertices));
            skyVB->Unlock();
            D3DMATRIX skyProjection{};
            skyProjection._11 = skyProjection._22 = 1;
            skyProjection._33 = 100.f / 99.9f;
            skyProjection._34 = 1;
            skyProjection._43 = -.1f * 100 / 99.9f;
            d->SetFVF(0x142);
            d->SetStreamSource(0, skyVB, 0, sizeof(SkyVertex));
            d->SetTexture(0, skyTexture);
            d->SetTransform(D3DTS_VIEW, &id);
            d->SetTransform(D3DTS_PROJECTION, &skyProjection);
            d->SetRenderState(D3DRS_ZENABLE, FALSE);
            d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            DWORD beforeCapture = Pixel(d, bb);
            water.beginFrame();
            d->BeginScene();
            water.captureEnvironment(D3DPT_TRIANGLELIST, 0, 2);
            // Neuz blends multiple skybox materials at dusk. Both layers must survive the
            // same capture, even though the first draw has just advanced the refresh timer.
            d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
            d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
            d->SetRenderState(D3DRS_TEXTUREFACTOR, 0xffff0000);
            d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            water.captureEnvironment(D3DPT_TRIANGLELIST, 0, 2);
            d->SetRenderState(D3DRS_TEXTUREFACTOR, 0x800000ff);
            d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            water.captureEnvironment(D3DPT_TRIANGLELIST, 0, 2);
            d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            d->EndScene();
            CHECK(Pixel(d, bb) == beforeCapture); // Repeated sky draws never touch the real scene.
            CHECK(water.bytes() == 128 * 128 * 4 * 6 + normalBytes);
            Com<IDirect3DSurface9> restoredTarget, restoredDepth;
            d->GetRenderTarget(0, restoredTarget.out());
            d->GetDepthStencilSurface(restoredDepth.out());
            CHECK(restoredTarget.p == bb.p && restoredDepth.p);
            D3DVIEWPORT9 restoredViewport{};
            d->GetViewport(&restoredViewport);
            CHECK(restoredViewport.Width == 320 && restoredViewport.Height == 240);
            d->SetFVF(0x152);
            d->SetStreamSource(0, vb, 0, 36);
            d->SetTexture(0, texture);
            d->SetTransform(D3DTS_VIEW, &view);
            d->SetTransform(D3DTS_PROJECTION, &projection);
            d->SetRenderState(D3DRS_ZENABLE, TRUE);
            d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            d->BeginScene();
            CHECK(water.begin(settings));
            float details[4]{};
            d->GetPixelShaderConstantF(9, details, 1);
            CHECK(details[3] == 1); // The actual sky capture, not the analytic fallback, is selected.
            Com<IDirect3DBaseTexture9> capturedSky;
            d->GetTexture(2, capturedSky.out());
            CHECK(capturedSky.p && capturedSky->GetType() == D3DRTYPE_CUBETEXTURE);
            d->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
            water.end();
            d->EndScene();
            Com<IDirect3DSurface9> skyFace, skyRead;
            auto *cube = static_cast<IDirect3DCubeTexture9 *>(capturedSky.p);
            CHECK(SUCCEEDED(cube->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_Y, 0, skyFace.out())));
            CHECK(SUCCEEDED(d->CreateOffscreenPlainSurface(128, 128, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                                           skyRead.out(), nullptr)));
            CHECK(SUCCEEDED(d->GetRenderTargetData(skyFace, skyRead)));
            D3DLOCKED_RECT skyLock{};
            CHECK(SUCCEEDED(skyRead->LockRect(&skyLock, nullptr, D3DLOCK_READONLY)));
            DWORD cubePixel = *(DWORD *)((BYTE *)skyLock.pBits + 64 * skyLock.Pitch + 64 * 4);
            skyRead->UnlockRect();
            CHECK((cubePixel & 0xffffff) != 0);
            CHECK(abs(int((cubePixel >> 16) & 255) - 127) <= 1);
            CHECK(abs(int(cubePixel & 255) - 128) <= 1);
            CHECK((cubePixel & 0xff00) == 0);
            water.beginFrame(); // A different/unknown sky next frame must not display stale reflections.
            d->BeginScene();
            CHECK(water.begin(settings));
            d->GetPixelShaderConstantF(9, details, 1);
            CHECK(details[3] == 0);
            water.end();
            d->EndScene();
            water.beginFrame(false, false, false);
            CHECK(water.bytes() == normalBytes);
        }
        // Surface uploads must invalidate the owning texture's classification too.
        d->SetStreamSource(0, vb, 0, 36);
        Com<IDirect3DSurface9> level;
        CHECK(SUCCEEDED(texture->GetSurfaceLevel(0, level.out())));
        CHECK(SUCCEEDED(level->LockRect(&lock, nullptr, 0)));
        ((BYTE *)lock.pBits)[0] ^= 1;
        level->UnlockRect();
        InvalidateWaterSurface(level);
        CHECK(!water.begin(settings));
        CHECK(SUCCEEDED(texture->LockRect(0, &lock, nullptr, 0)));
        ((BYTE *)lock.pBits)[0] = pixels[0];
        texture->UnlockRect(0);
        InvalidateWaterTag(texture);
        CHECK(water.begin(settings));
        water.end();
        Drop(bb.p);
        water.invalidate();
        CHECK(SUCCEEDED(d->Reset(&pp)));
    }
    DestroyWindow(window);
    printf("Water scene-copy fixture: MSAA=%d\n", int(pp.MultiSampleType));
    puts("PASS: exact water texture selection, unrelated material skipped, WRITEONLY VB, shader state "
         "restored, transparency, single scene capture, fresh frames, zero-cost refraction off, "
         "reset/recreation.");
}
