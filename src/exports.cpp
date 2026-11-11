#include "exports.hpp"
#include "proxy.hpp"
#include <cstdio>

namespace { HMODULE selfModule = nullptr, systemModule = nullptr; INIT_ONCE loadOnce=INIT_ONCE_STATIC_INIT; }

std::wstring ModuleDirectory() {
    wchar_t path[32768]{};
    GetModuleFileNameW(selfModule, path, 32768);
    std::wstring result(path);
    return result.substr(0, result.find_last_of(L"\\/") + 1);
}
void Log(const char* text) {
    auto path = ModuleDirectory() + L"FENB.log";
    FILE* f = _wfopen(path.c_str(), L"ab");
    if (!f) return;
    fprintf(f, "%s\n", text); fclose(f);
}

static BOOL CALLBACK LoadSystem(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t path[MAX_PATH]{};
    if (!GetSystemDirectoryW(path,MAX_PATH-12)) return FALSE;
    wcscat_s(path,L"\\d3d9.dll");
    systemModule=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!systemModule) return FALSE;
    BindForwarders(systemModule); return TRUE;
}
extern "C" void EnsureSystem() {
    if (!InitOnceExecuteOnce(&loadOnce,LoadSystem,nullptr,nullptr))
        TerminateProcess(GetCurrentProcess(),ERROR_MOD_NOT_FOUND);
}
// No library loading, rendering, allocation or COM work under the loader lock.
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        selfModule = module;
        DisableThreadLibraryCalls(module);
    }
    // Do not call COM, GPU or UI teardown under the loader lock.
    // The system runtime intentionally remains loaded for this proxy's process lifetime.
    return TRUE;
}

extern "C" IDirect3D9* WINAPI FENB_Direct3DCreate9(UINT version) {
    EnsureSystem();
    using Fn = IDirect3D9* (WINAPI*)(UINT);
    auto fn = reinterpret_cast<Fn>(GetProcAddress(systemModule, "Direct3DCreate9"));
    IDirect3D9* result = fn ? fn(version) : nullptr;
    Adopt(&result, nullptr); return result;
}
extern "C" HRESULT WINAPI FENB_Direct3DCreate9Ex(UINT version, IDirect3D9Ex** result) {
    EnsureSystem();
    using Fn = HRESULT (WINAPI*)(UINT, IDirect3D9Ex**);
    auto fn = reinterpret_cast<Fn>(GetProcAddress(systemModule, "Direct3DCreate9Ex"));
    if (!fn) { if (result) *result = nullptr; return D3DERR_NOTAVAILABLE; }
    auto hr = fn(version, result); if (SUCCEEDED(hr)) Adopt(result, nullptr); return hr;
}
extern "C" IDirect3D9* WINAPI FENB_Direct3DCreate9On12(UINT version, void* overrides, UINT count) {
    EnsureSystem();
    using Fn = IDirect3D9* (WINAPI*)(UINT, void*, UINT);
    auto fn = reinterpret_cast<Fn>(GetProcAddress(systemModule, "Direct3DCreate9On12"));
    auto* result = fn ? fn(version, overrides, count) : nullptr;
    Adopt(&result, nullptr); return result;
}
extern "C" HRESULT WINAPI FENB_Direct3DCreate9On12Ex(UINT version, void* overrides, UINT count, IDirect3D9Ex** result) {
    EnsureSystem();
    using Fn = HRESULT (WINAPI*)(UINT, void*, UINT, IDirect3D9Ex**);
    auto fn = reinterpret_cast<Fn>(GetProcAddress(systemModule, "Direct3DCreate9On12Ex"));
    if (!fn) { if (result) *result = nullptr; return D3DERR_NOTAVAILABLE; }
    auto hr = fn(version, overrides, count, result); if (SUCCEEDED(hr)) Adopt(result, nullptr); return hr;
}
