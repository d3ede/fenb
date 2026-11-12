#include "common.hpp"
#include <generated/scene_shaders.hpp>
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
constexpr UINT W = 1024, H = 640;
std::vector<DWORD> Read(IDirect3DDevice9 *d) {
    Com<IDirect3DSurface9> rt, resolved, read;
    d->GetRenderTarget(0, rt.out());
    D3DSURFACE_DESC desc{};
    rt->GetDesc(&desc);
    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
        if (FAILED(d->CreateRenderTarget(W, H, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, resolved.out(),
                                         nullptr)) ||
            FAILED(d->StretchRect(rt, nullptr, resolved, nullptr, D3DTEXF_NONE)))
            return {};
    }
    if (FAILED(d->CreateOffscreenPlainSurface(W, H, desc.Format, D3DPOOL_SYSTEMMEM, read.out(), nullptr)) ||
        FAILED(d->GetRenderTargetData(resolved.p ? resolved.p : rt.p, read)))
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
unsigned Difference(const std::vector<DWORD> &a, const std::vector<DWORD> &b) {
    if (a.size() != W * H || b.size() != a.size())
        return ~0u;
    unsigned count = 0;
    for (size_t i = 0; i < a.size(); ++i)
        for (int shift : {0, 8, 16})
            if (abs(int((a[i] >> shift) & 255) - int((b[i] >> shift) & 255)) > 3) {
                ++count;
                break;
            }
    return count;
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
    fwrite(&file, sizeof(file), 1, f);
    fwrite(&info, sizeof(info), 1, f);
    fwrite(pixels.data(), 4, pixels.size(), f);
    fclose(f);
}
} // namespace
int main(int argc, char **) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FENBSkyDetailTest";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"Sky material detail fixture", WS_OVERLAPPEDWINDOW, 0, 0,
                                W, H, nullptr, nullptr, wc.hInstance, nullptr);
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
    Com<IDirect3DTexture9> art;
    UINT width = 64, height = 32;
    if (FILE *file = _wfopen(L"X:\\Games\\Homesickness\\Weather\\skybox03.dds", L"rb")) {
        DWORD header[32]{};
        CHECK(fread(header, 1, 128, file) == 128);
        width = header[4];
        height = header[3];
        CHECK(header[21] == D3DFMT_DXT1);
        const UINT rowBytes = (width + 3) / 4 * 8, rows = (height + 3) / 4;
        std::vector<BYTE> pixels(rowBytes * rows);
        CHECK(fread(pixels.data(), 1, pixels.size(), file) == pixels.size());
        fclose(file);
        CHECK(SUCCEEDED(
            d->CreateTexture(width, height, 1, 0, D3DFMT_DXT1, D3DPOOL_MANAGED, art.out(), nullptr)));
        D3DLOCKED_RECT lock{};
        CHECK(SUCCEEDED(art->LockRect(0, &lock, nullptr, 0)));
        for (UINT y = 0; y < rows; ++y)
            memcpy((BYTE *)lock.pBits + y * lock.Pitch, pixels.data() + y * rowBytes, rowBytes);
        art->UnlockRect(0);
    } else {
        CHECK(SUCCEEDED(
            d->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, art.out(), nullptr)));
        D3DLOCKED_RECT lock{};
        CHECK(SUCCEEDED(art->LockRect(0, &lock, nullptr, 0)));
        for (UINT y = 0; y < height; ++y)
            for (UINT x = 0; x < width; ++x)
                ((DWORD *)((BYTE *)lock.pBits + y * lock.Pitch))[x] =
                    (x * 19 + y * 23) % 47 == 0 ? 0xffa0b0c0 : 0xff101830;
        art->UnlockRect(0);
    }
    Com<IDirect3DVertexShader9> vs;
    Com<IDirect3DPixelShader9> ps;
    CHECK(SUCCEEDED(d->CreateVertexShader(ShaderSkyVS, vs.out())));
    CHECK(SUCCEEDED(d->CreatePixelShader(ShaderSkyPS, ps.out())));
    D3DMATRIX id{};
    id._11 = id._22 = id._33 = id._44 = 1;
    D3DMATRIX projection{};
    projection._11 = 1;
    projection._22 = float(W) / H;
    projection._33 = 1;
    projection._34 = 1;
    projection._43 = -.1f;
    auto render = [&](float detail, float stars, bool night, bool cloud = false, float yaw = 0,
                      bool native = false) {
        D3DMATRIX view = id;
        view._11 = view._33 = std::cos(yaw);
        view._13 = std::sin(yaw);
        view._31 = -std::sin(yaw);
        d->SetTransform(D3DTS_WORLD, &id);
        d->SetTransform(D3DTS_VIEW, &view);
        d->SetTransform(D3DTS_PROJECTION, &projection);
        d->SetFVF(0x142);
        d->SetTexture(0, art);
        d->SetVertexShader(native ? nullptr : vs.p);
        d->SetPixelShader(native ? nullptr : ps.p);
        d->SetRenderState(D3DRS_ZENABLE, FALSE);
        d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        d->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        d->SetRenderState(D3DRS_LIGHTING, FALSE);
        d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        d->SetRenderState(D3DRS_FOGENABLE, FALSE);
        d->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        d->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        for (auto key : {D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER})
            d->SetSamplerState(0, key, D3DTEXF_LINEAR);
        d->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        d->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
        d->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        d->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
        D3DMATRIX matrices[] = {id, view, projection, id};
        d->SetVertexShaderConstantF(0, &matrices[0].m[0][0], 16);
        float layout[4]{};
        d->SetVertexShaderConstantF(16, layout, 1);
        float c[25][4]{};
        c[0][1] = .5f;
        c[0][2] = 1;
        c[1][0] = cloud ? 1.f : 0;
        c[1][1] = night ? 1.f : 0;
        c[11][1] = 1;
        c[20][0] = 2;
        c[20][1] = D3DTA_TEXTURE;
        c[20][3] = 2;
        c[21][0] = D3DTA_TEXTURE;
        c[24][0] = 1.f / width;
        c[24][1] = 1.f / height;
        c[24][2] = detail;
        c[24][3] = stars;
        d->SetPixelShaderConstantF(0, c[0], 25);
        struct V {
            float x, y, z;
            DWORD color;
            float u, v;
        };
        const float h = float(H) / W;
        V q[] = {{-1, -h, 1, 0xffffffff, 0, 1},
                 {-1, h, 1, 0xffffffff, 0, 0},
                 {1, -h, 1, 0xffffffff, .25f, 1},
                 {1, h, 1, 0xffffffff, .25f, 0}};
        d->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff000000, 1, 0);
        d->BeginScene();
        auto hr = d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(V));
        d->EndScene();
        return SUCCEEDED(hr);
    };
    CHECK(render(0, 0, true, false, 0, true));
    auto original = Read(d);
    CHECK(original.size() == W * H);
    CHECK(render(0, 1, true));
    CHECK(Difference(original, Read(d)) == 0);
    CHECK(render(1, 0, false));
    auto day = Read(d);
    CHECK(render(1, 1, false));
    CHECK(Difference(day, Read(d)) == 0);
    CHECK(render(1, 0, true, true));
    auto clouds = Read(d);
    CHECK(render(1, 1, true, true));
    CHECK(Difference(clouds, Read(d)) == 0);
    CHECK(render(1, 0, true));
    auto coarse = Read(d);
    CHECK(render(1, 1, true));
    auto fine = Read(d);
    const auto starPixels = Difference(coarse, fine);
    printf("Fine star pixels=%u\n", starPixels);
    CHECK(starPixels > 50);
    CHECK(render(1, 1, true, false, .25f));
    CHECK(Difference(fine, Read(d)) > 1000);
    CHECK(render(1, 1, true));
    CHECK(Difference(fine, Read(d)) == 0);
    Save(original, L"build/sky-detail-native.bmp");
    Save(fine, L"build/sky-detail-improved.bmp");
    // The night panorama also contains gray clouds. Fine stars belong behind that artwork.
    width = 64; height = 32;
    CHECK(SUCCEEDED(d->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8,
                                    D3DPOOL_MANAGED, art.out(), nullptr)));
    D3DLOCKED_RECT cloudLock{};
    CHECK(SUCCEEDED(art->LockRect(0, &cloudLock, nullptr, 0)));
    for (UINT y = 0; y < height; ++y)
        for (UINT x = 0; x < width; ++x)
            ((DWORD *)((BYTE *)cloudLock.pBits + y * cloudLock.Pitch))[x] = 0xff606060;
    art->UnlockRect(0);
    CHECK(render(1, 0, true));
    auto opaqueClouds = Read(d);
    CHECK(render(1, 1, true));
    CHECK(Difference(opaqueClouds, Read(d)) == 0);
    puts("PASS: neutral sky parity, daylight/cloud isolation, fine stars and camera round trip");
    return 0;
}
