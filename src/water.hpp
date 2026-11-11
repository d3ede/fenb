#pragma once
#include "settings.hpp"
#include "state_guard.hpp"
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
    std::unique_ptr<StateGuard> state_;
    BCRYPT_ALG_HANDLE hash_ = nullptr;
    bool failed_ = false;
    bool matches(IDirect3DTexture9 *texture);

  public:
    explicit WaterFX(IDirect3DDevice9 *device) : device_(device) {}
    ~WaterFX();
    bool begin(const Settings &settings, UINT upStride = 0);
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
