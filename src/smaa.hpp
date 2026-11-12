#pragma once
#include "common.hpp"
#include <generated/smaa_shaders.hpp>
#include "../vendor/smaa/AreaTex.h"
#include "../vendor/smaa/SearchTex.h"
#include <cstring>

class Smaa {
    IDirect3DDevice9 *device_;
    Com<IDirect3DTexture9> edges_, weights_, area_, search_;
    Com<IDirect3DSurface9> edgeSurface_, weightSurface_;
    Com<IDirect3DPixelShader9> edgePS_, weightPS_, blendPS_;
    UINT width_ = 0, height_ = 0;
    bool lookup(Com<IDirect3DTexture9> &texture, UINT w, UINT h, D3DFORMAT format, const unsigned char *data,
                UINT pitch) {
        if (texture.p)
            return true;
        Com<IDirect3DTexture9> upload;
        if (FAILED(device_->CreateTexture(w, h, 1, 0, format, D3DPOOL_SYSTEMMEM, upload.out(), nullptr)))
            return false;
        D3DLOCKED_RECT rect{};
        if (FAILED(upload->LockRect(0, &rect, nullptr, 0)))
            return false;
        for (UINT y = 0; y < h; ++y)
            memcpy(static_cast<unsigned char *>(rect.pBits) + y * rect.Pitch, data + y * pitch, pitch);
        upload->UnlockRect(0);
        if (FAILED(device_->CreateTexture(w, h, 1, 0, format, D3DPOOL_DEFAULT, texture.out(), nullptr)) ||
            FAILED(device_->UpdateTexture(upload, texture))) {
            texture.out();
            return false;
        }
        return true;
    }
    bool target(Com<IDirect3DTexture9> &texture, Com<IDirect3DSurface9> &surface, UINT w, UINT h) {
        return SUCCEEDED(device_->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                                D3DPOOL_DEFAULT, texture.out(), nullptr)) &&
               SUCCEEDED(texture->GetSurfaceLevel(0, surface.out()));
    }
    bool pass(IDirect3DSurface9 *surface, IDirect3DPixelShader9 *shader, IDirect3DTexture9 *first,
              IDirect3DTexture9 *second = nullptr, IDirect3DTexture9 *third = nullptr, bool clear = false) {
        for (DWORD i = 0; i < 5; ++i)
            device_->SetTexture(i, nullptr);
        D3DVIEWPORT9 vp{0, 0, width_, height_, 0, 1};
        if (FAILED(device_->SetRenderTarget(0, surface)) || FAILED(device_->SetViewport(&vp)))
            return false;
        if (clear && FAILED(device_->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1, 0)))
            return false;
        device_->SetPixelShader(shader);
        device_->SetTexture(0, first);
        device_->SetTexture(1, second);
        device_->SetTexture(2, third);
        struct V {
            float x, y, z, rhw, u, v;
        };
        V quad[] = {{-.5f, -.5f, 0, 1, 0, 0},
                    {width_ - .5f, -.5f, 0, 1, 1, 0},
                    {-.5f, height_ - .5f, 0, 1, 0, 1},
                    {width_ - .5f, height_ - .5f, 0, 1, 1, 1}};
        return SUCCEEDED(device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(V)));
    }

  public:
    explicit Smaa(IDirect3DDevice9 *device) : device_(device) {}
    void invalidate() {
        edgeSurface_.out();
        weightSurface_.out();
        edges_.out();
        weights_.out();
        area_.out();
        search_.out();
        edgePS_.out();
        weightPS_.out();
        blendPS_.out();
        width_ = height_ = 0;
    }
    size_t bytes() const {
        return size_t(width_) * height_ * 8 + (area_.p ? AREATEX_SIZE : 0) + (search_.p ? SEARCHTEX_SIZE : 0);
    }
    bool apply(IDirect3DTexture9 *source, IDirect3DSurface9 *destination, UINT w, UINT h) {
        if (width_ != w || height_ != h) {
            invalidate();
            if (!target(edges_, edgeSurface_, w, h) || !target(weights_, weightSurface_, w, h)) {
                invalidate();
                return false;
            }
            width_ = w;
            height_ = h;
        }
        if (!edgePS_.p && FAILED(device_->CreatePixelShader(ShaderSMAAEdges, edgePS_.out())))
            return false;
        if (!weightPS_.p && FAILED(device_->CreatePixelShader(ShaderSMAAWeights, weightPS_.out())))
            return false;
        if (!blendPS_.p && FAILED(device_->CreatePixelShader(ShaderSMAABlend, blendPS_.out())))
            return false;
        if (!lookup(area_, AREATEX_WIDTH, AREATEX_HEIGHT, D3DFMT_A8L8, areaTexBytes, AREATEX_PITCH) ||
            !lookup(search_, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, D3DFMT_L8, searchTexBytes, SEARCHTEX_PITCH))
            return false;
        float metrics[] = {1.f / w, 1.f / h, float(w), float(h)};
        device_->SetPixelShaderConstantF(0, metrics, 1);
        for (DWORD i = 0; i < 3; ++i) {
            device_->SetSamplerState(i, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            device_->SetSamplerState(i, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            device_->SetSamplerState(i, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            device_->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            device_->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            device_->SetSamplerState(i, D3DSAMP_SRGBTEXTURE, FALSE);
        }
        return pass(edgeSurface_, edgePS_, source, nullptr, nullptr, true) &&
               pass(weightSurface_, weightPS_, edges_, area_, search_) &&
               pass(destination, blendPS_, source, weights_);
    }
};
