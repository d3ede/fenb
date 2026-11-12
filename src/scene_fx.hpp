#pragma once
#include "depth_copy.hpp"
#include "settings.hpp"
#include "draw_call.hpp"
#include <bcrypt.h>
#include "scene_tags.hpp"

// GPU-only depth optics. All targets and camera samples are scoped to one device/main frame.
class SceneFX {
    friend struct SceneFXFixture;
    struct Target {
        Com<IDirect3DTexture9> texture;
        Com<IDirect3DSurface9> surface;
        UINT width = 0, height = 0;
        D3DFORMAT format = D3DFMT_A8R8G8B8;
        bool make(IDirect3DDevice9 *, UINT, UINT, D3DFORMAT = D3DFMT_A8R8G8B8);
        void clear() {
            surface.out();
            texture.out();
            width = height = 0;
        }
        size_t bytes() const {
            return texture.p ? size_t(width) * height * (format == D3DFMT_A16B16G16R16F ? 8 : 4) : 0;
        }
    };
    IDirect3DDevice9 *device_;
    DepthCopy depth_;
    Target output_, ao_, rays_, luma_[4], exposure_[2];
    Target emitterColor_[4], emitterPosition_[4], localField_, visibleEmitters_;
    Com<IDirect3DPixelShader9> aoPS_, rayPS_, compositePS_, lumaPS_, reducePS_, adaptPS_, skyPS_;
    Com<IDirect3DVertexShader9> skyVS_;
    Com<IDirect3DVertexShader9> emitterVS_;
    Com<IDirect3DPixelShader9> emitterPS_, sumPS_, localPS_, visibilityPS_, fxaaPS_;
    BCRYPT_ALG_HANDLE hash_ = nullptr;
    bool emitters_ = false;
    unsigned emitterDraws_ = 0;
    std::unique_ptr<StateGuard> skyState_;
    bool foliageActive_ = false;
    DWORD originalAmbient_ = 0;
    D3DMATERIAL9 originalMaterial_{};
    D3DMATRIX view_{}, projection_{};
    D3DVIEWPORT9 viewport_{};
    D3DLIGHT9 sun_{};
    float eye_[4]{}, illumination_[4]{1, 1, 1, 0};
    float positions_[8][4]{}, colors_[8][4]{}, attenuation_[8][4]{};
    unsigned lights_ = 0, exposureIndex_ = 0;
    ULONGLONG exposureTime_ = 0;
    bool camera_ = false, sunValid_ = false, depthReady_ = false;
    bool programs();
    bool emissive(IDirect3DTexture9 *);
    void reduceEmitters(const Settings &);
    bool pass(Target &, IDirect3DPixelShader9 *, IDirect3DTexture9 *, IDirect3DTexture9 * = nullptr);

  public:
    explicit SceneFX(IDirect3DDevice9 *d) : device_(d), depth_(d) {}
    ~SceneFX() {
        invalidate();
        if (hash_)
            BCryptCloseAlgorithmProvider(hash_, 0);
    }
    void beginFrame();
    void observe(const Settings &);
    void invalidate();
    void releasePost();
    bool prepare(UINT, UINT, const Settings &);
    IDirect3DTexture9 *process(IDirect3DTexture9 *, const Settings &, IDirect3DSurface9 *reusable = nullptr);
    IDirect3DTexture9 *exposure(IDirect3DTexture9 *, const Settings &);
    bool beginSky(const Settings &, DWORD materialTag);
    void endSky() {
        skyState_.reset();
    }
    void beginFoliage(const Settings &);
    void endFoliage() {
        if (foliageActive_) {
            device_->SetMaterial(&originalMaterial_);
            device_->SetRenderState(D3DRS_AMBIENT, originalAmbient_);
            foliageActive_ = false;
        }
    }
    void captureEmitter(const Settings &, DrawCall);
    size_t bytes() const;
    bool depthReady() const {
        return depthReady_;
    }
    unsigned lightCount() const {
        return lights_;
    }
};
