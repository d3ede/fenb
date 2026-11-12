#pragma once
#include "depth_copy.hpp"
#include "settings.hpp"
#include "draw_call.hpp"
#include <bcrypt.h>
#include "scene_tags.hpp"
#include "smaa.hpp"

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
    Smaa smaa_;
    Target output_, ao_, rays_, raySource_, luma_[4], exposure_[2], balance_[2];
    Target skyBackdrop_;
    Com<IDirect3DCubeTexture9> skyEnvironment_;
    Target skyMean_[3];
    Com<IDirect3DPixelShader9> skyMeanPS_;
    bool skyBackdropReady_ = false;
    float nativeFog_[4]{}; // enabled, start, end, range-based
    float nativeFogColor_[4]{};
    Target emitterColor_[4], emitterPosition_[4], localField_;
    Com<IDirect3DPixelShader9> aoPS_, rayPS_, raySourcePS_, compositePS_, lumaPS_, reducePS_, adaptPS_, skyPS_;
    Com<IDirect3DPixelShader9> dofPS_;
    Com<IDirect3DVertexShader9> skyVS_;
    Com<IDirect3DPixelShader9> flarePS_;
    Com<IDirect3DVertexShader9> emitterVS_;
    Com<IDirect3DPixelShader9> emitterPS_, sumPS_, localPS_, fxaaPS_;
    BCRYPT_ALG_HANDLE hash_ = nullptr;
    bool emitters_ = false;
    unsigned emitterDraws_ = 0;
    struct SourceSlot {
        uint64_t key = 0;
        ULONGLONG seen = 0;
        unsigned frame = 0;
        float anchor[3]{};
    };
    SourceSlot sources_[32]{};
    unsigned sourceFrame_ = 0;
    float sourceOrigin_[4]{};
    bool sourceOriginValid_ = false;
    std::unique_ptr<StateGuard> skyState_;
    bool foliageActive_ = false;
    DWORD originalAmbient_ = 0;
    D3DMATERIAL9 originalMaterial_{};
    D3DMATRIX view_{}, projection_{};
    D3DVIEWPORT9 viewport_{};
    D3DLIGHT9 sun_{};
    float eye_[4]{}, illumination_[4]{1, 1, 1, 0};
    float positions_[8][4]{}, colors_[8][4]{}, attenuation_[8][4]{};
    unsigned lights_ = 0, exposureIndex_ = 0, balanceIndex_ = 0;
    ULONGLONG exposureTime_ = 0, balanceTime_ = 0;
    bool camera_ = false, sunValid_ = false, depthReady_ = false;
    bool skyApplied_ = false, sunOnScreen_ = false;
    bool programs();
    bool emissive(IDirect3DTexture9 *);
    void reduceEmitters(const Settings &);
    bool pass(Target &, IDirect3DPixelShader9 *, IDirect3DTexture9 *, IDirect3DTexture9 * = nullptr);

  public:
    explicit SceneFX(IDirect3DDevice9 *d) : device_(d), depth_(d), smaa_(d) {}
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
    IDirect3DTexture9 *process(IDirect3DTexture9 *, const Settings &);
    IDirect3DTexture9 *postTexture() const {
        return output_.texture;
    }
    IDirect3DSurface9 *postSurface() const {
        return output_.surface;
    }
    bool antialias(IDirect3DTexture9 *, IDirect3DSurface9 *, const Settings &);
    IDirect3DTexture9 *exposure(IDirect3DTexture9 *, const Settings &);
    IDirect3DTexture9 *balanceHistory() const {
        return balance_[balanceIndex_].texture;
    }
    bool beginSky(const Settings &, DWORD materialTag);
    void skyEnvironment(IDirect3DCubeTexture9 *texture) {
        if (skyEnvironment_.p == texture) return;
        skyEnvironment_.out();
        skyEnvironment_.p = texture;
        if (texture) texture->AddRef();
    }
    void beginFlare(const Settings &);
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
    bool skyApplied() const {
        return skyApplied_;
    }
    bool sunOnScreen() const {
        return sunOnScreen_;
    }
    bool hasLightSource() const {
        return lights_ != 0 || emitters_;
    }
};
