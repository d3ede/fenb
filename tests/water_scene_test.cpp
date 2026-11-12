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
struct WaterFXFixture {
    static bool seed(WaterFX &water, IDirect3DDevice9 *d) {
        return water.reflectionSurface_.p &&
               SUCCEEDED(d->ColorFill(water.reflectionSurface_, nullptr, 0xff00ffff));
    }
    static std::vector<DWORD> reflection(WaterFX &water, IDirect3DDevice9 *d) {
        if (!water.reflectionSurface_.p) return {};
        D3DSURFACE_DESC desc{}; water.reflectionSurface_->GetDesc(&desc);
        Com<IDirect3DSurface9> read;
        if (FAILED(d->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
                                                D3DPOOL_SYSTEMMEM, read.out(), nullptr)) ||
            FAILED(d->GetRenderTargetData(water.reflectionSurface_, read))) return {};
        D3DLOCKED_RECT data{};
        if (FAILED(read->LockRect(&data, nullptr, D3DLOCK_READONLY))) return {};
        std::vector<DWORD> pixels(desc.Width * desc.Height);
        for (UINT y = 0; y < desc.Height; ++y)
            memcpy(pixels.data() + y * desc.Width, (BYTE *)data.pBits + y * data.Pitch, desc.Width * 4);
        read->UnlockRect(); return pixels;
    }
};
namespace {
constexpr UINT Width = 640, Height = 400;
std::vector<DWORD> ReadFrame(IDirect3DDevice9 *d) {
    Com<IDirect3DSurface9> bb, resolved, read;
    d->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, bb.out());
    D3DSURFACE_DESC desc{};
    bb->GetDesc(&desc);
    IDirect3DSurface9 *source = bb;
    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
        if (FAILED(d->CreateRenderTarget(Width, Height, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE,
                                         resolved.out(), nullptr)) ||
            FAILED(d->StretchRect(bb, nullptr, resolved, nullptr, D3DTEXF_NONE)))
            return {};
        source = resolved;
    }
    if (FAILED(d->CreateOffscreenPlainSurface(Width, Height, desc.Format, D3DPOOL_SYSTEMMEM, read.out(),
                                              nullptr)) ||
        FAILED(d->GetRenderTargetData(source, read)))
        return {};
    D3DLOCKED_RECT lock{};
    if (FAILED(read->LockRect(&lock, nullptr, D3DLOCK_READONLY)))
        return {};
    std::vector<DWORD> result(Width * Height);
    for (UINT y = 0; y < Height; ++y)
        memcpy(result.data() + y * Width, (BYTE *)lock.pBits + y * lock.Pitch, Width * 4);
    read->UnlockRect();
    return result;
}
bool Save(const std::vector<DWORD> &pixels, const wchar_t *path) {
    FILE *f = _wfopen(path, L"wb");
    if (!f)
        return false;
    BITMAPFILEHEADER h{};
    h.bfType = 0x4d42;
    h.bfOffBits = sizeof(h) + sizeof(BITMAPINFOHEADER);
    h.bfSize = h.bfOffBits + Width * Height * 4;
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info);
    info.biWidth = Width;
    info.biHeight = -LONG(Height);
    info.biPlanes = 1;
    info.biBitCount = 32;
    bool ok = fwrite(&h, 1, sizeof(h), f) == sizeof(h) && fwrite(&info, 1, sizeof(info), f) == sizeof(info) &&
              fwrite(pixels.data(), 4, pixels.size(), f) == pixels.size();
    fclose(f);
    return ok;
}
int Difference(DWORD a, DWORD b) {
    return std::abs(int((a >> 16) & 255) - int((b >> 16) & 255)) +
           std::abs(int((a >> 8) & 255) - int((b >> 8) & 255)) + std::abs(int(a & 255) - int(b & 255));
}
} // namespace
int main(int argc, char **argv) {
    bool msaa = false, depth24 = false;
    for (int i = 1; i < argc; ++i) {
        msaa |= !strcmp(argv[i], "--msaa");
        depth24 |= !strcmp(argv[i], "--depth24");
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FENBWaterSceneTest";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"FENB water scene fixture", WS_OVERLAPPEDWINDOW, 0, 0,
                                Width, Height, nullptr, nullptr, wc.hInstance, nullptr);
    Com<IDirect3D9> root;
    root.p = Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(root.p);
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.hDeviceWindow = window;
    pp.BackBufferWidth = Width;
    pp.BackBufferHeight = Height;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = depth24 ? D3DFMT_D24X8 : D3DFMT_D16;
    if (msaa) {
        if (FAILED(root->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, pp.BackBufferFormat, TRUE,
                                                    D3DMULTISAMPLE_8_SAMPLES, nullptr))) {
            puts("SKIP: 8x MSAA unavailable");
            return 0;
        }
        pp.MultiSampleType = D3DMULTISAMPLE_8_SAMPLES;
    }
    Com<IDirect3DDevice9> d;
    CHECK(SUCCEEDED(
        root->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, d.out())));
    {
        DepthCopy probe(d);
        d->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, .5f, 0);
        if (!probe.capture(Width, Height)) {
            puts("SKIP: optional NVIDIA depth copy unavailable; fallback material is tested separately");
            return 0;
        }
        CHECK(probe.quantum() == (depth24 ? 1.f / 16777215.f : 1.f / 65535.f));
    }
    Com<IDirect3DTexture9> texture;
    CHECK(SUCCEEDED(d->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, texture.out(), nullptr)));
    D3DLOCKED_RECT lock{};
    CHECK(SUCCEEDED(texture->LockRect(0, &lock, nullptr, 0)));
    *(DWORD *)lock.pBits = 0xffffffff;
    texture->UnlockRect(0);
    DWORD tag = 1;
    texture->SetPrivateData(WaterTag, &tag, sizeof(tag),
                            0); // Synthetic fixture; exact resource recognition is tested in water_test.
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
    projection._33 = 100.f / 99.9f;
    projection._34 = 1;
    projection._43 = -.1f * 100 / 99.9f;
    struct Floor {
        float x, y, z;
        DWORD color;
    };
    struct Vertex {
        float x, y, z, nx, ny, nz;
        DWORD color;
        float u, v;
    };
    Vertex vertices[] = {{-8, 0, -5, 0, 1, 0, 0x00ffffff, 0, 0},
                         {-8, 0, 60, 0, 1, 0, 0x00ffffff, 0, 1},
                         {8, 0, -5, 0, 1, 0, 0x00ffffff, 1, 0},
                         {8, 0, 60, 0, 1, 0, 0x00ffffff, 1, 1}};
    WaterFX water(d);
    Settings settings;
    settings.waterWaves = 0;
    settings.waterRipples = 0;
    settings.waterSpeed = 0;
    settings.waterTexture = 0;
    settings.waterGlints = 0;
    settings.waterSky = .75f;
    settings.waterOpacity = .49f;
    settings.waterRefraction = .4f;
    float sceneLight = -1;
    float wallSlope = 0;
    bool foldedWall = false;
    bool steppedMesh = false;
    bool foregroundBank = false;
    bool translatedScene = false;
    auto render = [&](float floorY, bool wall, bool replace) {
        water.beginFrame(true, true, true, settings.waterCaustics > 0);
        d->SetRenderState(D3DRS_ZENABLE, TRUE);
        d->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        d->SetRenderState(D3DRS_LIGHTING, FALSE);
        d->SetRenderState(D3DRS_FOGENABLE, FALSE);
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        d->SetVertexShader(nullptr);
        d->SetPixelShader(nullptr);
        D3DMATRIX sceneWorld = id, sceneView = view;
        if (translatedScene) {
            sceneWorld._41 = 7000;
            sceneWorld._42 = 100;
            sceneWorld._43 = 3300;
            for (int axis = 0; axis < 3; ++axis)
                sceneView.m[3][axis] -=
                    7000 * view.m[0][axis] + 100 * view.m[1][axis] + 3300 * view.m[2][axis];
        }
        d->SetTransform(D3DTS_WORLD, &sceneWorld);
        d->SetTransform(D3DTS_VIEW, &sceneView);
        d->SetTransform(D3DTS_PROJECTION, &projection);
        d->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        d->SetTexture(0, nullptr);
        d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        d->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        d->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff9cb8cc, 1, 0);
        d->BeginScene();
        Floor floor[] = {{-12, floorY, -5, 0xff827050},
                         {-12, floorY, 60, 0xff827050},
                         {12, floorY, -5, 0xff827050},
                         {12, floorY, 60, 0xff827050}};
        d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, floor, sizeof(Floor));
        Floor object[] = {{-2, 0, 18, 0xffd05028}, {2, 0, 18, 0xffd05028}, {-2, 8, 18+8*wallSlope, 0xffd05028},
                          {-2, 8, 18+8*wallSlope, 0xffd05028}, {2, 0, 18, 0xffd05028}, {2, 8, 18+8*wallSlope, 0xffd05028}};
        if (wall && foldedWall) {
            const float crease=.013f;
            Floor fold[]={{-2,0,26,0xffd05028},{crease,0,18,0xffd05028},{-2,8,26,0xffd05028},{crease,8,18,0xffd05028},
                          {crease,0,18,0xffd05028},{2,0,26,0xffd05028},{crease,8,18,0xffd05028},{2,8,26,0xffd05028}};
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,fold,sizeof(Floor));
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,fold+4,sizeof(Floor));
        } else if (wall)
            d->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, object, sizeof(Floor));
        if (foregroundBank) {
            Floor bank[] = {
                {1, 0, 5, 0xff406020}, {5, 0, 8, 0xff406020}, {1, 9, 5, 0xff406020}, {5, 9, 8, 0xff406020}};
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, bank, sizeof(Floor));
        }
        bool ok = true;
        if (replace) {
            if (sceneLight >= 0) {
                d->SetRenderState(D3DRS_LIGHTING, TRUE);
                D3DMATERIAL9 material{};
                material.Ambient = material.Diffuse = {1, 1, 1, 1};
                d->SetMaterial(&material);
                D3DLIGHT9 light{};
                light.Type = D3DLIGHT_DIRECTIONAL;
                light.Direction.y = -1;
                light.Diffuse = {.7f * sceneLight, .7f * sceneLight, .7f * sceneLight, 1};
                d->SetLight(0, &light);
                d->LightEnable(0, TRUE);
                BYTE ambient = BYTE(76 * sceneLight);
                d->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(ambient, ambient, ambient));
            }
            d->SetFVF(0x152);
            d->SetTexture(0, texture);
            d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            ok = water.begin(settings, sizeof(Vertex));
            float optics[4]{};
            d->GetPixelShaderConstantF(14, optics, 1);
            ok = ok &&
                 optics[0] ==
                     ((settings.waterDepth > 0 || settings.waterSSR > 0 || settings.waterCaustics > 0) ? 1.f
                                                                                                       : 0.f);
            if (ok && !steppedMesh) {
                water.reflect(
                    [&]() { return d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex)); });
                ok = SUCCEEDED(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex)));
            } else if (ok) {
                Vertex left[4], right[4];
                memcpy(left, vertices, sizeof(vertices));
                memcpy(right, vertices, sizeof(vertices));
                left[1].z = left[3].z = 10;
                right[0].z = right[2].z = 10;
                for (auto &vertex : right)
                    vertex.y = -.2f;
                Vertex skirt[] = {{-8, 0, 10, 0, 0, -1, 0x00ffffff, 0, 0},
                                  {-8, -.2f, 10, 0, 0, -1, 0x00ffffff, 0, 1},
                                  {8, 0, 10, 0, 0, -1, 0x00ffffff, 1, 0},
                                  {8, -.2f, 10, 0, 0, -1, 0x00ffffff, 1, 1}};
                ok = SUCCEEDED(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, left, sizeof(Vertex))) &&
                     SUCCEEDED(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, right, sizeof(Vertex))) &&
                     SUCCEEDED(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, skirt, sizeof(Vertex)));
            }
            water.end(true);
        }
        d->EndScene();
        return ok;
    };
    for (int reset = 0; reset < 2; ++reset) {
        translatedScene = reset != 0;
        settings.waterSSRQuality = reset;
        settings.waterSSR = 0;
        CHECK(render(-2, true, true));
        auto off = ReadFrame(d);
        CHECK(off.size() == Width * Height);
        settings.waterSSR = 1;
        CHECK(render(-2, true, true));
        auto on = ReadFrame(d);
        CHECK(on.size() == off.size());
        unsigned reflected = 0;
        for (UINT y = Height * 4 / 10; y < Height * 8 / 10; ++y)
            for (UINT x = Width * 4 / 10; x < Width * 6 / 10; ++x) {
                auto index = y * Width + x;
                if (int((on[index] >> 16) & 255) - int((off[index] >> 16) & 255) > 8)
                    ++reflected;
            }
        printf("SSR reflected object pixels: %u, MSAA=%u\n", reflected, unsigned(pp.MultiSampleType));
        CHECK(reflected > 100);
        settings.waterRipples = .8f;
        CHECK(render(-2, true, true));
        auto rippled = ReadFrame(d);
        unsigned changedNormals = 0;
        for (UINT y = Height * 4 / 10; y < Height * 8 / 10; ++y)
            for (UINT x = Width * 3 / 10; x < Width * 7 / 10; ++x)
                if (Difference(rippled[y * Width + x], on[y * Width + x]) > 3)
                    ++changedNormals;
        CHECK(changedNormals > 100);
        settings.waterRipples = 0;
        CHECK(on[80 * Width + 320] == off[80 * Width + 320]); // Real object stays unchanged above water.
        if (reset == 0) {
            CHECK(Save(off, L"build/water-scene-ssr-off.bmp"));
            CHECK(Save(on, L"build/water-scene-ssr-on.bmp"));
        }
        if (!reset) {
            CHECK(WaterFXFixture::seed(water, d));
            CHECK(render(-2, false, true));
            auto fresh = WaterFXFixture::reflection(water, d);
            CHECK(fresh.size() == Width * Height && (fresh[0] >> 24) == 0);
            auto fullProjection = projection;
            projection._33 = 60.f / 59.9f;
            projection._43 = -.1f * projection._33;
            CHECK(render(-2, false, true));
            unsigned skyHits = 0;
            for (DWORD pixel : WaterFXFixture::reflection(water, d))
                skyHits += (pixel >> 24) != 0;
            printf("SSR false sky-plane hits: %u\n", skyHits);
            CHECK(skyHits == 0);
            projection = fullProjection;
            auto savedView = view;
            settings.waterRefraction = 0;
            // Red reflection pixels must lie inside the analytic reflection of the actual red wall.
            // Repeat with camera yaw and an intersecting foreground bank, rather than only a static view.
            for (float slope : {0.f, 2.f}) {
              wallSlope = slope;
              for (float yaw : {-.4f, 0.f, .4f}) {
                float ex = std::sin(yaw) * 8, ey = 4, ez = -std::cos(yaw) * 8;
                float zx = -ex, zy = -ey, zz = 12 - ez;
                float len = std::sqrt(zx * zx + zy * zy + zz * zz);
                zx /= len;
                zy /= len;
                zz /= len;
                float xx = zz, xz = -zx;
                len = std::sqrt(xx * xx + xz * xz);
                xx /= len;
                xz /= len;
                float yx = zy * xz, yy = zz * xx - zx * xz, yz = -zy * xx;
                view = {xx,
                        yx,
                        zx,
                        0,
                        0,
                        yy,
                        zy,
                        0,
                        xz,
                        yz,
                        zz,
                        0,
                        -ex * xx - ez * xz,
                        -ex * yx - ey * yy - ez * yz,
                        -ex * zx - ey * zy - ez * zz,
                        1};
                foregroundBank = slope == 0;
                settings.waterSSR = 0;
                CHECK(render(-2, true, true));
                auto noMirror = ReadFrame(d);
                settings.waterSSR = 1;
                CHECK(render(-2, true, true));
                auto mirror = ReadFrame(d);
                unsigned hits = 0, invalid = 0;
                for (UINT y = 0; y < Height; ++y)
                    for (UINT x = 0; x < Width; ++x) {
                        auto i = y * Width + x;
                        int dr = int((mirror[i] >> 16) & 255) - int((noMirror[i] >> 16) & 255);
                        int dg = int((mirror[i] >> 8) & 255) - int((noMirror[i] >> 8) & 255);
                        int db = int(mirror[i] & 255) - int(noMirror[i] & 255);
                        if (dr <= 6 || dr - dg <= 8 || dr - db <= 8)
                            continue;
                        ++hits;
                        float vx = (float(x) / Width * 2 - 1) / projection._11;
                        float vy = (1 - float(y) / Height * 2) / projection._22;
                        float rx = vx * xx + vy * yx + zx, ry = vy * yy + zy, rz = vx * xz + vy * yz + zz;
                        float t = -ey / ry;
                        float wx = ex + rx * t, wz = ez + rz * t;
                        float reflectedT = (18 - wz) / (rz + slope * ry);
                        float hx = wx + rx * reflectedT, hy = -ry * reflectedT;
                        invalid +=
                            !(t > 0 && reflectedT > 0 && hx > -2.2f && hx < 2.2f && hy > -.2f && hy < 8.2f);
                    }
                printf("SSR slope=%.1f camera yaw=%.2f: hits=%u outside analytic mirror=%u\n", slope, yaw, hits, invalid);
                CHECK(Save(mirror, yaw < 0   ? L"build/water-orbit-left.bmp"
                                   : yaw > 0 ? L"build/water-orbit-right.bmp"
                                             : L"build/water-orbit-center.bmp"));
                CHECK(hits > 100 && invalid * 100 < hits * 2);
              }
            }
            wallSlope = 0;
            foregroundBank = false;
            view = savedView;
            settings.waterRefraction = .4f;
            foldedWall=true;
            settings.waterRefraction=0;
            CHECK(render(-2,true,true));
            auto folded=WaterFXFixture::reflection(water,d);
            unsigned creaseNotches=0,creaseRows=0;
            for(UINT y=Height/3;y<Height*3/4;++y) {
                const UINT center=Width/2;
                if((folded[y*Width+center-5]>>24)>40 && (folded[y*Width+center+5]>>24)>40) {
                    ++creaseRows;
                    for(UINT x=center-2;x<=center+2;++x)
                        creaseNotches+=(folded[y*Width+x]>>24)+24<std::min(folded[y*Width+x-3]>>24,folded[y*Width+x+3]>>24);
                }
            }
            printf("Continuous folded bank: rows=%u reflection notches=%u\n",creaseRows,creaseNotches);
            CHECK(creaseRows>50 && creaseNotches<10);
            CHECK(Save(folded,L"build/water-folded-reflection.bmp"));
            foldedWall=false;settings.waterRefraction=.4f;
        }
        settings.waterSSR = 0;
        CHECK(render(-.02f, false, false));
        auto bare = ReadFrame(d);
        CHECK(render(-.02f, false, true));
        auto shallow = ReadFrame(d);
        CHECK(render(-3, false, true));
        auto deep = ReadFrame(d);
        auto sample = 270 * Width + 320;
        int nearDifference = Difference(bare[sample], shallow[sample]);
        int deepDifference = Difference(bare[sample], deep[sample]);
        printf("Depth absorption: shallow=%d deep=%d\n", nearDifference, deepDifference);
        CHECK(nearDifference < 20 && deepDifference > nearDifference + 20);
        // Shallow and coplanar flats keep a visible layer on the original water mesh.
        // Original geometry/depth testing keeps it off the dry background outside that mesh.
        auto originalSettings = settings;
        settings.waterOpacity = .74f;
        settings.waterDepth = 2;
        settings.waterShore = 2.35f;
        settings.waterRipples = .8f;
        settings.waterSky = .74f;
        CHECK(render(-.08f, false, false));
        auto shallowBare = ReadFrame(d);
        CHECK(render(-.08f, false, true));
        auto shallowLayer = ReadFrame(d);
        int layerDifference = Difference(shallowBare[sample], shallowLayer[sample]);
        CHECK(layerDifference > 10);
        CHECK(render(0, false, false));
        auto edgeBare = ReadFrame(d);
        CHECK(render(0, false, true));
        auto contact = ReadFrame(d);
        int contactDifference = Difference(edgeBare[sample], contact[sample]);
        printf("Shallow layer: %d, zero-depth contact: %d\n", layerDifference, contactDifference);
        CHECK(contactDifference > 10);
        CHECK(contact[20 * Width + 20] == edgeBare[20 * Width + 20]);
        if (reset == 0) {
            CHECK(Save(shallowBare, L"build/water-shallow-bare.bmp"));
            CHECK(Save(shallowLayer, L"build/water-shallow-layer.bmp"));
        }
        settings = originalSettings;
        // Wave amplitude must not relocate the reflected object across the water.
        settings.waterSky = 0;
        settings.waterRefraction = 0;
        auto reflectionMask = [&](float amplitude) {
            settings.waterWaves = amplitude;
            settings.waterSSR = 0;
            render(-2, true, true);
            auto background = ReadFrame(d);
            settings.waterSSR = 1;
            render(-2, true, true);
            auto reflectedFrame = ReadFrame(d);
            std::vector<bool> mask(Width * Height);
            for (UINT y = Height * 4 / 10; y < Height * 8 / 10; ++y)
                for (UINT x = Width * 3 / 10; x < Width * 7 / 10; ++x) {
                    UINT i = y * Width + x;
                    mask[i] = int((reflectedFrame[i] >> 16) & 255) > int((background[i] >> 16) & 255);
                }
            return mask;
        };
        auto flatMask = reflectionMask(0), waveMask = reflectionMask(.15f);
        unsigned shared = 0, total = 0;
        for (size_t i = 0; i < flatMask.size(); ++i) {
            shared += flatMask[i] && waveMask[i];
            total += flatMask[i] || waveMask[i];
        }
        CHECK(total > 100 && shared * 100 > total * 90);
        printf("Reflection footprint overlap at maximum waves: %.2f%%\n", 100.f * shared / total);
        settings.waterDepth = 0;
        settings.waterSSR = 0;
        settings.waterRefraction = 0;
        settings.waterOpacity = 1;
        settings.waterWaves = 0;
        steppedMesh = true;
        CHECK(render(-3, false, true));
        auto stitchedFlat = ReadFrame(d);
        settings.waterWaves = .15f;
        CHECK(render(-3, false, true));
        auto stitchedWaves = ReadFrame(d);
        CHECK(stitchedFlat == stitchedWaves); // Adjacent tiles and their vertical skirt remain welded.
        steppedMesh = false;
        settings.waterWaves = 0;
        settings.waterSky = .75f;
        sceneLight = 1;
        CHECK(render(-3, false, true));
        auto day = ReadFrame(d);
        sceneLight = .07f;
        CHECK(render(-3, false, true));
        auto night = ReadFrame(d);
        auto luminance = [](DWORD pixel) {
            return int((pixel >> 16) & 255) + int((pixel >> 8) & 255) + int(pixel & 255);
        };
        CHECK(luminance(night[sample]) < luminance(day[sample]) / 3);
        printf("Water lighting: day=%d night=%d\n", luminance(day[sample]), luminance(night[sample]));
        sceneLight = -1;
        // Caustics light only the transmitted underwater scene, not the real object above it.
        settings.waterSky = 0;
        settings.waterWaves = .03f;
        settings.waterRipples = .7f;
        settings.waterDepth = .8f;
        sceneLight = 1;
        settings.waterCaustics = 0;
        CHECK(render(-2, true, true));
        auto unlitFloor = ReadFrame(d);
        settings.waterCaustics = 1;
        CHECK(render(-2, true, true));
        auto focusedFloor = ReadFrame(d);
        unsigned focused = 0;
        for (UINT y = Height / 2; y < Height; ++y)
            for (UINT x = Width / 4; x < Width * 3 / 4; ++x)
                focused += Difference(unlitFloor[y * Width + x], focusedFloor[y * Width + x]) > 3;
        printf("Caustic receiver pixels: %u depth24=%d\n", focused, depth24);
        CHECK(focused > 100);
        CHECK(unlitFloor[80 * Width + 320] == focusedFloor[80 * Width + 320]);
        const auto quarterBytes = water.bytes();
        settings.waterCausticScale = 2;
        CHECK(render(-2, true, true));
        CHECK(water.bytes() == quarterBytes + Width * Height * 3 / 4);
        settings.waterCausticScale = 4;
        sceneLight = .07f;
        settings.waterCaustics = 0;
        CHECK(render(-2, true, true));
        auto nightOff = ReadFrame(d);
        CHECK(water.bytes() == quarterBytes - Width * Height / 4);
        settings.waterCaustics = 1;
        CHECK(render(-2, true, true));
        auto nightOn = ReadFrame(d);
        unsigned dayDifference = 0, nightDifference = 0;
        for (UINT y = Height / 2; y < Height; ++y)
            for (UINT x = Width / 4; x < Width * 3 / 4; ++x) {
                auto i = y * Width + x;
                dayDifference += Difference(unlitFloor[i], focusedFloor[i]);
                nightDifference += Difference(nightOff[i], nightOn[i]);
            }
        CHECK(dayDifference > nightDifference * 4);
        sceneLight = 1;
        if (!reset) {
            CHECK(Save(unlitFloor, L"build/water-caustics-off.bmp"));
            CHECK(Save(focusedFloor, L"build/water-caustics-on.bmp"));
        }
        settings.waterWaves = settings.waterRipples = 0;
        CHECK(render(-2, false, true));
        auto flatFocused = ReadFrame(d);
        settings.waterCaustics = 0;
        CHECK(render(-2, false, true));
        CHECK(flatFocused == ReadFrame(d));
        settings.waterSky = .75f;
        sceneLight = -1;
        settings.waterDepth = 1;
        settings.waterRefraction = .4f;
        settings.waterOpacity = .49f;
        water.beginFrame(false, false, false, false);
        CHECK(water.bytes() == 87380); // Only the small managed normal mip chain remains.
        water.invalidate();
        CHECK(water.bytes() == 0);
        CHECK(SUCCEEDED(d->Reset(&pp)));
    }
    DestroyWindow(window);
    puts("PASS: real MSAA depth, SSR object reflection, depth absorption, shore continuity, Reset "
         "recreation.");
}
