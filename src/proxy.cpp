#include "proxy.hpp"
#include "resource_lookup_cache.hpp"

namespace {
std::recursive_mutex registryMutex;
std::unordered_map<IUnknown*, ProxyLife*> byReal;
std::unordered_map<IUnknown*, ProxyLife*> byWrapper;
std::atomic<uint64_t> registryGeneration{1};
thread_local ResourceLookupCache<IUnknown*> resourceCache;
}
ProxyLife* MakeProxy(REFIID, void*, IUnknown*);

ProxyLife::ProxyLife(IUnknown* real, IUnknown* owner) : realBase_(real), owner_(owner) {
    if (owner_) owner_->AddRef();
}
void ProxyLife::registerSelf(IUnknown* self) {
    std::lock_guard lock(registryMutex);
    self_ = self; byReal[realBase_] = this; byWrapper[self] = this;
    registryGeneration.fetch_add(1,std::memory_order_release);
}
ProxyLife::~ProxyLife() {
    // Renderer holds native COM resources. Tear it down BEFORE releasing the native device.
    runtime_.reset();
    realBase_->Release();
    if (owner_) owner_->Release();
}
ULONG ProxyLife::release() {
    std::unique_lock lock(registryMutex);
    auto remaining = --refs_;
    if (remaining) return remaining;
    byReal.erase(realBase_); byWrapper.erase(self_);
    registryGeneration.fetch_add(1,std::memory_order_release);
    lock.unlock();
    delete this;
    return 0;
}
HRESULT ProxyLife::query(REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    HRESULT hr = realBase_->QueryInterface(iid, out);
    if (SUCCEEDED(hr)) WrapObject(iid, out, owner_);
    return hr;
}
IUnknown* UnwrapUnknown(IUnknown* object) {
    if (!object) return nullptr;
    IUnknown *real=nullptr;
    if(resourceCache.find(object,registryGeneration.load(std::memory_order_acquire),real))return real;
    std::lock_guard lock(registryMutex);
    auto it = byWrapper.find(object);
    real=it == byWrapper.end() ? object : it->second->realBase_;
    resourceCache.remember(object,registryGeneration.load(std::memory_order_relaxed),real);
    return real;
}
Runtime* RuntimeFor(IUnknown* wrapper) {
    std::lock_guard lock(registryMutex);
    auto it = byWrapper.find(wrapper);
    return it == byWrapper.end() ? nullptr : it->second->runtime_.get();
}
void WrapObject(REFIID iid, void** object, IUnknown* owner) {
    if (!object || !*object) return;
    std::lock_guard lock(registryMutex);
    auto* raw = static_cast<IUnknown*>(*object);
    auto existing = byReal.find(raw);
    if (existing != byReal.end()) {
        existing->second->addRef(); raw->Release(); *object = existing->second->self_; return;
    }
    // Prefer the most-derived standard interface so QueryInterface preserves identity.
    auto tryType = [&](REFIID candidate) {
        void* derived = nullptr;
        if (FAILED(raw->QueryInterface(candidate, &derived))) return false;
        auto it = byReal.find(static_cast<IUnknown*>(derived));
        if (it != byReal.end()) {
            it->second->addRef(); static_cast<IUnknown*>(derived)->Release();
            raw->Release(); *object = it->second->self_; return true;
        }
        auto* proxy = MakeProxy(candidate, derived, owner);
        if (!proxy) { static_cast<IUnknown*>(derived)->Release(); return false; }
        raw->Release(); *object = proxy->self_; return true;
    };
    if (iid == IID_IUnknown) {
        // Unknown can be returned by resource GetContainer; identify its standard family.
        const IID* types[] = {&IID_IDirect3DDevice9Ex, &IID_IDirect3DDevice9,
            &IID_IDirect3D9Ex, &IID_IDirect3D9, &IID_IDirect3DSwapChain9Ex,
            &IID_IDirect3DSwapChain9, &IID_IDirect3DTexture9, &IID_IDirect3DCubeTexture9,
            &IID_IDirect3DVolumeTexture9, &IID_IDirect3DSurface9, &IID_IDirect3DVolume9,
            &IID_IDirect3DVertexBuffer9, &IID_IDirect3DIndexBuffer9};
        for (auto type : types) if (tryType(*type)) return;
    }
    if (iid == IID_IDirect3D9 && tryType(IID_IDirect3D9Ex)) return;
    if (iid == IID_IDirect3DDevice9 && tryType(IID_IDirect3DDevice9Ex)) return;
    if (iid == IID_IDirect3DSwapChain9 && tryType(IID_IDirect3DSwapChain9Ex)) return;
    if (iid == IID_IDirect3DBaseTexture9 || iid == IID_IDirect3DResource9) {
        const IID* types[] = {&IID_IDirect3DTexture9, &IID_IDirect3DCubeTexture9,
            &IID_IDirect3DVolumeTexture9, &IID_IDirect3DSurface9,
            &IID_IDirect3DVertexBuffer9, &IID_IDirect3DIndexBuffer9};
        for (auto type : types) if (tryType(*type)) return;
    }
    auto* proxy = MakeProxy(iid, *object, owner);
    if (proxy) *object = proxy->self_;
}

