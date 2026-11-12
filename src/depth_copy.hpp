#pragma once
#include "state_guard.hpp"

// Optional NVIDIA driver path. Function IDs/signatures are from NVIDIA's public NVAPI SDK:
// https://github.com/NVIDIA/nvapi/blob/main/nvapi_interface.h
// https://docs.nvidia.com/nvapi/group__dx.html
// No NVAPI redistributable, executable patch, CPU depth readback or replacement game depth surface.
class DepthCopy {
    using Resource = int(__cdecl *)(IDirect3DResource9 *);
    using Copy = int(__cdecl *)(IDirect3DDevice9 *, IDirect3DResource9 *, const RECT *, IDirect3DResource9 *,
                                const RECT *, D3DTEXTUREFILTERTYPE);
    // RegisterResource is not reference-counted by the driver. Two independent snapshots may
    // share the same game depth surface, so registration and API lifetime need shared ownership.
    struct Backend {
        HMODULE module = nullptr;
        Resource registerResource = nullptr, unregisterResource = nullptr;
        Copy copy = nullptr;
        int(__cdecl *unload)() = nullptr;
        bool initialized = false;
        std::mutex mutex;
        std::unordered_map<IDirect3DResource9 *, unsigned> references;
        bool initialize() {
            module = LoadLibraryExW(L"nvapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!module)
                return false;
            auto query =
                reinterpret_cast<void *(__cdecl *)(unsigned)>(GetProcAddress(module, "nvapi_QueryInterface"));
            if (!query)
                return false;
            auto init = reinterpret_cast<int(__cdecl *)()>(query(0x0150e828));
            unload = reinterpret_cast<int(__cdecl *)()>(query(0xd22bdd7e));
            registerResource = reinterpret_cast<Resource>(query(0xa064bdfc));
            unregisterResource = reinterpret_cast<Resource>(query(0xbb2b17aa));
            copy = reinterpret_cast<Copy>(query(0x22de03aa));
            initialized = init && unload && registerResource && unregisterResource && copy && init() == 0;
            return initialized;
        }
        bool acquire(IDirect3DResource9 *resource) {
            std::lock_guard lock(mutex);
            auto it = references.find(resource);
            if (it != references.end()) {
                ++it->second;
                return true;
            }
            if (registerResource(resource) != 0)
                return false;
            references.emplace(resource, 1);
            return true;
        }
        void release(IDirect3DResource9 *resource) {
            std::lock_guard lock(mutex);
            auto it = references.find(resource);
            if (it == references.end())
                return;
            if (--it->second == 0) {
                unregisterResource(resource);
                references.erase(it);
            }
        }
        ~Backend() {
            if (initialized)
                unload();
            if (module)
                FreeLibrary(module);
        }
        static std::shared_ptr<Backend> get() {
            static std::mutex gate;
            static std::weak_ptr<Backend> shared;
            std::lock_guard lock(gate);
            auto backend = shared.lock();
            if (!backend) {
                backend = std::make_shared<Backend>();
                if (!backend->initialize())
                    return {};
                shared = backend;
            }
            return backend;
        }
    };
    IDirect3DDevice9 *device_;
    std::shared_ptr<Backend> backend_;
    bool tried_ = false, attempted_ = false, ready_ = false;
    bool sourceRegistered_ = false, destinationRegistered_ = false;
    bool disabled_ = false;
    Com<IDirect3DSurface9> source_;
    Com<IDirect3DTexture9> texture_;
    UINT width_ = 0, height_ = 0;
    unsigned bits_ = 16;
    bool driver() {
        if (!tried_) {
            tried_ = true;
            backend_ = Backend::get();
        }
        return backend_ != nullptr;
    }

  public:
    explicit DepthCopy(IDirect3DDevice9 *device) : device_(device) {}
    ~DepthCopy() {
        invalidate();
    }
    void invalidate() {
        disabled_ = false;
        if (sourceRegistered_)
            backend_->release(source_);
        if (destinationRegistered_)
            backend_->release(texture_);
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
    bool capture(UINT width, UINT height, IDirect3DSurface9 *worldSource = nullptr) {
        if (disabled_)
            return false;
        if (attempted_)
            return ready_;
        attempted_ = true;
        if (!driver())
            return false;
        Com<IDirect3DSurface9> source;
        if(worldSource) { source.p=worldSource; source->AddRef(); }
        D3DSURFACE_DESC desc{};
        if ((!source.p && FAILED(device_->GetDepthStencilSurface(source.out()))) || FAILED(source->GetDesc(&desc)) ||
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
            sourceRegistered_ = backend_->acquire(source_);
            destinationRegistered_ = backend_->acquire(texture_);
            if (!sourceRegistered_ || !destinationRegistered_) {
                invalidate();
                attempted_ = true;
                disabled_ = true;
                return false;
            }
            width_ = width;
            height_ = height;
            bits_ = desc.Format == D3DFMT_D16 ? 16 : 24;
            Log(bits_ == 24 ? "Water depth: NVIDIA copy uses a 24-bit source."
                            : "Water depth: NVIDIA copy uses a 16-bit source.");
        }
        // Driver copies can have implementation-specific state effects. Restore immediately.
        StateGuard state(device_);
        if (!state.valid())
            return false;
        if (worldSource && FAILED(device_->SetDepthStencilSurface(source_)))
            return false;
        ready_ = backend_->copy(device_, source_, nullptr, texture_, nullptr, D3DTEXF_NONE) == 0;
        return ready_;
    }
    IDirect3DTexture9 *texture() const {
        return ready_ ? texture_.p : nullptr;
    }
    size_t bytes() const {
        return texture_.p ? size_t(width_) * height_ * 4 : 0;
    }
    float quantum() const {
        return bits_ == 24 ? 1.f / 16777215.f : 1.f / 65535.f;
    }
};
