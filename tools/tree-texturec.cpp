// Offline authoring tool only. Production uses embedded DDS blocks without D3DX.
#include "common.hpp"
#include <cstdio>
int wmain(int argc, wchar_t **argv) {
    if (argc != 4) return 1;
    HMODULE library = LoadLibraryExW(L"d3dx9_43.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library) return 2;
    using Load = HRESULT(WINAPI *)(IDirect3DDevice9 *, LPCWSTR, UINT, UINT, UINT, DWORD, D3DFORMAT,
                                  D3DPOOL, DWORD, DWORD, D3DCOLOR, void *, PALETTEENTRY *, IDirect3DTexture9 **);
    using Save = HRESULT(WINAPI *)(LPCWSTR, int, IDirect3DBaseTexture9 *, const PALETTEENTRY *);
    auto load = reinterpret_cast<Load>(GetProcAddress(library, "D3DXCreateTextureFromFileExW"));
    auto save = reinterpret_cast<Save>(GetProcAddress(library, "D3DXSaveTextureToFileW"));
    if (!load || !save) return 3;
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FENBTreeTextureCompiler";
    RegisterClassW(&wc);
    HWND window=CreateWindowW(wc.lpszClassName,L"Offline tree texture compiler",WS_OVERLAPPEDWINDOW,
                             0,0,32,32,nullptr,nullptr,wc.hInstance,nullptr);
    Com<IDirect3D9> root;root.p=Direct3DCreate9(D3D_SDK_VERSION);
    if (!root.p) return 4;
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow=window;pp.BackBufferWidth=pp.BackBufferHeight=32;
    Com<IDirect3DDevice9> device;
    if (FAILED(root->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,device.out()))) return 5;
    Com<IDirect3DTexture9> texture;
    D3DFORMAT format=!wcscmp(argv[3],L"alpha")?D3DFMT_DXT5:D3DFMT_DXT1;
    if (FAILED(load(device,argv[1],UINT(-1),UINT(-1),0,0,format,D3DPOOL_MANAGED,3,5,0,
                    nullptr,nullptr,texture.out()))) return 6;
    if (FAILED(save(argv[2],4,texture,nullptr))) return 7;
    return 0;
}
