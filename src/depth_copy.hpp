#pragma once
#include "state_guard.hpp"

// Optional NVIDIA driver path. Function IDs/signatures are from NVIDIA's public NVAPI SDK:
// https://github.com/NVIDIA/nvapi/blob/main/nvapi_interface.h
// https://docs.nvidia.com/nvapi/group__dx.html
// No NVAPI redistributable, executable patch, CPU depth readback or replacement game depth surface.
class DepthCopy {
    IDirect3DDevice9 *device_;
    HMODULE module_ = nullptr;
    using Resource = int(__cdecl *)(IDirect3DResource9 *);
    using Copy = int(__cdecl *)(IDirect3DDevice9 *, IDirect3DResource9 *, const RECT *, IDirect3DResource9 *,
                                const RECT *, D3DTEXTUREFILTERTYPE);
    Resource register_ = nullptr, unregister_ = nullptr;
    Copy copy_ = nullptr;
    int(__cdecl *unload_)() = nullptr;
    bool initialized_ = false, tried_ = false, attempted_ = false, ready_ = false;
    bool sourceRegistered_ = false, destinationRegistered_ = false;
    bool disabled_ = false;
    Com<IDirect3DSurface9> source_;
    Com<IDirect3DTexture9> texture_;
    UINT width_ = 0, height_ = 0;
    bool driver() {
        if (tried_)
            return initialized_;
        tried_ = true;
        module_ = LoadLibraryExW(L"nvapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module_)
            return false;
        auto query =
            reinterpret_cast<void *(__cdecl *)(unsigned)>(GetProcAddress(module_, "nvapi_QueryInterface"));
        if (!query)
            return false;
        auto initialize = reinterpret_cast<int(__cdecl *)()>(query(0x0150e828));
        unload_ = reinterpret_cast<int(__cdecl *)()>(query(0xd22bdd7e));
        register_ = reinterpret_cast<Resource>(query(0xa064bdfc));
        unregister_ = reinterpret_cast<Resource>(query(0xbb2b17aa));
        copy_ = reinterpret_cast<Copy>(query(0x22de03aa));
        initialized_ = initialize && unload_ && register_ && unregister_ && copy_ && initialize() == 0;
        return initialized_;
    }

  public:
    explicit DepthCopy(IDirect3DDevice9 *device) : device_(device) {}
    ~DepthCopy() {
        invalidate();
        if (initialized_)
            unload_();
        if (module_)
            FreeLibrary(module_);
    }
    void invalidate() {
        disabled_ = false;
        if (sourceRegistered_)
            unregister_(source_);
        if (destinationRegistered_)
            unregister_(texture_);
        sourceRegistered_ = destinationRegistered_ = false;
        source_.out();
        texture_.out();
        width_ = height_ = 0;
        attempted_ = ready_ = false;
    }
    void beginFrame(bool enabled) {
        attempted_ = ready_ = false;
        if (!enabled)
            invalidate();
    }
    bool capture(UINT width, UINT height) {
        if (disabled_)
            return false;
        if (attempted_)
            return ready_;
        attempted_ = true;
        if (!driver())
            return false;
        Com<IDirect3DSurface9> source;
        D3DSURFACE_DESC desc{};
        if (FAILED(device_->GetDepthStencilSurface(source.out())) || FAILED(source->GetDesc(&desc)) ||
            desc.Width != width || desc.Height != height ||
            (desc.Format != D3DFMT_D16 && desc.Format != D3DFMT_D24X8 && desc.Format != D3DFMT_D24S8))
            return false;
        if (source_.p != source.p || width != width_ || height != height_) {
            invalidate();
            attempted_ = true;
            if (FAILED(device_->CreateTexture(width, height, 1, D3DUSAGE_DEPTHSTENCIL,
                                              D3DFORMAT(MAKEFOURCC('I', 'N', 'T', 'Z')), D3DPOOL_DEFAULT,
                                              texture_.out(), nullptr))) {
                disabled_ = true;
                return false;
            }
            source_.p = source.p;
            source_->AddRef();
            sourceRegistered_ = register_(source_) == 0;
            destinationRegistered_ = register_(texture_) == 0;
            if (!sourceRegistered_ || !destinationRegistered_) {
                invalidate();
                attempted_ = true;
                disabled_ = true;
                return false;
            }
            width_ = width;
            height_ = height;
        }
        // Driver copies can have implementation-specific state effects. Restore immediately.
        StateGuard state(device_);
        if (!state.valid())
            return false;
        ready_ = copy_(device_, source_, nullptr, texture_, nullptr, D3DTEXF_NONE) == 0;
        return ready_;
    }
    IDirect3DTexture9 *texture() const {
        return ready_ ? texture_.p : nullptr;
    }
    size_t bytes() const {
        return texture_.p ? size_t(width_) * height_ * 4 : 0;
    }
};
