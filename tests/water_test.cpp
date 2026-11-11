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
int main() {
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
    V vertices[] = {{-1, 0, -1, 0, 1, 0, 0xffffffff, 0, 0},
                    {-1, 0, 1, 0, 1, 0, 0xffffffff, 0, 1},
                    {1, 0, -1, 0, 1, 0, 0xffffffff, 1, 0},
                    {1, 0, 1, 0, 1, 0, 0xffffffff, 1, 1}};
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
    puts("PASS: exact water texture selection, unrelated material skipped, WRITEONLY VB, shader state "
         "restored, transparency, reset/recreation.");
}
