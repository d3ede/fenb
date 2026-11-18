#pragma once
#include "common.hpp"

// GPU-backed draw arguments may be retained for a bounded material-mask batch.
// UP draws deliberately use the immediate path: their CPU pointers are borrowed.
struct SurfaceDraw {
    D3DPRIMITIVETYPE type = D3DPT_TRIANGLELIST;
    UINT first = 0, primitives = 0, minVertex = 0, vertices = 0;
    INT baseVertex = 0;
    bool indexed = false;
};

// Borrowed UP vertices/indices, valid only during the intercepted native draw.
struct ParticleDraw {
    D3DPRIMITIVETYPE type = D3DPT_TRIANGLELIST;
    UINT primitives = 0;
    const void *data = nullptr;
    UINT stride = 0;
    const void *indices = nullptr;
    D3DFORMAT indexFormat = D3DFMT_UNKNOWN;
    UINT minVertex = 0, vertices = 0;
};

// Non-owning synchronous callback. Native draw arguments remain live for this whole call;
// retaining this object is forbidden. Even large indexed-draw lambdas require no heap allocation.
class DrawCall {
    const void *context_;
    HRESULT (*invoke_)(const void *);

  public:
    template <class F>
    DrawCall(const F &fn)
        : context_(&fn), invoke_([](const void *p) -> HRESULT { return (*static_cast<const F *>(p))(); }) {}
    HRESULT operator()() const {
        return invoke_(context_);
    }
};
