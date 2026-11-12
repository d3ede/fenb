#pragma once
#include "settings.hpp"
#include "scene_fx.hpp"

class Effects {
  public:
    explicit Effects(IDirect3DDevice9 *device) : device_(device), scene_(device) {}
    ~Effects() {
        invalidate();
    }
    bool prepare(IDirect3DSurface9 *backbuffer, const Settings &settings);
    void draw(IDirect3DSurface9 *backbuffer, const Settings &settings);
    void invalidate();
    void idle(const Settings &);
    size_t bytes() const {
        return bytes_ + scene_.bytes();
    }
    SceneFX &sceneFX() {
        return scene_;
    }

  private:
    void releaseTargets();
    bool makeTexture(UINT width, UINT height, IDirect3DTexture9 **texture, IDirect3DSurface9 **surface);
    bool shaders();
    void pass(IDirect3DSurface9 *target, UINT w, UINT h, IDirect3DPixelShader9 *shader,
              IDirect3DTexture9 *input, IDirect3DTexture9 *second = nullptr);
    IDirect3DDevice9 *device_;
    SceneFX scene_;
    IDirect3DTexture9 *frame_ = nullptr;
    IDirect3DSurface9 *frameSurface_ = nullptr;
    IDirect3DTexture9 *bloom_[2]{};
    IDirect3DSurface9 *bloomSurface_[2]{};
    IDirect3DPixelShader9 *bright_ = nullptr;
    IDirect3DPixelShader9 *blur_ = nullptr;
    IDirect3DPixelShader9 *composite_ = nullptr;
    UINT width_ = 0, height_ = 0, bw_ = 0, bh_ = 0;
    int scale_ = 0;
    size_t bytes_ = 0;
    bool shaderFailure_ = false;
};
