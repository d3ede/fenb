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
inline bool SupportedPresentationHost() {
    static const bool known = [] {
        wchar_t path[32768]{};
        GetModuleFileNameW(nullptr, path, 32768);
        const auto *name = wcsrchr(path, L'\\');
        if (!_wcsicmp(name ? name + 1 : path, L"FENB-preview.exe"))
            return true;
        return client_detail::Current().frames();
    }();
    return known;
}
inline bool ConfiguredPreciseDepth() {
    if (!SupportedPresentationHost())
        return false;
    Settings settings;
    settings.load(ModuleDirectory() + L"FENB.ini");
    return settings.preciseWaterDepth;
}
template <class Call>
HRESULT WithMultisample(IDirect3D9 *root, UINT adapter, D3DDEVTYPE type, D3DPRESENT_PARAMETERS *p,
                        int samples, Call &&call) {
    if (!p || samples == 0 || (samples != -1 && samples != 2 && samples != 4 && samples != 8))
        return call(p);
    auto adjusted = *p;
    adjusted.MultiSampleType = samples < 0 ? D3DMULTISAMPLE_NONE : D3DMULTISAMPLE_TYPE(samples);
    adjusted.MultiSampleQuality = 0;
    if (adjusted.MultiSampleType == p->MultiSampleType && p->MultiSampleQuality == 0)
        return call(p);
    if (adjusted.MultiSampleType != D3DMULTISAMPLE_NONE) {
        if (p->SwapEffect != D3DSWAPEFFECT_DISCARD)
            return call(p);
        D3DDISPLAYMODE display{};
        if (FAILED(root->GetAdapterDisplayMode(adapter, &display)))
            return call(p);
        const D3DFORMAT color = p->BackBufferFormat == D3DFMT_UNKNOWN ? display.Format : p->BackBufferFormat;
        for (auto format : {color, p->EnableAutoDepthStencil ? p->AutoDepthStencilFormat : color}) {
            DWORD quality = 0;
            if (FAILED(root->CheckDeviceMultiSampleType(adapter, type, format, p->Windowed,
                                                        adjusted.MultiSampleType, &quality)) ||
                !quality)
                return call(p);
        }
    }
    HRESULT result = call(&adjusted);
    if (SUCCEEDED(result)) {
        *p = adjusted;
        Log("Presentation: requested MSAA mode applied.");
        return result;
    }
    if (result == D3DERR_DEVICELOST || result == D3DERR_DEVICEHUNG || result == D3DERR_DEVICEREMOVED)
        return result;
    Log("Presentation: MSAA override failed; retrying original game parameters.");
    return call(p);
}
template <class Call>
HRESULT WithConfiguredDepth(IDirect3D9 *root, UINT adapter, D3DDEVTYPE type, D3DPRESENT_PARAMETERS *p,
                            Call &&call) {
    if (!SupportedPresentationHost())
        return call(p);
    Settings settings;
    settings.load(ModuleDirectory() + L"FENB.ini");
    return WithMultisample(root, adapter, type, p, settings.msaaSamples, [&](auto *parameters) {
        return WithDepthFormat(root, adapter, type, parameters, settings.preciseWaterDepth, call);
    });
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

template <class Call>
HRESULT WithPresentationSettings(IDirect3DDevice9 *device, D3DPRESENT_PARAMETERS *p,
                                 const Settings &settings, Call &&call) {
    if (!SupportedPresentationHost()) return call(p);
    Com<IDirect3D9> root;
    D3DDEVICE_CREATION_PARAMETERS creation{};
    if (FAILED(device->GetDirect3D(root.out())) || FAILED(device->GetCreationParameters(&creation)))
        return call(p);
    return WithMultisample(root, creation.AdapterOrdinal, creation.DeviceType, p, settings.msaaSamples,
                          [&](auto *adjusted) {
        return WithDepthFormat(root, creation.AdapterOrdinal, creation.DeviceType, adjusted,
                               settings.preciseWaterDepth, call);
    });
}
