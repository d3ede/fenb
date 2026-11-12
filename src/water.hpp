#pragma once
#include "settings.hpp"
#include "state_guard.hpp"
#include "depth_copy.hpp"
#include <bcrypt.h>
// Texture-private metadata dies with the resource; no dangling pointer cache or retained game textures.
inline constexpr GUID WaterTag = {
    0x1a491582, 0x408f, 0x49bd, {0xa5, 0x63, 0x33, 0x6a, 0x9e, 0x51, 0x76, 0xe2}};
inline void InvalidateWaterTag(IDirect3DResource9 *resource) {
    if (resource)
        resource->FreePrivateData(WaterTag);
}
inline void InvalidateWaterSurface(IDirect3DSurface9 *surface) {
    Com<IDirect3DTexture9> texture;
    if (surface && SUCCEEDED(surface->GetContainer(__uuidof(IDirect3DTexture9),
                                                   reinterpret_cast<void **>(texture.out()))))
        InvalidateWaterTag(texture);
}
class WaterFX {
    IDirect3DDevice9 *device_;
    Com<IDirect3DVertexShader9> vs_;
    Com<IDirect3DPixelShader9> ps_;
    Com<IDirect3DTexture9> normals_;
    size_t normalBytes_ = 0;
    bool normalFailed_ = false;
    Com<IDirect3DTexture9> scene_;
    Com<IDirect3DSurface9> sceneSurface_;
    DepthCopy depth_;
    Com<IDirect3DCubeTexture9> environment_;
    bool skySeen_ = false, environmentReady_ = false, environmentUpdating_ = false;
    bool environmentFailed_ = false;
    DWORD environmentTime_ = 0;
    UINT sceneWidth_ = 0, sceneHeight_ = 0;
    D3DFORMAT sceneFormat_ = D3DFMT_UNKNOWN;
    bool sceneAttempted_ = false, sceneReady_ = false;
    std::unique_ptr<StateGuard> state_;
    BCRYPT_ALG_HANDLE hash_ = nullptr;
    bool failed_ = false;
    DWORD classify(IDirect3DTexture9 *texture);
    bool captureScene();
    bool normalTexture();

  public:
    explicit WaterFX(IDirect3DDevice9 *device) : device_(device), depth_(device) {}
    ~WaterFX();
    bool begin(const Settings &settings, UINT upStride = 0);
    void captureEnvironment(D3DPRIMITIVETYPE type, UINT start, UINT count);
    void beginFrame(bool refraction = true, bool sky = true, bool depth = true) {
        sceneAttempted_ = sceneReady_ = false;
        depth_.beginFrame(depth);
        skySeen_ = environmentUpdating_ = false;
        if (!sky) {
            environment_.out();
            environmentReady_ = false;
        }
        if (!refraction) {
            sceneSurface_.out();
            scene_.out();
        }
    }
    size_t bytes() const {
        return (scene_.p ? size_t(sceneWidth_) * sceneHeight_ * 4 : 0) +
               (environment_.p ? 128 * 128 * 4 * 6 : 0) + depth_.bytes() + normalBytes_;
    }
    void end(bool up = false, bool indexed = false) {
        const bool replaced = state_ != nullptr;
        state_.reset();
        // UP calls intentionally clear input bindings. Restoring a state block must not undo that.
        if (replaced && up)
            device_->SetStreamSource(0, nullptr, 0, 0);
        if (replaced && indexed)
            device_->SetIndices(nullptr);
    }
    void invalidate();
};
