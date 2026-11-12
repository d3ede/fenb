#pragma once
#include "settings.hpp"
#include "fps_limiter.hpp"
#include <utility>

// Upgrade only an automatically allocated D16 buffer, retaining color, size and MSAA quality.
inline D3DFORMAT PreciseDepthFormat(IDirect3D9 *root, UINT adapter, D3DDEVTYPE type,
                                    const D3DPRESENT_PARAMETERS &p) {
    if (!p.EnableAutoDepthStencil || p.AutoDepthStencilFormat != D3DFMT_D16)
        return p.AutoDepthStencilFormat;
    D3DDISPLAYMODE mode{};
    if (FAILED(root->GetAdapterDisplayMode(adapter, &mode)))
        return p.AutoDepthStencilFormat;
    D3DFORMAT color = p.BackBufferFormat == D3DFMT_UNKNOWN ? mode.Format : p.BackBufferFormat;
    D3DFORMAT display = p.Windowed ? mode.Format : color;
    for (auto depth : {D3DFMT_D24X8, D3DFMT_D24S8}) {
        DWORD qualities = 0;
        if (SUCCEEDED(root->CheckDeviceFormat(adapter, type, display, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE,
                                              depth)) &&
            SUCCEEDED(root->CheckDepthStencilMatch(adapter, type, display, color, depth)) &&
            SUCCEEDED(root->CheckDeviceMultiSampleType(adapter, type, depth, p.Windowed, p.MultiSampleType,
                                                       &qualities)) &&
            p.MultiSampleQuality < qualities)
            return depth;
    }
    return p.AutoDepthStencilFormat;
}
template <class Call>
HRESULT WithDepthFormat(IDirect3D9 *root, UINT adapter, D3DDEVTYPE type, D3DPRESENT_PARAMETERS *p,
                        bool enabled, Call &&call) {
    if (!p || !enabled)
        return call(p);
    auto precise = *p;
    precise.AutoDepthStencilFormat = PreciseDepthFormat(root, adapter, type, *p);
    if (precise.AutoDepthStencilFormat == p->AutoDepthStencilFormat)
        return call(p);
    HRESULT result = call(&precise);
    if (SUCCEEDED(result)) {
        *p = precise;
        Log("Water depth: upgraded D16 to 24-bit depth; MSAA retained.");
        return result;
    }
    if (result == D3DERR_DEVICELOST || result == D3DERR_DEVICEHUNG || result == D3DERR_DEVICEREMOVED)
        return result;
    Log("Water depth: 24-bit allocation failed; retrying original D16 parameters.");
    return call(p);
}
inline bool ConfiguredPreciseDepth() {
    static const bool known = [] {
        wchar_t path[32768]{};
        GetModuleFileNameW(nullptr, path, 32768);
        return fps_detail::KnownClientFile(path);
    }();
    if (!known)
        return false;
    Settings settings;
    settings.load(ModuleDirectory() + L"FENB.ini");
    return settings.preciseWaterDepth;
}
template <class Call>
HRESULT WithConfiguredDepth(IDirect3D9 *root, UINT adapter, D3DDEVTYPE type, D3DPRESENT_PARAMETERS *p,
                            Call &&call) {
    return WithDepthFormat(root, adapter, type, p, ConfiguredPreciseDepth(), std::forward<Call>(call));
}
template <class Call>
HRESULT WithConfiguredDepth(IDirect3DDevice9 *device, D3DPRESENT_PARAMETERS *p, Call &&call) {
    Com<IDirect3D9> root;
    D3DDEVICE_CREATION_PARAMETERS creation{};
    if (FAILED(device->GetDirect3D(root.out())) || FAILED(device->GetCreationParameters(&creation)))
        return call(p);
    return WithConfiguredDepth(root, creation.AdapterOrdinal, creation.DeviceType, p,
                               std::forward<Call>(call));
}
