#pragma once
#include "common.hpp"

class StateGuard {
    IDirect3DDevice9* device_;
    Com<IDirect3DStateBlock9> block_;
    IDirect3DSurface9* targets_[4]{};
    IDirect3DSurface9* depth_ = nullptr;
    D3DVIEWPORT9 viewport_{};
    RECT scissor_{};
    D3DMATRIX transforms_[3]{};
    bool transformsValid_[3]{};
    UINT targetCount_ = 1;
    bool valid_ = false;
public:
    explicit StateGuard(IDirect3DDevice9* device) : device_(device) {
        D3DCAPS9 caps{};
        if (FAILED(device->GetDeviceCaps(&caps))) return;
        targetCount_ = std::clamp<UINT>(caps.NumSimultaneousRTs, 1, 4);
        if (FAILED(device->CreateStateBlock(D3DSBT_ALL, block_.out()))) return;
        if (FAILED(block_->Capture())) return;
        if (FAILED(device->GetViewport(&viewport_))) return;
        device->GetScissorRect(&scissor_);
        for (UINT i = 0; i < targetCount_; ++i) device->GetRenderTarget(i, &targets_[i]);
        device->GetDepthStencilSurface(&depth_);
        D3DTRANSFORMSTATETYPE types[] = {D3DTS_WORLD, D3DTS_VIEW, D3DTS_PROJECTION};
        for (int i=0; i<3; ++i) transformsValid_[i] = SUCCEEDED(device->GetTransform(types[i], &transforms_[i]));
        valid_ = targets_[0] != nullptr;
    }
    ~StateGuard() {
        if (valid_) {
            for (UINT i=0; i<targetCount_; ++i) device_->SetRenderTarget(i, targets_[i]);
            device_->SetDepthStencilSurface(depth_);
            block_->Apply();
            D3DTRANSFORMSTATETYPE types[] = {D3DTS_WORLD, D3DTS_VIEW, D3DTS_PROJECTION};
            for (int i=0; i<3; ++i) if (transformsValid_[i]) device_->SetTransform(types[i], &transforms_[i]);
            device_->SetViewport(&viewport_); device_->SetScissorRect(&scissor_);
        }
        for (auto*& target : targets_) Drop(target);
        Drop(depth_);
    }
    bool valid() const { return valid_; }
    void isolateTargets() {
        for (UINT i=1; i<targetCount_; ++i) device_->SetRenderTarget(i, nullptr);
        device_->SetDepthStencilSurface(nullptr);
    }
};

