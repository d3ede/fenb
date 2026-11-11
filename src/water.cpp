#include "water.hpp"
#include "generated/water_catalog.hpp"
#include <generated/water_vs.hpp>
#include <generated/water_ps.hpp>
#include <cstring>

WaterFX::~WaterFX() {
    invalidate();
    if (hash_)
        BCryptCloseAlgorithmProvider(hash_, 0);
}
void WaterFX::invalidate() {
    end();
    vs_.out();
    ps_.out();
    failed_ = false;
}
bool WaterFX::matches(IDirect3DTexture9 *texture) {
    DWORD tag = 0, size = sizeof(tag);
    if (SUCCEEDED(texture->GetPrivateData(WaterTag, &tag, &size)) && size == sizeof(tag))
        return tag == 1;
    D3DSURFACE_DESC desc{};
    if (FAILED(texture->GetLevelDesc(0, &desc)))
        return false;
    tag = 2;
    if (desc.Width == 128 && desc.Height == 128 && desc.Pool == D3DPOOL_MANAGED && !desc.Usage &&
        (desc.Format == D3DFMT_A1R5G5B5 || desc.Format == D3DFMT_DXT1)) {
        if (!hash_)
            BCryptOpenAlgorithmProvider(&hash_, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        D3DLOCKED_RECT lock{};
        if (hash_ && SUCCEEDED(texture->LockRect(0, &lock, nullptr, D3DLOCK_READONLY))) {
            BCRYPT_HASH_HANDLE hash = nullptr;
            if (BCryptCreateHash(hash_, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
                UINT rows = desc.Format == D3DFMT_DXT1 ? 32 : 128,
                     bytes = desc.Format == D3DFMT_DXT1 ? 256 : 256;
                bool ok = true;
                for (UINT y = 0; y < rows; ++y)
                    if (BCryptHashData(hash, (PUCHAR)lock.pBits + size_t(y) * lock.Pitch, bytes, 0) < 0) {
                        ok = false;
                        break;
                    }
                unsigned char digest[32]{};
                if (ok && BCryptFinishHash(hash, digest, 32, 0) >= 0)
                    for (auto &known : WaterHashes)
                        if (!memcmp(digest, known, 32)) {
                            tag = 1;
                            break;
                        }
                BCryptDestroyHash(hash);
            }
            texture->UnlockRect(0);
        } else
            return false; // transient lock failure: do not permanently blacklist the resource
    }
    texture->SetPrivateData(WaterTag, &tag, sizeof(tag), 0);
    return tag == 1;
}
bool WaterFX::begin(const Settings &s, UINT upStride) {
    if (failed_ || state_)
        return false;
    Com<IDirect3DPixelShader9> original;
    if (FAILED(device_->GetPixelShader(original.out())) || original.p)
        return false;
    DWORD z = 0, write = 1, blend = 0, src = 0, dest = 0;
    device_->GetRenderState(D3DRS_ZENABLE, &z);
    device_->GetRenderState(D3DRS_ZWRITEENABLE, &write);
    device_->GetRenderState(D3DRS_ALPHABLENDENABLE, &blend);
    device_->GetRenderState(D3DRS_SRCBLEND, &src);
    device_->GetRenderState(D3DRS_DESTBLEND, &dest);
    if (!z || write || !blend || src != D3DBLEND_SRCALPHA || dest != D3DBLEND_INVSRCALPHA)
        return false;
    if (!upStride) {
        Com<IDirect3DVertexBuffer9> vb;
        UINT offset = 0;
        if (FAILED(device_->GetStreamSource(0, vb.out(), &offset, &upStride)))
            return false;
    }
    if (upStride != 36)
        return false;
    Com<IDirect3DBaseTexture9> base;
    if (FAILED(device_->GetTexture(0, base.out())) || !base.p || base->GetType() != D3DRTYPE_TEXTURE)
        return false;
    if (!matches(static_cast<IDirect3DTexture9 *>(base.p)))
        return false;
    D3DMATRIX matrices[3]{};
    if (FAILED(device_->GetTransform(D3DTS_WORLD, &matrices[0])) ||
        FAILED(device_->GetTransform(D3DTS_VIEW, &matrices[1])) ||
        FAILED(device_->GetTransform(D3DTS_PROJECTION, &matrices[2])))
        return false;
    if (!vs_.p && (FAILED(device_->CreateVertexShader(ShaderWaterVS, vs_.out())) ||
                   FAILED(device_->CreatePixelShader(ShaderWaterPS, ps_.out())))) {
        failed_ = true;
        Log("WaterFX: shader creation failed; original material retained.");
        return false;
    }
    auto &v = matrices[1];
    // Rigid camera inverse: Neuz uses an orthonormal view matrix, confirmed in capture.
    float eye[] = {
        -(v._41 * v._11 + v._42 * v._12 + v._43 * v._13), -(v._41 * v._21 + v._42 * v._22 + v._43 * v._23),
        -(v._41 * v._31 + v._42 * v._32 + v._43 * v._33), float(GetTickCount64() % 3600000) * .001f};
    DWORD rangeFog = 0;
    device_->GetRenderState(D3DRS_RANGEFOGENABLE, &rangeFog);
    float waves[] = {s.waterWaves, s.waterLength, s.waterSpeed, rangeFog ? 1.f : 0.f};
    float surface[] = {s.waterRipples, s.waterRippleScale, s.waterOpacity, s.waterSky};
    float sun[] = {s.waterGlints, s.waterGloss, s.waterTint, s.waterSpeed};
    D3DLIGHT9 light{};
    BOOL on = FALSE;
    bool lit = SUCCEEDED(device_->GetLight(0, &light)) && SUCCEEDED(device_->GetLightEnable(0, &on)) && on &&
               light.Type == D3DLIGHT_DIRECTIONAL;
    float dir[] = {-light.Direction.x, -light.Direction.y, -light.Direction.z, lit ? 1.f : 0.f};
    float lightColor[] = {light.Diffuse.r, light.Diffuse.g, light.Diffuse.b, 1};
    DWORD fogEnabled = 0, fogMode = 0, color = 0, start = 0, fogEnd = 0, density = 0;
    device_->GetRenderState(D3DRS_FOGENABLE, &fogEnabled);
    device_->GetRenderState(D3DRS_FOGVERTEXMODE, &fogMode);
    if (!fogMode)
        device_->GetRenderState(D3DRS_FOGTABLEMODE, &fogMode);
    device_->GetRenderState(D3DRS_FOGCOLOR, &color);
    device_->GetRenderState(D3DRS_FOGSTART, &start);
    device_->GetRenderState(D3DRS_FOGEND, &fogEnd);
    device_->GetRenderState(D3DRS_FOGDENSITY, &density);
    float fogColor[] = {float((color >> 16) & 255) / 255, float((color >> 8) & 255) / 255,
                        float(color & 255) / 255, 1};
    float fog[] = {fogEnabled && fogMode ? 1.f : 0.f, float(fogMode), 0, 0}, extra[4]{};
    memcpy(&fog[2], &start, 4);
    memcpy(&fog[3], &fogEnd, 4);
    memcpy(&extra[0], &density, 4);
    auto state = std::make_unique<StateGuard>(device_);
    if (!state->valid())
        return false;
    state_ = std::move(state);
    if (FAILED(device_->SetVertexShader(vs_)) || FAILED(device_->SetPixelShader(ps_)) ||
        FAILED(device_->SetVertexShaderConstantF(0, &matrices[0].m[0][0], 12)) ||
        FAILED(device_->SetVertexShaderConstantF(12, eye, 1)) ||
        FAILED(device_->SetVertexShaderConstantF(13, waves, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(0, eye, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(1, surface, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(2, sun, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(3, dir, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(4, lightColor, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(5, fogColor, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(6, fog, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(7, extra, 1))) {
        end();
        return false;
    }
    device_->SetRenderState(D3DRS_FOGENABLE, FALSE); // PS performs the original fog exactly once
    return true;
}
