#pragma once
#include "sfx.hpp"
#include <cstdio>

// A disposable material/motion fixture. No game process or configuration is modified.
inline LRESULT CALLBACK RefinementPreviewWindow(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_CLOSE) { DestroyWindow(window); return 0; }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, w, l);
}
inline int RefinementPreview(bool capture) {
    constexpr UINT width = 900, height = 600;
    WNDCLASSW wc{};
    wc.lpfnWndProc = RefinementPreviewWindow;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FENBRefinementPreview";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName,
        L"FENB refinement fixture | Previous wisps: left | Native buffered mist: right", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width + 16, height + 39, nullptr, nullptr, wc.hInstance, nullptr);
    if (!window) return 2;
    // Use the system renderer, independent of profiles beside the preview executable.
    wchar_t systemDirectory[MAX_PATH]{};
    if(!GetSystemDirectoryW(systemDirectory,MAX_PATH))return 3;
    const std::wstring systemD3D=std::wstring(systemDirectory)+L"\\d3d9.dll";
    HMODULE module = LoadLibraryExW(systemD3D.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    using Create = IDirect3D9 *(WINAPI *)(UINT);
    auto create = module ? reinterpret_cast<Create>(GetProcAddress(module, "Direct3DCreate9")) : nullptr;
    Com<IDirect3D9> root;
    root.p = create ? create(D3D_SDK_VERSION) : nullptr;
    if (!root.p) return 3;
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow = window;
    pp.BackBufferWidth = width; pp.BackBufferHeight = height; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D16;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    Com<IDirect3DDevice9> device;
    if (FAILED(root->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                  &pp, device.out()))) return 4;
    Com<IDirect3DTexture9> texture;
    // Original material is optional and read-only; clean checkouts use a small synthetic flame.
    if (FILE *file = _wfopen(L"X:/Games/Homesickness/SFX/Texture/sfx_EffectEnchant.dds", L"rb")) {
        DWORD header[32]{};
        if (fread(header, 1, 128, file) != 128) { fclose(file); return 5; }
        UINT w = header[4], h = header[3]; auto format = D3DFORMAT(header[21]);
        if (!w || !h || w > 4096 || h > 4096 ||
            (format != D3DFMT_DXT1 && format != D3DFMT_DXT3 && format != D3DFMT_DXT5)) {
            fclose(file); return 5;
        }
        UINT bytes = (w + 3) / 4 * (format == D3DFMT_DXT1 ? 8 : 16), rows = (h + 3) / 4;
        std::vector<BYTE> data(bytes * rows);
        bool read = fread(data.data(), 1, data.size(), file) == data.size(); fclose(file);
        if (!read || FAILED(device->CreateTexture(w, h, 1, 0, format, D3DPOOL_MANAGED,
                                                   texture.out(), nullptr))) return 5;
        D3DLOCKED_RECT lock{};
        if (FAILED(texture->LockRect(0, &lock, nullptr, 0))) return 5;
        for (UINT y = 0; y < rows; ++y)
            memcpy((BYTE *)lock.pBits + y * lock.Pitch, data.data() + y * bytes, bytes);
        texture->UnlockRect(0);
    } else {
        if (FAILED(device->CreateTexture(64, 64, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                          texture.out(), nullptr))) return 5;
        D3DLOCKED_RECT lock{};
        if (FAILED(texture->LockRect(0, &lock, nullptr, 0))) return 5;
        for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) {
            float u = (x - 31.5f) / 31.5f, v = y / 63.f;
            float density = std::exp(-u * u * 6) * std::sin(v * 3.14159265f);
            density *= .55f + .45f * std::sin(x * .6f + std::sin(y * .1f) * 2);
            unsigned c = unsigned(density * 255);
            ((DWORD *)((BYTE *)lock.pBits + y * lock.Pitch))[x] = 0xff000000 | (c << 8) | c;
        }
        texture->UnlockRect(0);
    }
    DWORD kind = 7; texture->SetPrivateData(SfxTag, &kind, 4, 0);
    SfxFX effects(device);
    Settings settings; settings.sfx = true; settings.sceneQuality = true;
    D3DMATRIX identity{}; identity._11 = identity._22 = identity._33 = identity._44 = 1;
    auto view = identity; view._43 = 4;
    D3DMATRIX projection{};
    projection._11 = 1.3f; projection._22 = 2; projection._33 = 100.f / 99.9f;
    projection._34 = 1; projection._43 = -.1f * projection._33;
    struct Vertex { float x, y, z; DWORD color; float u, v; };
    Com<IDirect3DVertexBuffer9> fan;
    if(FAILED(device->CreateVertexBuffer(80,D3DUSAGE_WRITEONLY,0x102,D3DPOOL_SYSTEMMEM,fan.out(),nullptr)))return 5;
    struct NativeVertex{float x,y,z,u,v;};
    NativeVertex nativeQuad[]={{-.5f,.5f,0,0,0},{.5f,.5f,0,1,0},{.5f,-.5f,0,1,1},{-.5f,-.5f,0,0,1}};
    void *upload=nullptr;if(FAILED(fan->Lock(0,0,&upload,0)))return 5;
    particle_buffer::Write mirror(fan);mirror.begin(fan,0,0,upload,0);memcpy(upload,nativeQuad,sizeof(nativeQuad));mirror.finish(fan);
    if(FAILED(fan->Unlock()))return 5;
    if(!capture)ShowWindow(window, SW_SHOW);
    const ULONGLONG start = GetTickCount64();
    unsigned frames = 0;
    while (IsWindow(window)) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) return 0;
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        if (!IsWindow(window)) break;
        float time = capture ? 1.f : float(GetTickCount64() - start) * .001f;
        effects.beginFrame(true);
        device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff24382e, 1, 0);
        device->SetVertexShader(nullptr); device->SetPixelShader(nullptr);
        device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        device->SetTransform(D3DTS_VIEW, &view); device->SetTransform(D3DTS_PROJECTION, &projection);
        device->SetTransform(D3DTS_WORLD, &identity); device->SetTransform(D3DTS_TEXTURE0, &identity);
        device->SetRenderState(D3DRS_ZENABLE, TRUE); device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        effects.observe();
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE); device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE); device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE); device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA); device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        device->SetTexture(0, texture);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->BeginScene();
        for (int side = 0; side < 2; ++side) {
          device->SetFVF(side?0x102:0x142);
          device->SetTextureStageState(0,D3DTSS_COLOROP,side?D3DTOP_SELECTARG1:D3DTOP_MODULATE);
          device->SetTextureStageState(0,D3DTSS_ALPHAARG2,side?D3DTA_TFACTOR:D3DTA_DIFFUSE);
          if(side)device->SetStreamSource(0,fan,0,20);
          for(int foot : {-1,1}) {
           native_refinement::Scope modelScope(&foot);
           for (int i = 9; i >= 0; --i) {
            float age = (i + std::fmod(time * 2, 1.f)) / 10.f;
            auto world = identity;
            if(side){world._11=.09f;world._22=.32f;}
            world._41 = (side ? 1.2f : -1.2f) + std::sin(age * 4 + time * .35f) * .16f + foot*.11f;
            world._42 = 1.35f - age * 2.8f;
            device->SetTransform(D3DTS_WORLD, &world);
            float stopped=capture?1.f:std::clamp((6-std::fmod(time,7.f))*.5f,0.f,1.f);
            DWORD color = (DWORD((1 - age) * 145*stopped) << 24) | 0x0099aabb;
            Vertex quad[] = {{-.045f,.16f,0,color,0,0},{.045f,.16f,0,color,1,0},
                             {-.045f,-.16f,0,color,0,1},{.045f,-.16f,0,color,1,1}};
            ParticleDraw geometry{D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex)};
            if(side){geometry={D3DPT_TRIANGLEFAN,2,nullptr,0,nullptr,D3DFMT_UNKNOWN,0,0,{D3DPT_TRIANGLEFAN,0,2}};device->SetRenderState(D3DRS_TEXTUREFACTOR,color);}
            bool styled = effects.begin(settings, false,true,side?geometry:ParticleDraw{});
            if(!effects.skipNativeDraw()){
                if(side)device->DrawPrimitive(D3DPT_TRIANGLEFAN,0,2);
                else device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex));
            }
            if (styled) effects.end(!side);
          }
          }
        }
        device->EndScene();
        if (capture && ++frames == 2) {
            Com<IDirect3DSurface9> target, read;
            device->GetRenderTarget(0, target.out());
            if (FAILED(device->CreateOffscreenPlainSurface(width, height, pp.BackBufferFormat,
                        D3DPOOL_SYSTEMMEM, read.out(), nullptr)) ||
                FAILED(device->GetRenderTargetData(target, read))) return 6;
            D3DLOCKED_RECT lock{};
            if (FAILED(read->LockRect(&lock, nullptr, D3DLOCK_READONLY))) return 6;
            FILE *file = _wfopen(L"build/refinement-preview.bmp", L"wb");
            if (!file) { read->UnlockRect(); return 6; }
            BITMAPFILEHEADER header{}; header.bfType = 0x4d42;
            header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER); header.bfSize = header.bfOffBits + width * height * 4;
            BITMAPINFOHEADER info{}; info.biSize = sizeof(info); info.biWidth = width; info.biHeight = -int(height);
            info.biPlanes = 1; info.biBitCount = 32;
            fwrite(&header, sizeof(header), 1, file); fwrite(&info, sizeof(info), 1, file);
            for (UINT y = 0; y < height; ++y) fwrite((BYTE *)lock.pBits + y * lock.Pitch, 4, width, file);
            fclose(file); read->UnlockRect(); DestroyWindow(window);
            return 0;
        }
        device->Present(nullptr, nullptr, nullptr, nullptr);
    }
    return 0;
}
