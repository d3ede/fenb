#include "effects.hpp"
#include "state_guard.hpp"
#include <cstdio>
#include <cstring>
#include <array>
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
    static void ageSources(SceneFX &fx, ULONGLONG milliseconds) {
        for(auto &source:fx.sources_) if(source.key) source.seen-=milliseconds;
    }
    static unsigned sourceCount(const SceneFX &fx) {
        unsigned count=0;for(const auto &source:fx.sources_)count+=source.key!=0;return count;
    }
    static bool hasSkyBackdrop(const SceneFX &fx) {
        return fx.skyBackdrop_.texture.p != nullptr;
    }
    static float surfaceMaskMaximum(SceneFX &fx,IDirect3DDevice9 *d) {
        Com<IDirect3DSurface9> read;
        if(FAILED(d->CreateOffscreenPlainSurface(fx.surfaceMask_.width,fx.surfaceMask_.height,
            D3DFMT_A16B16G16R16F,D3DPOOL_SYSTEMMEM,read.out(),nullptr)) ||
           FAILED(d->GetRenderTargetData(fx.surfaceMask_.surface,read)))return -1;
        D3DLOCKED_RECT lock{};if(FAILED(read->LockRect(&lock,nullptr,D3DLOCK_READONLY)))return -1;
        unsigned maximum=0;
        for(UINT y=0;y<fx.surfaceMask_.height;++y)for(UINT x=0;x<fx.surfaceMask_.width;++x)
            maximum=std::max(maximum,unsigned(((unsigned short *)((BYTE *)lock.pBits+y*lock.Pitch))[x*4]));
        read->UnlockRect();return float(maximum);
    }
    static std::array<float, 3> sourcePosition(SceneFX &fx, IDirect3DDevice9 *d) {
        Com<IDirect3DSurface9> read;
        if (FAILED(d->CreateOffscreenPlainSurface(8, 4, D3DFMT_A16B16G16R16F, D3DPOOL_SYSTEMMEM, read.out(),
                                                  nullptr)) ||
            FAILED(d->GetRenderTargetData(fx.emitterPosition_[3].surface, read)))
            return {999, 999, 999};
        D3DLOCKED_RECT lock{};
        if (FAILED(read->LockRect(&lock, nullptr, D3DLOCK_READONLY)))
            return {999, 999, 999};
        auto half = [](unsigned h) {
            float v = (h & 1023) / 1024.f;
            int e = (h >> 10) & 31;
            return (h & 0x8000 ? -1.f : 1.f) * (e ? std::ldexp(1 + v, e - 15) : std::ldexp(v, -14));
        };
        auto *p = static_cast<unsigned short *>(lock.pBits);
        float weight = half(p[3]);
        std::array<float, 3> result{};
        for (int i = 0; i < 3; ++i)
            result[i] = half(p[i]) / std::max(.000001f, weight) + fx.sourceOrigin_[i];
        read->UnlockRect();
        return result;
    }
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
UINT W = 640, H = 400;
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
// Optional local-asset verification reproduces the client's D3DX ARGB8 uploads.
// The shipping DLL has no D3DX dependency and no client artwork is copied into Git.
bool LoadMaterial(IDirect3DDevice9 *d,const wchar_t *path,Com<IDirect3DTexture9> &texture) {
    HMODULE module=LoadLibraryExW(L"d3dx9_43.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module)return false;
    using Load=HRESULT(WINAPI *)(IDirect3DDevice9 *,LPCWSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,
                                D3DPOOL,DWORD,DWORD,D3DCOLOR,void *,void *,IDirect3DTexture9 **);
    auto load=reinterpret_cast<Load>(GetProcAddress(module,"D3DXCreateTextureFromFileExW"));
    const bool ok=load && SUCCEEDED(load(d,path,0xffffffff,0xffffffff,1,0,D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED,1,1,0,nullptr,nullptr,texture.out()));
    FreeLibrary(module);return ok;
}
} // namespace
int main(int argc, char **argv) {
    const bool surfaceBenchmark=argc>1 && !strcmp(argv[1],"--surface-benchmark");
    if(surfaceBenchmark){W=1920;H=1080;}
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
    if (argc > 1 && !surfaceBenchmark)
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
    bool emit = false, nativePoint = true, occluded = false, foreground = false;
    float cameraYaw = 0, cameraHeight = 4;
    bool cameraOrbit = false;
    bool planeOnly = false, moonBackdrop = false;
    bool nativeFogScene = false, horizonCube = false, hostilePostState = false, swapHudDepth = false;
    Com<IDirect3DCubeTexture9> fogSky;
    CHECK(SUCCEEDED(d->CreateCubeTexture(4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                       fogSky.out(), nullptr)));
    for (UINT face = 0; face < 6; ++face) {
        D3DLOCKED_RECT skyLock{};
        CHECK(SUCCEEDED(fogSky->LockRect(D3DCUBEMAP_FACES(face), 0, &skyLock, nullptr, 0)));
        for (UINT y = 0; y < 4; ++y)
            for (UINT x = 0; x < 4; ++x)
                ((DWORD *)((BYTE *)skyLock.pBits + y * skyLock.Pitch))[x] =
                    face == D3DCUBEMAP_FACE_POSITIVE_Y ? 0xff3070cc : 0xffb06030;
        fogSky->UnlockRect(D3DCUBEMAP_FACES(face), 0);
    }
    int moonShift = 0;
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
    unsigned farEmitters = 0;
    float emitterJitter = 0;
    std::array<float, 3> sceneOffset{};
    // Native building light maps can leave stage 1 selecting the unchanged stage-0 result.
    DWORD emitterStage1 = D3DTOP_DISABLE, emitterStage1Argument = D3DTA_CURRENT;
    bool surfaceFixture = false, surfaceGrass = false, atlasGrass = false, surfacePattern = false;
    unsigned surfaceTiles=1, surfaceUnknownTiles=0;
    bool surfaceBuffered=true, surfaceBlend=false, surfaceBadNormals=false;
    float surfaceDepthBias=0, surfaceSlopeBias=0;
    bool surfaceVerticesReady=false, surfaceVerticesGrass=false;
    Com<IDirect3DVertexBuffer9> surfaceVertices;
    Com<IDirect3DTexture9> paving;
    CHECK(SUCCEEDED(d->CreateTexture(2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,paving.out(),nullptr)));
    CHECK(SUCCEEDED(paving->LockRect(0,&lock,nullptr,0)));
    for(int y=0;y<2;++y)for(int x=0;x<2;++x)
        ((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=0xff808080;
    paving->UnlockRect(0);
    DWORD pavingTag=32;
    paving->SetPrivateData(EmitterTag,&pavingTag,4,0);
    Com<IDirect3DTexture9> unknownPaving;
    CHECK(SUCCEEDED(d->CreateTexture(2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,unknownPaving.out(),nullptr)));
    DWORD unknownPavingTag=4;unknownPaving->SetPrivateData(EmitterTag,&unknownPavingTag,4,0);
    Com<IDirect3DTexture9> surfaceCoverage;
    CHECK(SUCCEEDED(d->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,surfaceCoverage.out(),nullptr)));
    auto setSurfaceCoverage=[&](BYTE alpha) {
        D3DLOCKED_RECT coverage{};if(FAILED(surfaceCoverage->LockRect(0,&coverage,nullptr,0)))return false;
        *(DWORD*)coverage.pBits=(DWORD(alpha)<<24)|0xffffff;surfaceCoverage->UnlockRect(0);return true;
    };
    CHECK(setSurfaceCoverage(255));
    struct Vertex {
        float x, y, z;
        DWORD color;
    };
    auto render = [&](bool solid = false) {
        fx.sceneFX().beginFrame();
        d->SetVertexShader(nullptr);
        d->SetPixelShader(nullptr);
        d->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        auto sceneWorld = id;
        for (int axis = 0; axis < 3; ++axis) sceneWorld.m[3][axis] = sceneOffset[axis];
        d->SetTransform(D3DTS_WORLD, &sceneWorld);
        D3DMATRIX currentView = view;
        if (cameraYaw != 0 || cameraOrbit) {
            // Orbit around the receiver while keeping the world and emissive geometry fixed.
            float ex = std::sin(cameraYaw) * 12, ez = 4 - std::cos(cameraYaw) * 12;
            float zx = -ex, zy = -cameraHeight, zz = 4 - ez;
            float length = std::sqrt(zx * zx + zy * zy + zz * zz);
            zx /= length;
            zy /= length;
            zz /= length;
            float xx = zz, xz = -zx;
            length = std::sqrt(xx * xx + xz * xz);
            xx /= length;
            xz /= length;
            float yx = zy * xz, yy = zz * xx - zx * xz, yz = -zy * xx;
            currentView = {xx,
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
                           -ex * yx - cameraHeight * yy - ez * yz,
                           -ex * zx - cameraHeight * zy - ez * zz,
                           1};
        }
        for (int axis = 0; axis < 3; ++axis)
            for (int component = 0; component < 3; ++component)
                currentView.m[3][axis] -= sceneOffset[component] * currentView.m[component][axis];
        d->SetTransform(D3DTS_VIEW, &currentView);
        d->SetTransform(D3DTS_PROJECTION, &projection);
        d->SetRenderState(D3DRS_ZENABLE, TRUE);
        d->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        d->SetRenderState(D3DRS_LIGHTING, FALSE);
        d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        DWORD depthBias=0;memcpy(&depthBias,&surfaceDepthBias,4);
        d->SetRenderState(D3DRS_DEPTHBIAS,depthBias);
        DWORD slopeBias=0;memcpy(&slopeBias,&surfaceSlopeBias,4);
        d->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,slopeBias);
        d->SetRenderState(D3DRS_FOGENABLE, FALSE);
        if (nativeFogScene) {
            float start = 0, end = 30; DWORD startBits = 0, endBits = 0;
            memcpy(&startBits, &start, 4); memcpy(&endBits, &end, 4);
            d->SetRenderState(D3DRS_FOGENABLE, TRUE);
            d->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
            d->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
            d->SetRenderState(D3DRS_RANGEFOGENABLE, FALSE);
            d->SetRenderState(D3DRS_FOGSTART, startBits);
            d->SetRenderState(D3DRS_FOGEND, endBits);
            d->SetRenderState(D3DRS_FOGCOLOR, 0xffb06030);
        }
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
        light.Position = {sceneOffset[0], 3 + sceneOffset[1], 10 + sceneOffset[2]};
        light.Diffuse = {2, .8f, .2f, 1};
        light.Range = 12;
        light.Attenuation0 = 1;
        d->SetLight(1, &light);
        d->LightEnable(1, nativePoint);
        d->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                 solid ? solidColor : (balanceScene ? 0xff101827 : 0xff8faec4), 1, 0);
        if(moonBackdrop) {
            D3DRECT moon{LONG(W/2-30+moonShift),50,LONG(W/2+30+moonShift),110};
            d->Clear(1,&moon,D3DCLEAR_TARGET,0xffd0e8ff,1,0);
        }
        fx.sceneFX().observe(settings);
        if (horizonCube)
            fx.sceneFX().skyEnvironment(fogSky);
        if (FAILED(d->BeginScene()))
            return false;
        if (!solid) {
            std::vector<Vertex> floor;
            for (int z = -5; z < 100; ++z)
                for (int x = -15; x < 15; ++x) {
                    DWORD c = darkScene ? 0xff25211c : ((x + z) & 1 ? 0xff7c725a : 0xffb2a581);
                    if(planeOnly)c=0xff808080;
                    for (auto v : std::initializer_list<Vertex>{{float(x), 0, float(z), c},
                                                                {float(x), 0, float(z + 1), c},
                                                                {float(x + 1), 0, float(z), c},
                                                                {float(x + 1), 0, float(z), c},
                                                                {float(x), 0, float(z + 1), c},
                                                                {float(x + 1), 0, float(z + 1), c}})
                        floor.push_back(v);
                }
            d->DrawPrimitiveUP(D3DPT_TRIANGLELIST, UINT(floor.size() / 3), floor.data(), sizeof(Vertex));
            if(surfaceFixture) {
                struct PavingVertex {float x,y,z,nx,ny,nz,u,v,u1,v1;};
                std::vector<PavingVertex> maskFloor;
                for(const auto &v:floor)maskFloor.push_back({v.x,v.y,v.z,surfaceBadNormals && v.x>0?.3f:0.f,1,0,
                    atlasGrass?.9f:.3f,atlasGrass?.77f:.3f,0,0});
                d->SetFVF(0x212); d->SetTexture(0,paving);
                if(surfaceBlend) {
                    d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
                    d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
                    d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
                    d->SetTexture(1,surfaceCoverage);
                    d->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
                    d->SetTextureStageState(1,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
                    d->SetTextureStageState(1,D3DTSS_TEXCOORDINDEX,1);
                    d->SetTextureStageState(1,D3DTSS_TEXTURETRANSFORMFLAGS,0);
                }
                if(surfaceBuffered) {
                    if(!surfaceVertices.p && FAILED(d->CreateVertexBuffer(UINT(maskFloor.size()*sizeof(PavingVertex)),
                        D3DUSAGE_WRITEONLY,0x212,D3DPOOL_MANAGED,surfaceVertices.out(),nullptr)))return false;
                    if(!surfaceVerticesReady || surfaceVerticesGrass!=atlasGrass) {
                        void *data=nullptr;
                        if(FAILED(surfaceVertices->Lock(0,0,&data,0)))return false;
                        memcpy(data,maskFloor.data(),maskFloor.size()*sizeof(PavingVertex));surfaceVertices->Unlock();
                        surfaceVerticesReady=true;surfaceVerticesGrass=atlasGrass;
                    }
                    d->SetStreamSource(0,surfaceVertices,0,sizeof(PavingVertex));
                }
                for(unsigned tile=0;tile<surfaceTiles;++tile) {
                    const UINT first=UINT(maskFloor.size()/3)*tile/surfaceTiles;
                    const UINT end=UINT(maskFloor.size()/3)*(tile+1)/surfaceTiles;
                    fx.sceneFX().captureSurface(settings,[&] {
                        return surfaceBuffered?d->DrawPrimitive(D3DPT_TRIANGLELIST,first*3,end-first):
                            d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,end-first,maskFloor.data()+first*3,sizeof(PavingVertex));
                    },surfaceBuffered?SurfaceDraw{D3DPT_TRIANGLELIST,first*3,end-first}:SurfaceDraw{});
                }
                if(surfaceUnknownTiles) {
                    d->SetTexture(0,unknownPaving);
                    const UINT triangles=UINT(maskFloor.size()/3);
                    for(unsigned tile=0;tile<surfaceUnknownTiles;++tile) {
                        const UINT first=triangles*tile/surfaceUnknownTiles;
                        const UINT end=triangles*(tile+1)/surfaceUnknownTiles;
                        fx.sceneFX().captureSurface(settings,[&] {
                            return surfaceBuffered?d->DrawPrimitive(D3DPT_TRIANGLELIST,first*3,end-first):
                                d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,end-first,maskFloor.data()+first*3,sizeof(PavingVertex));
                        },surfaceBuffered?SurfaceDraw{D3DPT_TRIANGLELIST,first*3,end-first}:SurfaceDraw{});
                    }
                    d->SetTexture(0,paving);
                }
                if(surfaceGrass) {
                    // An opaque grass layer on the same depth plane must erase paving response.
                    DWORD unknown=4;paving->SetPrivateData(EmitterTag,&unknown,4,0);
                    d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
                    d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
                    d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
                    fx.sceneFX().captureSurface(settings,[&] {
                        return d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(maskFloor.size()/3),maskFloor.data(),sizeof(PavingVertex));
                    });
                    paving->SetPrivateData(EmitterTag,&pavingTag,4,0);
                    d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
                }
                if(surfaceBlend) {
                    d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
                    d->SetTexture(1,nullptr);d->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
                }
                d->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);d->SetTexture(0,nullptr);
            }
            Vertex wall[] = {{-2, 0, 18, 0xff948674},
                             {2, 0, 18, 0xff948674},
                             {-2, 7, 18, 0xff948674},
                             {2, 7, 18, 0xff948674}};
            if (balanceScene)
                for (auto &v : wall)
                    v.color = 0xffc0c0c0;
            if (surfaceFixture)
                for (auto &v : wall) v.color=0xff2060d0;
            if(!planeOnly) {
                if(surfacePattern)for(int stripe=0;stripe<12;++stripe) {
                    const float left=-2+stripe/3.f,right=left+1/3.f;
                    const DWORD color=stripe&1?0xff1030e0:0xffd0a030;
                    Vertex band[]={{left,0,18,color},{right,0,18,color},{left,7,18,color},{right,7,18,color}};
                    d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,band,sizeof(Vertex));
                }else d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, wall, sizeof(Vertex));
            }
            if (foreground) {
                Vertex character[] = {{-.35f, 0, 3, 0xff506070},
                                      {.35f, 0, 3, 0xff506070},
                                      {-.35f, 2, 3, 0xff506070},
                                      {.35f, 2, 3, 0xff506070}};
                d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, character, sizeof(Vertex));
            }
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
            d->SetTextureStageState(1, D3DTSS_COLOROP, emitterStage1);
            d->SetTextureStageState(1, D3DTSS_COLORARG1, emitterStage1Argument);
            d->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
            d->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
            D3DMATERIAL9 mat{};
            mat.Diffuse = {1, 1, 1, 1};
            d->SetMaterial(&mat);
            for (unsigned i=0;i<farEmitters;++i) {
                auto distant=sceneWorld;distant._41+=200+i*10.f;
                d->SetTransform(D3DTS_WORLD,&distant);
                fx.sceneFX().captureEmitter(settings,[&]() {
                    return d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(E));
                });
            }
            auto lamp=sceneWorld;lamp._41+=emitterJitter;
            d->SetTransform(D3DTS_WORLD,&lamp);
            fx.sceneFX().captureEmitter(
                settings, [&]() { return d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(E)); });
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(E));
        }
        Com<IDirect3DSurface9> bb;
        d->GetRenderTarget(0, bb.out());
        bool ok = true;
        Com<IDirect3DSurface9> mainDepth,hudDepth;
        if(swapHudDepth) {
            D3DSURFACE_DESC desc{};
            ok = SUCCEEDED(d->GetDepthStencilSurface(mainDepth.out())) && SUCCEEDED(mainDepth->GetDesc(&desc)) &&
                 SUCCEEDED(d->CreateDepthStencilSurface(W,H,D3DFMT_D16,desc.MultiSampleType,desc.MultiSampleQuality,
                                                       TRUE,hudDepth.out(),nullptr));
            if(!ok)return false;
            d->SetDepthStencilSurface(hudDepth);
            d->Clear(0,nullptr,D3DCLEAR_ZBUFFER,0,.05f,0); // UI/marker depth must not become world geometry.
        }
        if(hostilePostState) {
            const float plane[]={0,0,0,-1};
            d->SetClipPlane(0,plane);
            d->SetRenderState(D3DRS_CLIPPLANEENABLE,1);
            d->SetRenderState(D3DRS_MULTISAMPLEMASK,0);
            d->SetRenderState(D3DRS_WRAP0,D3DWRAP_U|D3DWRAP_V);
        }
        {
            StateGuard state(d);
            if (fx.prepare(bb, settings)) {
                state.isolateTargets();
                fx.draw(bb, settings);
            } else if (settings.postEnabled())
                ok = false;
        }
        if(hostilePostState) {
            DWORD clips=0,mask=1,wrap=0;
            d->GetRenderState(D3DRS_CLIPPLANEENABLE,&clips);
            d->GetRenderState(D3DRS_MULTISAMPLEMASK,&mask);
            d->GetRenderState(D3DRS_WRAP0,&wrap);
            ok &= clips==1 && mask==0 && wrap==(D3DWRAP_U|D3DWRAP_V);
            d->SetRenderState(D3DRS_CLIPPLANEENABLE,0);
            d->SetRenderState(D3DRS_MULTISAMPLEMASK,0xffffffff);
            d->SetRenderState(D3DRS_WRAP0,0);
        }
        // Simulated HUD after the world processing must retain its exact RGB.
        if(swapHudDepth)d->SetDepthStencilSurface(mainDepth);
        D3DRECT rect{8, 8, 40, 28};
        d->Clear(1, &rect, D3DCLEAR_TARGET, 0xff40a0e0, 1, 0);
        d->EndScene();
        return ok;
    };
    if(surfaceBenchmark) {
        surfaceFixture=true;surfaceTiles=28;surfaceUnknownTiles=1800;
        settings.surfaceStrength=1;settings.surfaceDistance=100;settings.surfaceRoughness=.65f;
        Com<IDirect3DQuery9> fence;CHECK(SUCCEEDED(d->CreateQuery(D3DQUERYTYPE_EVENT,fence.out())));
        auto flush=[&] {fence->Issue(D3DISSUE_END);const auto start=GetTickCount64();
            while(fence->GetData(nullptr,0,D3DGETDATA_FLUSH)==S_FALSE){if(GetTickCount64()-start>5000)return false;Sleep(0);}return true;};
        LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
        for(bool detailed:{false,true})for(float yaw:{0.f,.8f,-.65f})for(bool enabled:{false,true}) {
            cameraYaw=yaw;settings.sceneQuality=detailed;settings.surfaceReflections=enabled;
            for(int i=0;i<4;++i)CHECK(render());CHECK(flush());
            LARGE_INTEGER start{},end{};QueryPerformanceCounter(&start);
            for(int i=0;i<16;++i)CHECK(render());CHECK(flush());QueryPerformanceCounter(&end);
            printf("SURFACE BENCH quality=%d yaw=%.2f enabled=%d: %.3f ms/frame (1080p, 28 paving + 1800 terrain draws)\n",
                detailed,yaw,enabled,double(end.QuadPart-start.QuadPart)*1000/frequency.QuadPart/16);
            if(enabled)CHECK(SceneFXFixture::surfaceMaskMaximum(fx.sceneFX(),d)==0);
        }
        return 0;
    }
    CHECK(render());
    auto original = Read(d);
    CHECK(original.size() == W * H);
    CHECK(Save(original, L"build/scene-original.bmp"));
    settings.color=true;settings.exposure=1;
    CHECK(render());auto normalPost=Read(d);
    hostilePostState=true;
    CHECK(render());auto isolatedPost=Read(d);
    unsigned postError=0;
    for(size_t i=0;i<normalPost.size();++i) for(int shift:{0,8,16})
        postError+=abs(int((normalPost[i]>>shift)&255)-int((isolatedPost[i]>>shift)&255));
    CHECK(postError < normalPost.size()); // Allow subpixel MSAA edge changes, never a missing color pass.
    CHECK(normalPost[(H-20)*W+W/2]==isolatedPost[(H-20)*W+W/2]);
    hostilePostState=false;settings.color=false;settings.exposure=0;
    settings.fog = true;
    settings.fogDensity = .025f;
    settings.fogStart = 0;
    CHECK(render());auto worldFog=Read(d);
    swapHudDepth=true;CHECK(render());auto markerDepthFog=Read(d);swapHudDepth=false;
    unsigned depthSourceError=0;
    for(size_t i=0;i<worldFog.size();++i)for(int shift:{0,8,16})
        depthSourceError+=abs(int((worldFog[i]>>shift)&255)-int((markerDepthFog[i]>>shift)&255));
    CHECK(depthSourceError < worldFog.size());
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
    // Fully fogged distant geometry converges to the actual sky drawn before the world.
    settings.fogDensity = 2;
    CHECK(render());
    auto horizon = Read(d);
    CHECK(std::abs(Luma(horizon[120 * W + W / 2]) - Luma(original[50 * W + 20])) <= 3);
    CHECK(horizon[50 * W + 20] == original[50 * W + 20]);
    CHECK(Save(horizon, L"build/scene-fog-horizon.bmp"));
    moonBackdrop=true;moonShift=-45;
    CHECK(render()); auto moonLeft=Read(d);
    moonShift=45;CHECK(render());auto moonRight=Read(d);
    unsigned leakedMoon=0;
    for(UINT y=60;y<120;++y)for(UINT x=W/2-20;x<W/2+20;++x)
        leakedMoon += std::abs(Luma(moonLeft[y*W+x])-Luma(moonRight[y*W+x]))>3;
    CHECK(leakedMoon==0);
    CHECK(Save(moonRight,L"build/scene-fog-no-moon-through-wall.bmp"));
    moonBackdrop=false;
    // Horizon color comes from world-space side faces, not a blue zenith/view average.
    horizonCube = true;
    CHECK(render());
    auto warmHorizon = Read(d);
    const DWORD expectedAir = 0xffb06030;
    for (int shift : {0, 8, 16})
        CHECK(abs(int((warmHorizon[120 * W + W / 2] >> shift) & 255) -
                  int((expectedAir >> shift) & 255)) <= 2);
    CHECK(Save(warmHorizon, L"build/scene-fog-warm-horizon.bmp"));
    nativeFogScene = true;
    settings.fog = false;
    CHECK(render()); auto nativeFogOnly = Read(d);
    settings.fog = true; settings.fogDensity = .001f;
    CHECK(render()); auto compatibleFog = Read(d);
    unsigned doubleFogged = 0;
    for (UINT y = H / 2; y < H - 10; ++y)
        for (UINT x = W / 4; x < W * 3 / 4; ++x)
            doubleFogged += abs(Luma(nativeFogOnly[y * W + x]) - Luma(compatibleFog[y * W + x])) > 3;
    printf("Native fog detail changed pixels: %u\n", doubleFogged);
    CHECK(doubleFogged == 0);
    CHECK(Save(compatibleFog, L"build/scene-fog-native-detail.bmp"));
    nativeFogScene = horizonCube = false;
    CHECK(SceneFXFixture::hasSkyBackdrop(fx.sceneFX()));
    disableEffect(3);
    settings.nightFoliage = 0;
    settings.fxaa = true;
    CHECK(render());
    CHECK(!SceneFXFixture::hasSkyBackdrop(fx.sceneFX()));
    settings.fxaa = false;
    settings.ssao = true;
    settings.aoStrength = 2;
    settings.aoRadius = 3;
    CHECK(render());
    auto ao = Read(d);
    CHECK(Difference(original, ao) > 100);
    CHECK(Save(ao, L"build/scene-ssao.bmp"));
    planeOnly=true;
    for(float yaw : {-.4f,0.f,.4f}) {
        cameraYaw=yaw;settings.ssao=false;CHECK(render());auto plain=Read(d);
        settings.ssao=true;CHECK(render());auto shaded=Read(d);
        unsigned banded=0;
        for(UINT y=H/2+30;y<H-5;++y)for(UINT x=W/4;x<W*3/4;++x)
            banded += std::abs(Luma(plain[y*W+x])-Luma(shaded[y*W+x]))>3;
        printf("Unoccluded slope AO band pixels: %u, yaw=%.2f\n",banded,yaw);
        CHECK(banded==0);
    }
    planeOnly=false;cameraYaw=0;
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
    // A strong directional game light over a dark sky must not synthesize a giant disc.
    solidColor = 0xff202030;
    settings.sunRays = false;
    CHECK(render(true));
    auto darkSkyWithoutRays = Read(d);
    settings.sunRays = true;
    CHECK(render(true));
    CHECK(Difference(darkSkyWithoutRays, Read(d)) == 0);
    solidColor = 0xff303030;
    darkScene = true;
    settings.sunRays = false;
    CHECK(render());
    auto nightWithoutRays = Read(d);
    settings.sunRays = true;
    CHECK(render());
    CHECK(!fx.sceneFX().sunOnScreen()); // A bright blue channel does not make a night light the sun.
    CHECK(Difference(nightWithoutRays, Read(d)) == 0);
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
    settings.localStrength = 2;
    settings.localRange = 1.55f;
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
    auto fixedSource = SceneFXFixture::sourcePosition(fx.sceneFX(), d);
    CHECK(std::abs(fixedSource[0]) < .05f && std::abs(fixedSource[1] - 3) < .05f &&
          std::abs(fixedSource[2] - 9) < .05f);
    for (float yaw : {-.65f, .35f, .8f}) {
        cameraYaw = yaw;
        CHECK(render());
        auto rotatedSource = SceneFXFixture::sourcePosition(fx.sceneFX(), d);
        for (int axis = 0; axis < 3; ++axis)
            CHECK(std::abs(rotatedSource[axis] - fixedSource[axis]) < .02f);
    }
    cameraYaw = 0;
    CHECK(render());
    // Teleport between distant maps without toggling local lighting. Relative FP16 positions
    // must rebase, discard the previous map's cached lights and match a freshly enabled pass.
    for (const auto offset : {std::array<float, 3>{32768, 512, -65536},
                              std::array<float, 3>{0, 0, 0}}) {
        sceneOffset = offset;
        for (float yaw : {0.f, -.65f, .8f}) {
            cameraYaw = yaw;
            CHECK(render());
            CHECK(SceneFXFixture::sourceCount(fx.sceneFX()) == 1);
            const auto source = SceneFXFixture::sourcePosition(fx.sceneFX(), d);
            printf("Teleport light: offset %.0f %.0f %.0f, yaw %.2f, source %.3f %.3f %.3f\n",
                   offset[0], offset[1], offset[2], yaw, source[0], source[1], source[2]);
            for (int axis = 0; axis < 3; ++axis)
                CHECK(std::abs(source[axis] - offset[axis] - fixedSource[axis]) < .1f);
            const float beforeToggle = SceneFXFixture::lightMaximum(fx.sceneFX(), d);
            CHECK(beforeToggle > 0);
            settings.localLighting = false; CHECK(render());
            settings.localLighting = true; CHECK(render());
            CHECK(std::abs(SceneFXFixture::lightMaximum(fx.sceneFX(), d) - beforeToggle) < .03f);
        }
    }
    cameraYaw = 0;
    CHECK(render());
    // Real captured ARGB8 lamp and window identities, without seeding private tags.
    Com<IDirect3DTexture9> syntheticEmitter;
    syntheticEmitter.p=emitter.p;syntheticEmitter->AddRef();
    for(const wchar_t *name:{L"SFX/Texture/obj_GenLight01.dds",L"SFX/Texture/sfx_light02.dds",
                            L"Model/Texture/Obj_MaFlReq-light.dds"}) {
        auto path=std::wstring(L"X:/Games/Homesickness/")+name;
        if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES)continue;
        settings.localLighting=false;CHECK(render());
        CHECK(LoadMaterial(d,path.c_str(),emitter));
        settings.localLighting=true;CHECK(render());
        DWORD actualTag=0,size=4;
        CHECK(SUCCEEDED(emitter->GetPrivateData(EmitterTag,&actualTag,&size)) && (actualTag&1));
        CHECK(SceneFXFixture::sourceCount(fx.sceneFX())==1);
        const float active=SceneFXFixture::lightMaximum(fx.sceneFX(),d);
        CHECK(active>.001f);
        if(actualTag&256) {
            emit=false;SceneFXFixture::ageSources(fx.sceneFX(),700);
            CHECK(render());const float held=SceneFXFixture::lightMaximum(fx.sceneFX(),d);
            CHECK(std::abs(held-active)<.015f);
            for(float yaw:{-.65f,.8f}) {cameraYaw=yaw;CHECK(render());
                CHECK(SceneFXFixture::lightMaximum(fx.sceneFX(),d)>.001f);}
            cameraYaw=0;
            SceneFXFixture::ageSources(fx.sceneFX(),5000);CHECK(render());
            const auto expired=Read(d);
            settings.localLighting=false;CHECK(render());
            const auto disabled=Read(d);unsigned expiredDelta=0;
            for(size_t i=0;i<expired.size();++i)for(int shift:{0,8,16})
                expiredDelta=std::max(expiredDelta,unsigned(abs(int((expired[i]>>shift)&255)-int((disabled[i]>>shift)&255))));
            printf("Expired static light: mismatch=%u, maximum delta=%u\n",Difference(expired,disabled),expiredDelta);
            CHECK(expiredDelta<=(pp.MultiSampleType?32u:1u));
            settings.localLighting=true;
            emit=true;
        }
        printf("Actual lamp/window upload recognized; persistent=%d, light=%.3f\n",bool(actualTag&256),active);
    }
    settings.localLighting=false;CHECK(render());Drop(emitter.p);
    emitter.p=syntheticEmitter.p;emitter->AddRef();
    settings.localLighting=true;CHECK(render());
    const float directEmission = SceneFXFixture::lightMaximum(fx.sceneFX(), d);
    settings.localLighting = false; CHECK(render());
    emitterStage1 = D3DTOP_SELECTARG1;
    settings.localLighting = true; CHECK(render());
    CHECK(SceneFXFixture::sourceCount(fx.sceneFX()) == 1);
    CHECK(std::abs(SceneFXFixture::lightMaximum(fx.sceneFX(), d) - directEmission) < .01f);
    CHECK(Save(Read(d), L"build/scene-window-lighting.bmp"));
    settings.localLighting = false; CHECK(render());
    emitterStage1Argument = D3DTA_TEXTURE; // An actual second texture is not a pass-through.
    settings.localLighting = true; CHECK(render());
    CHECK(SceneFXFixture::sourceCount(fx.sceneFX()) == 0);
    settings.localLighting = false; CHECK(render());
    emitterStage1Argument = D3DTA_CURRENT;
    emitterStage1 = D3DTOP_MODULATE; // Nor is a color-changing operation on CURRENT.
    settings.localLighting = true; CHECK(render());
    CHECK(SceneFXFixture::sourceCount(fx.sceneFX()) == 0);
    emitterStage1 = D3DTOP_DISABLE;
    CHECK(render());
    for (int frame=0;frame<40;++frame) {
        emitterJitter=(frame&1)?.0002f:-.0002f;
        CHECK(render());
        CHECK(SceneFXFixture::sourceCount(fx.sceneFX())==1);
        CHECK(SceneFXFixture::lightMaximum(fx.sceneFX(),d)>0);
    }
    emitterJitter=0;
    settings.localLighting=false;CHECK(render());
    settings.localLighting=true;farEmitters=40;
    CHECK(render());
    CHECK(SceneFXFixture::sourceCount(fx.sceneFX())==32);
    CHECK(SceneFXFixture::lightMaximum(fx.sceneFX(),d)>0);
    farEmitters=0;
    settings.localLighting=false;CHECK(render());
    settings.localLighting=true;CHECK(render());
    // A foreground body cannot stamp displaced copies into the light field.
    foreground = true;
    settings.localLighting = false;
    CHECK(render());
    auto bodyOff = Read(d);
    settings.localLighting = true;
    CHECK(render());
    auto bodyOn = Read(d);
    unsigned phantomPixels = 0, receiverPixels = 0;
    for (UINT y = H / 2 + 3; y < H - 3; ++y)
        for (UINT x = 3; x < W - 3; ++x) {
            bool sameReceiver = true;
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx)
                    sameReceiver &= bodyOff[(y + dy) * W + x + dx] == emitterOff[(y + dy) * W + x + dx];
            if (sameReceiver) {
                ++receiverPixels;
                phantomPixels += std::abs(Luma(bodyOn[y * W + x]) - Luma(emitterOn[y * W + x])) > 6;
            }
        }
    printf("Unchanged receiver pixels=%u, phantom light silhouettes=%u\n", receiverPixels, phantomPixels);
    CHECK(receiverPixels > 50000 && phantomPixels * 1000 < receiverPixels);
    CHECK(Save(bodyOn, L"build/scene-light-no-phantoms.bmp"));
    foreground = false;
    CHECK(render());
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
    // The bulb being hidden from the camera must not switch off illumination everywhere.
    // These are unshadowed lights; camera depth is not a substitute for light-space shadow maps.
    CHECK(SceneFXFixture::lightMaximum(fx.sceneFX(), d) > 0);
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
    surfaceFixture=true;
    // Keep the same color-resolve path for MSAA when comparing the optional reflection.
    settings.surfaceReflections=true; settings.surfaceStrength=0; CHECK(render()); const auto pavingOff=Read(d);
    settings.surfaceReflections=true; settings.surfaceStrength=1; settings.surfaceRoughness=.5f;
    CHECK(render()); const auto pavingOn=Read(d);
    printf("Paving reflection pixels: %u\n",Difference(pavingOff,pavingOn));
    CHECK(Difference(pavingOff,pavingOn)>100);
    unsigned reflectedBlue=0;
    for(UINT y=145;y<H;++y)for(UINT x=0;x<W;++x) {
        const auto a=pavingOff[y*W+x],b=pavingOn[y*W+x];
        reflectedBlue+=int(b&255)>int(a&255)+4 && int((a>>16)&255)>int((b>>16)&255)+2;
    }
    printf("Blue object reflected on paving: %u pixels\n",reflectedBlue);
    CHECK(reflectedBlue>200); // Native MSAA resolve differences alone cannot satisfy this.
    CHECK(pavingOn[12*W+12]==pavingOff[12*W+12]);
    CHECK(pavingOn[50*W+20]==pavingOff[50*W+20]);
    CHECK(Save(pavingOff,L"build/surface-reflections-off.bmp"));
    CHECK(Save(pavingOn,L"build/surface-reflections-on.bmp"));
    auto maximumDelta=[](const auto &a,const auto &b) {
        unsigned maximum=0;
        for(size_t i=0;i<a.size();++i)for(unsigned shift:{0u,8u,16u})
            maximum=std::max(maximum,unsigned(std::abs(int((a[i]>>shift)&255)-int((b[i]>>shift)&255))));
        return maximum;
    };
    CHECK(maximumDelta(pavingOff,pavingOn)>10);
    // Native terrain can first introduce paving with partial blend-map coverage.
    // Coverage changes reflectance, never the receiver depth or the reflected silhouette.
    surfaceBlend=true;
    unsigned previousBlue=0;unsigned long long previousEnergy=0;
    for(BYTE coverage:{BYTE(64),BYTE(128),BYTE(192),BYTE(255)}) {
        CHECK(setSurfaceCoverage(coverage));CHECK(render());const auto partial=Read(d);
        unsigned blue=0;unsigned long long energy=0;
        for(UINT y=145;y<H;++y)for(UINT x=0;x<W;++x) {
            const auto a=pavingOff[y*W+x],b=partial[y*W+x];
            blue+=int(b&255)>int(a&255)+2 && int((a>>16)&255)>int((b>>16)&255)+1;
            if(int(b&255)>int(a&255))energy+=int(b&255)-int(a&255);
        }
        printf("Blended paving coverage %u: reflected pixels=%u, energy=%llu\n",unsigned(coverage),blue,energy);
        CHECK(blue>200 && blue>=previousBlue && energy>previousEnergy);previousBlue=blue;previousEnergy=energy;
        CHECK(partial[12*W+12]==pavingOff[12*W+12]);
        if(coverage==128)CHECK(Save(partial,L"build/surface-blended-paving.bmp"));
        if(coverage==255) {
            printf("Opaque/fully blended paving delta: %u\n",maximumDelta(partial,pavingOn));
            CHECK(maximumDelta(partial,pavingOn)<=(pp.MultiSampleType?32u:1u));
        }
    }
    surfaceBlend=false;
    // Smoothed artist normals must not cut holes in an otherwise flat paving triangle.
    // Native MSAA color resolves may differ by one 8-sample coverage step at grid edges.
    const unsigned pavingResolveTolerance=pp.MultiSampleType?32u:1u;
    surfaceBuffered=false;surfaceBadNormals=true;CHECK(render());
    CHECK(maximumDelta(pavingOn,Read(d))<=pavingResolveTolerance);surfaceBadNormals=false;
    sceneOffset={7000,100,3300};CHECK(render());const auto offsetStone=Read(d);
    unsigned offsetBlue=0;
    for(UINT y=145;y<H;++y)for(UINT x=0;x<W;++x) {
        const auto a=pavingOff[y*W+x],b=offsetStone[y*W+x];
        offsetBlue+=int(b&255)>int(a&255)+4 && int((a>>16)&255)>int((b>>16)&255)+2;
    }
    CHECK(offsetBlue>reflectedBlue*9/10);
    sceneOffset={};
    surfaceBuffered=false;CHECK(render());
    CHECK(maximumDelta(pavingOn,Read(d))<=pavingResolveTolerance);
    surfaceBuffered=true;
    surfacePattern=true;settings.surfaceRoughness=0;CHECK(render());const auto sharpStone=Read(d);
    settings.surfaceRoughness=1;CHECK(render());const auto roughStone=Read(d);
    CHECK(Difference(sharpStone,roughStone)>100);
    CHECK(Save(sharpStone,L"build/surface-polished.bmp"));CHECK(Save(roughStone,L"build/surface-rough.bmp"));
    surfacePattern=false;settings.surfaceRoughness=.5f;
    planeOnly=true;horizonCube=true;settings.surfaceStrength=0;CHECK(render());const auto dryStone=Read(d);
    settings.surfaceStrength=1;CHECK(render());const auto skyStone=Read(d);
    CHECK(Difference(dryStone,skyStone)>500);
    CHECK(skyStone[50*W+20]==dryStone[50*W+20] && skyStone[12*W+12]==dryStone[12*W+12]);
    CHECK(Save(skyStone,L"build/surface-sky-gloss.bmp"));
    // An empty, horizontal receiver cannot reflect its own paving. Changing the
    // search range must leave the captured-sky fallback intact at overhead angles.
    cameraOrbit=true;cameraYaw=.3f;
    for(bool detailed:{false,true})for(const auto bias:std::array<std::array<float,2>,5>{{
        {0,0},{-.0000005f,0},{-.000001f,0},{0,-1},{0,1}}})for(float elevation:{4.f,12.f,35.f,65.f,120.f,180.f}) {
        cameraHeight=elevation;surfaceDepthBias=bias[0];surfaceSlopeBias=bias[1];settings.sceneQuality=detailed;
        settings.surfaceStrength=0;CHECK(render());const auto bareStone=Read(d);settings.surfaceStrength=1;
        settings.surfaceDistance=0;CHECK(render());const auto skyOnlyStone=Read(d);
        settings.surfaceDistance=50;CHECK(render());const auto tracedStone=Read(d);
        printf("Overhead paving height %.0f bias=%.7f slope=%.1f detailed=%d: range mismatch=%u, delta=%u\n",elevation,bias[0],bias[1],detailed,
            Difference(skyOnlyStone,tracedStone),maximumDelta(skyOnlyStone,tracedStone));
        unsigned untouched=0,interior=0;
        for(UINT y=4;y+4<H;++y)for(UINT x=4;x+4<W;++x) {
            const size_t at=y*W+x;
            if((bareStone[at]&0xffffff)!=0x808080 || (bareStone[at-4]&0xffffff)!=0x808080 ||
               (bareStone[at+4]&0xffffff)!=0x808080 || (bareStone[at-4*W]&0xffffff)!=0x808080 ||
               (bareStone[at+4*W]&0xffffff)!=0x808080)continue;
            ++interior;untouched+=(tracedStone[at]&0xffffff)==0x808080;
        }
        printf("Overhead paving coverage: unchanged=%u / %u interior pixels\n",untouched,interior);
        CHECK(Save(tracedStone,L"build/surface-overhead.bmp"));
        if(!pp.MultiSampleType && !detailed && bias[0]==-.000001f && !bias[1] && elevation==180)
            CHECK(Save(tracedStone,L"build/surface-overhead-fixed.bmp"));
        CHECK(maximumDelta(skyOnlyStone,tracedStone)<=(pp.MultiSampleType?32u:1u));
        CHECK(interior>100 && untouched<interior/50);
    }
    cameraOrbit=false;cameraYaw=0;cameraHeight=4;surfaceDepthBias=surfaceSlopeBias=0;settings.surfaceDistance=50;settings.sceneQuality=false;
    foreground=true;settings.surfaceStrength=0;CHECK(render());const auto nativeBody=Read(d);
    settings.surfaceStrength=1;CHECK(render());const auto reflectedGround=Read(d);unsigned bodyPixels=0;
    for(UINT y=1;y+1<H;++y)for(UINT x=1;x+1<W;++x) {
        const size_t at=y*W+x;
        if((nativeBody[at]&0xffffff)!=0x506070 || (nativeBody[at-1]&0xffffff)!=0x506070 ||
           (nativeBody[at+1]&0xffffff)!=0x506070 || (nativeBody[at-W]&0xffffff)!=0x506070 ||
           (nativeBody[at+W]&0xffffff)!=0x506070)continue;
        ++bodyPixels;CHECK((reflectedGround[at]&0xffffff)==0x506070);
    }
    CHECK(bodyPixels>50);foreground=false;
    planeOnly=false;horizonCube=false;
    surfaceGrass=true; CHECK(render());
    printf("Grass reflection mismatch pixels: %u, mask maximum: %.0f\n",Difference(pavingOff,Read(d)),
           SceneFXFixture::surfaceMaskMaximum(fx.sceneFX(),d));
    CHECK(Save(Read(d),L"build/surface-grass.bmp"));
    CHECK(SceneFXFixture::surfaceMaskMaximum(fx.sceneFX(),d)==0);
    // Hardware MSAA resolves can vary by one coverage step at checkerboard edges after
    // switching away from the native multisampled target. No reflection may remain.
    CHECK(maximumDelta(pavingOff,Read(d))<=(pp.MultiSampleType?32u:0u));
    surfaceGrass=false;
    // Current Flaris plaza model atlases must be recognized independently of terrain.
    // Its planter grass shares one texture with the paving and must remain matte.
    Com<IDirect3DTexture9> syntheticPaving;
    syntheticPaving.p=paving.p;syntheticPaving->AddRef();
    for(const wchar_t *name:{L"Model/Texture/Obj_NewFlsenCent01.dds",L"Model/Texture/Obj_NewFlEastCent01.dds",
                            L"Model/Texture/Obj_NewFlNorthCent01.dds",L"World/Texture/5da3e18d.tga"}) {
        auto path=std::wstring(L"X:/Games/Homesickness/")+name;
        if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES)continue;
        CHECK(LoadMaterial(d,path.c_str(),paving));
        settings.surfaceStrength=1;CHECK(render());
        DWORD actualTag=0,size=4;
        CHECK(SUCCEEDED(paving->GetPrivateData(EmitterTag,&actualTag,&size)) && (actualTag&(32|64|512)));
        CHECK(SceneFXFixture::surfaceMaskMaximum(fx.sceneFX(),d)>0);
        CHECK(maximumDelta(pavingOff,Read(d))>10);
        if(std::wcsstr(name,L".tga")) {
            const float roughness=settings.surfaceRoughness;
            for(float rough:{0.f,.55f,1.f}) {
                settings.surfaceRoughness=rough;CHECK(render());
                CHECK(SceneFXFixture::surfaceMaskMaximum(fx.sceneFX(),d)>0);
                const auto road=Read(d);unsigned blue=0;
                for(UINT y=145;y<H;++y)for(UINT x=0;x<W;++x) {
                    const DWORD a=pavingOff[y*W+x],b=road[y*W+x];
                    blue+=int(b&255)>int(a&255)+4 && int((a>>16)&255)>int((b>>16)&255)+2;
                }
                printf("Actual TGA road reflection: roughness=%.2f, blue pixels=%u\n",rough,blue);
                CHECK(blue>100);
            }
            settings.surfaceRoughness=roughness;
        }
        if(actualTag&512) {
            atlasGrass=true;CHECK(render());
            CHECK(SceneFXFixture::surfaceMaskMaximum(fx.sceneFX(),d)==0);
            atlasGrass=false;
        }
    }
    Drop(paving.p);paving.p=syntheticPaving.p;paving->AddRef();
    settings.surfaceStrength=0;CHECK(render());
    CHECK(maximumDelta(pavingOff,Read(d))<=(pp.MultiSampleType?32u:0u));
    settings.surfaceReflections=false; surfaceFixture=false;
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
    auto materialFrame = [&](bool sky, bool improve, bool day, bool blueNight = false,
                             bool universalFill = false, bool scroll = false, bool neutralSky = false,
                             DWORD skyTag = 3) {
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
        D3DMATRIX skyUV = id;
        skyUV._31 = .125f;
        skyUV._32 = .25f;
        skyUV._41 = .3125f;
        skyUV._42 = .0625f;
        d->SetTransform(D3DTS_TEXTURE0, &skyUV);
        if (scroll)
            d->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
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
        if (universalFill) {
            s.lightingBalance = improve;
            s.nightFoliage = 0;
        }
        if (neutralSky) {
            s.skyStrength = 0;
            s.sunHalo = 0;
            s.cloudContrast = 1;
            s.skyDetail = 0;
            s.skyStars = 0;
        }
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
                ok = fx.sceneFX().beginSky(s, skyTag);
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
            DWORD foliage = universalFill ? 4 : 2;
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
    CHECK(materialFrame(true, false, true, false, false, true, true));
    auto nativeScrolledSky = Read(d);
    CHECK(materialFrame(true, true, true, false, false, true, true));
    auto replacedScrolledSky = Read(d);
    unsigned wrongUV = 0;
    for (UINT i = 0; i < W * H; ++i)
        wrongUV += std::abs(Luma(nativeScrolledSky[i]) - Luma(replacedScrolledSky[i])) > 6;
    printf("Sky neutral native-stage/scroll mismatch pixels: %u\n", wrongUV);
    CHECK(wrongUV < 100);
    CHECK(materialFrame(true, true, false, false, false, false, false, 5));
    auto fixedNightStars = Read(d);
    CHECK(materialFrame(true, true, false, false, false, true, false, 5));
    CHECK(Difference(fixedNightStars, Read(d)) > 1000); // Keep the owner's intended native sky motion.
    CHECK(materialFrame(true, false, false, false, false, true, true, 5));
    auto nativeNightStars = Read(d);
    CHECK(materialFrame(true, true, false, false, false, true, true, 5));
    CHECK(Difference(nativeNightStars, Read(d)) == 0); // Detail zero retains native animation.
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
    CHECK(materialFrame(false, false, false, false, true));
    auto darkProp = Read(d);
    CHECK(materialFrame(false, true, false, false, true));
    CHECK(Difference(darkProp, Read(d)) > 1000); // Unknown opaque prop, not a foliage catalog match.
    CHECK(Save(Read(d), L"build/scene-night-prop.bmp"));
    CHECK(materialFrame(false, false, true, false, true));
    auto dayProp = Read(d);
    CHECK(materialFrame(false, true, true, false, true));
    CHECK(dayProp == Read(d));
    CHECK(materialFrame(false, false, true));
    auto dayLeaves = Read(d);
    CHECK(materialFrame(false, true, true));
    CHECK(dayLeaves == Read(d));
    // The exact lens-flare material becomes a smooth halo while preserving native fade/state.
    DWORD flareTag = 8;
    emitter->SetPrivateData(EmitterTag, &flareTag, 4, 0);
    auto flareFrame = [&](bool replace, float strength) {
        d->SetTransform(D3DTS_WORLD, &id);
        d->SetTransform(D3DTS_VIEW, &id);
        d->SetTransform(D3DTS_PROJECTION, &projection);
        d->SetVertexShader(nullptr);
        d->SetPixelShader(nullptr);
        d->SetFVF(0x142);
        d->SetTexture(0, emitter);
        d->SetRenderState(D3DRS_LIGHTING, FALSE);
        d->SetRenderState(D3DRS_ZENABLE, FALSE);
        d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        d->SetRenderState(D3DRS_FOGENABLE, TRUE);
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        d->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        d->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        d->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff203040, 1, 0);
        d->BeginScene();
        Settings s;
        s.sky = true;
        s.sunHalo = strength;
        if (replace)
            fx.sceneFX().beginFlare(s);
        Com<IDirect3DPixelShader9> activeFlare;
        d->GetPixelShader(activeFlare.out());
        struct V {
            float x, y, z;
            DWORD color;
            float u, v;
        };
        V q[] = {{-1, -1, 2, 0xffffffff, 0, 1},
                 {-1, 1, 2, 0xffffffff, 0, 0},
                 {1, -1, 2, 0xffffffff, 1, 1},
                 {1, 1, 2, 0xffffffff, 1, 0}};
        HRESULT flareResult = d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(V));
        fx.sceneFX().endSky();
        d->EndScene();
        DWORD fogState = 0;
        d->GetRenderState(D3DRS_FOGENABLE, &fogState);
        return fogState == TRUE && SUCCEEDED(flareResult);
    };
    CHECK(flareFrame(true, 1));
    auto softFlare = Read(d);
    CHECK(Save(softFlare, L"build/scene-soft-sun-flare.bmp"));
    CHECK(flareFrame(false, 1));
    CHECK(Save(Read(d), L"build/scene-native-sun-flare.bmp"));
    CHECK(Luma(softFlare[H / 2 * W + W / 2]) > Luma(0xff203040));
    CHECK((softFlare[(H / 2 + 150) * W + 160] & 0xffffff) == 0x203040);
    CHECK(flareFrame(true, 0));
    CHECK((Read(d)[H / 2 * W + W / 2] & 0xffffff) == 0x203040);
    // The real sun disc is deliberately distinct from the rainbow lens ghosts.
    DWORD sunDiscTag=128;
    emitter->SetPrivateData(EmitterTag,&sunDiscTag,4,0);
    CHECK(flareFrame(true,.3f));
    auto sunDisc=Read(d);
    CHECK(Luma(sunDisc[H/2*W+W/2])>Luma(softFlare[H/2*W+W/2])+50);
    CHECK((sunDisc[(H/2+150)*W+160]&0xffffff)==0x203040);
    CHECK(Save(sunDisc,L"build/scene-sun-disc.bmp"));
    CHECK(flareFrame(true,0));auto nativeDisc=Read(d);
    CHECK(flareFrame(false,0));CHECK(nativeDisc==Read(d));
    DWORD unknownTag = 4;
    emitter->SetPrivateData(EmitterTag, &unknownTag, 4, 0);
    CHECK(flareFrame(true, 1));
    auto unknownFlare = Read(d);
    CHECK(flareFrame(false, 1));
    CHECK(unknownFlare == Read(d));
    surfaceFixture=true; settings.surfaceReflections=true; settings.surfaceStrength=1;
    CHECK(render());
    CHECK(SceneFXFixture::surfaceMaskMaximum(fx.sceneFX(),d)>0);
    fx.invalidate();
    CHECK(fx.bytes() == 0);
    CHECK(SUCCEEDED(d->Reset(&pp)));
    settings.fog = true;
    CHECK(render());
    CHECK(SceneFXFixture::surfaceMaskMaximum(fx.sceneFX(),d)>0);
    fx.invalidate();
    DestroyWindow(window);
    puts("PASS: depth SSAO, fog, DoF, sun occlusion, native point-light receiver shading, GPU exposure, HUD, "
         "Reset.");
}
