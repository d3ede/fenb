#pragma once
#include "runtime.hpp"

void WrapObject(REFIID iid, void** object, IUnknown* owner = nullptr);
IUnknown* UnwrapUnknown(IUnknown* object);
template<class T> T* Unwrap(T* p) { return static_cast<T*>(UnwrapUnknown(p)); }
template<class T> void Adopt(T** p, IUnknown* owner) {
    if (p && *p) WrapObject(__uuidof(T), reinterpret_cast<void**>(p), owner);
}
Runtime* RuntimeFor(IUnknown* wrapper);

class ProxyLife {
public:
    ProxyLife(IUnknown* real, IUnknown* owner);
    virtual ~ProxyLife();
    ULONG addRef() { return ++refs_; }
    ULONG release();
    HRESULT query(REFIID iid, void** out);
    void registerSelf(IUnknown* self);
    IUnknown* realBase_;
    IUnknown* owner_;
    IUnknown* self_ = nullptr;
    std::unique_ptr<Runtime> runtime_;
private:
    std::atomic<ULONG> refs_{1};
};

