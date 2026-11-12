// Offline catalog helper. D3DX is loaded only by this tool, never by the production proxy.
#include "common.hpp"
#include <bcrypt.h>
#include <cstdio>
int wmain(int argc, wchar_t **argv) {
    HMODULE module = LoadLibraryExW(L"d3dx9_43.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module)
        return 2;
    using Load = HRESULT(WINAPI *)(IDirect3DDevice9 *, LPCWSTR, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
                                   DWORD, DWORD, D3DCOLOR, void *, PALETTEENTRY *, IDirect3DTexture9 **);
    auto load = reinterpret_cast<Load>(GetProcAddress(module, "D3DXCreateTextureFromFileExW"));
    if (!load)
        return 3;
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FENBTextureFingerprint";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"Offline texture fingerprint", WS_OVERLAPPEDWINDOW, 0, 0,
                                32, 32, nullptr, nullptr, wc.hInstance, nullptr);
    Com<IDirect3D9> root;
    root.p = Direct3DCreate9(D3D_SDK_VERSION);
    if (!root.p)
        return 4;
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window;
    pp.BackBufferWidth = pp.BackBufferHeight = 32;
    Com<IDirect3DDevice9> device;
    if (FAILED(root->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp,
                                  device.out())))
        return 5;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return 6;
    puts("[");
    bool first = true;
    unsigned failed = 0;
    for (int i = 1; i < argc; ++i) {
        Com<IDirect3DTexture9> texture;
        HRESULT result = load(device, argv[i], UINT(-1), UINT(-1), 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, 1,
                              1, 0, nullptr, nullptr, texture.out());
        if (FAILED(result)) {
            ++failed;
            continue;
        }
        D3DSURFACE_DESC d{};
        texture->GetLevelDesc(0, &d);
        D3DLOCKED_RECT lock{};
        if (FAILED(texture->LockRect(0, &lock, nullptr, D3DLOCK_READONLY))) {
            ++failed;
            continue;
        }
        BCRYPT_HASH_HANDLE hash = nullptr;
        bool ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
        for (UINT y = 0; ok && y < d.Height; ++y)
            ok = BCryptHashData(hash, (PUCHAR)lock.pBits + y * lock.Pitch, d.Width * 4, 0) >= 0;
        unsigned char digest[32]{};
        ok = ok && BCryptFinishHash(hash, digest, 32, 0) >= 0;
        if (hash)
            BCryptDestroyHash(hash);
        texture->UnlockRect(0);
        if (!ok) {
            ++failed;
            continue;
        }
        char path[32768]{};
        WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, path, sizeof(path), nullptr, nullptr);
        for (auto *p = path; *p; ++p)
            if (*p == '\\')
                *p = '/';
        printf("%s{\"file\":\"%s\",\"width\":%u,\"height\":%u,\"format\":21,\"sha256\":\"",
               first ? "" : ",\n", path, d.Width, d.Height);
        for (auto v : digest)
            printf("%02x", v);
        printf("\"}");
        first = false;
    }
    puts("\n]");
    BCryptCloseAlgorithmProvider(algorithm, 0);
    DestroyWindow(window);
    FreeLibrary(module);
    return failed ? 7 : 0;
}
