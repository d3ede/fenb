#pragma once
#include "common.hpp"

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
