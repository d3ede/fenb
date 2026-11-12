#include "scene_fx.hpp"
#include <generated/scene_shaders.hpp>
#include "generated/emitter_catalog.hpp"
#include "generated/foliage_catalog.hpp"
#include <cstring>
namespace {
struct PointSampler {
    IDirect3DDevice9 *device;
    DWORD slot, min = 0, mag = 0;
    PointSampler(IDirect3DDevice9 *d, DWORD s) : device(d), slot(s) {
        device->GetSamplerState(slot, D3DSAMP_MINFILTER, &min);
        device->GetSamplerState(slot, D3DSAMP_MAGFILTER, &mag);
        device->SetSamplerState(slot, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        device->SetSamplerState(slot, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    }
    ~PointSampler() {
        device->SetSamplerState(slot, D3DSAMP_MINFILTER, min);
        device->SetSamplerState(slot, D3DSAMP_MAGFILTER, mag);
    }
};
float LightLuminance(const D3DCOLORVALUE &color) {
    return color.r * .2126f + color.g * .7152f + color.b * .0722f;
}
float Daylight(const D3DCOLORVALUE &color) {
    return std::clamp((LightLuminance(color) - .12f) / .38f, 0.f, 1.f);
}
} // namespace

bool SceneFX::Target::make(IDirect3DDevice9 *d, UINT w, UINT h, D3DFORMAT f) {
    if (texture.p && width == w && height == h && format == f)
        return true;
    clear();
    if (FAILED(
            d->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, f, D3DPOOL_DEFAULT, texture.out(), nullptr)) ||
        FAILED(texture->GetSurfaceLevel(0, surface.out()))) {
        clear();
        return false;
    }
    width = w;
    height = h;
    format = f;
    return true;
}
bool SceneFX::programs() {
#define PROGRAM(member, entry)                                                                               \
    if (!member.p && FAILED(device_->CreatePixelShader(Shader##entry, member.out())))                        \
        return false;
    PROGRAM(aoPS_, SceneAO)
    PROGRAM(rayPS_, SceneRays)
    PROGRAM(compositePS_, SceneComposite)
    PROGRAM(lumaPS_, Luminance)
    PROGRAM(reducePS_, Reduce)
    PROGRAM(adaptPS_, Adapt)
    PROGRAM(skyPS_, SkyPS)
    PROGRAM(emitterPS_, EmitterPS)
    PROGRAM(sumPS_, EmitterSum)
    PROGRAM(localPS_, EmitterLighting) PROGRAM(visibilityPS_, EmitterVisibility) PROGRAM(fxaaPS_, FXAA)
#undef PROGRAM
        if (!emitterVS_.p &&
            FAILED(device_->CreateVertexShader(ShaderEmitterVS, emitterVS_.out()))) return false;
    return skyVS_.p || SUCCEEDED(device_->CreateVertexShader(ShaderSkyVS, skyVS_.out()));
}
void SceneFX::beginFrame() {
    camera_ = depthReady_ = emitters_ = false;
    skyApplied_ = sunOnScreen_ = false;
    lights_ = emitterDraws_ = 0;
    depth_.beginFrame(true);
}
void SceneFX::invalidate() {
    smaa_.invalidate();
    endSky();
    endFoliage();
    depth_.invalidate();
    output_.clear();
    ao_.clear();
    rays_.clear();
    for (auto &t : luma_)
        t.clear();
    for (auto &t : exposure_)
        t.clear();
    for (auto &t : balance_)
        t.clear();
    balanceTime_ = 0;
    exposureTime_ = 0;
    exposureIndex_ = 0;
    for (auto &t : emitterColor_)
        t.clear();
    for (auto &t : emitterPosition_)
        t.clear();
    localField_.clear();
    visibleEmitters_.clear();
    aoPS_.out();
    rayPS_.out();
    compositePS_.out();
    lumaPS_.out();
    reducePS_.out();
    adaptPS_.out();
    skyPS_.out();
    skyVS_.out();
    emitterPS_.out();
    emitterVS_.out();
    sumPS_.out();
    localPS_.out();
    visibilityPS_.out();
    fxaaPS_.out();
    beginFrame();
}
void SceneFX::releasePost() {
    smaa_.invalidate();
    depth_.invalidate();
    output_.clear();
    ao_.clear();
    rays_.clear();
    for (auto &t : luma_)
        t.clear();
    for (auto &t : exposure_)
        t.clear();
    for (auto &t : balance_)
        t.clear();
    balanceTime_ = 0;
    for (auto &t : emitterColor_)
        t.clear();
    for (auto &t : emitterPosition_)
        t.clear();
    localField_.clear();
    visibleEmitters_.clear();
    exposureTime_ = 0;
    emitters_ = depthReady_ = false;
}
size_t SceneFX::bytes() const {
    size_t n = depth_.bytes() + output_.bytes() + ao_.bytes() + rays_.bytes() + smaa_.bytes();
    for (auto &t : luma_)
        n += t.bytes();
    for (auto &t : exposure_)
        n += t.bytes();
    for (auto &t : balance_)
        n += t.bytes();
    for (auto &t : emitterColor_)
        n += t.bytes();
    for (auto &t : emitterPosition_)
        n += t.bytes();
    return n + localField_.bytes() + visibleEmitters_.bytes();
}
void SceneFX::observe(const Settings &s) {
    if (camera_ || (!s.ssao && !s.sunRays && !s.fog && !s.dof && !s.localLighting && !s.lightingBalance &&
                    s.nightFoliage <= 0))
        return;
    DWORD z = 0;
    device_->GetRenderState(D3DRS_ZWRITEENABLE, &z);
    if (!z)
        return;
    D3DMATRIX v{}, p{};
    if (FAILED(device_->GetTransform(D3DTS_VIEW, &v)) ||
        FAILED(device_->GetTransform(D3DTS_PROJECTION, &p)) || std::abs(p._34 - 1) > .001f ||
        std::abs(p._44) > .001f || std::abs(p._31) + std::abs(p._32) > .001f || p._11 <= 0 || p._22 <= 0)
        return;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            float dot = 0;
            for (int k = 0; k < 3; ++k)
                dot += v.m[i][k] * v.m[j][k];
            if (std::abs(dot - (i == j ? 1.f : 0.f)) > .01f)
                return;
        }
    if (FAILED(device_->GetViewport(&viewport_)))
        return;
    view_ = v;
    projection_ = p;
    camera_ = true;
    for (int axis = 0; axis < 3; ++axis)
        eye_[axis] = -(v._41 * v.m[axis][0] + v._42 * v.m[axis][1] + v._43 * v.m[axis][2]);
    BOOL on = FALSE;
    sunValid_ = SUCCEEDED(device_->GetLight(0, &sun_)) && SUCCEEDED(device_->GetLightEnable(0, &on)) && on &&
                sun_.Type == D3DLIGHT_DIRECTIONAL;
    DWORD ambient = 0;
    device_->GetRenderState(D3DRS_AMBIENT, &ambient);
    const float *sunColor = &sun_.Diffuse.r;
    for (int i = 0; i < 3; ++i)
        illumination_[i] = std::clamp(
            float((ambient >> (16 - i * 8)) & 255) / 255 + (sunValid_ ? sunColor[i] : 0), .025f, 1.f);
    if (s.localLighting)
        for (DWORD i = 0; i < 8; ++i) {
            D3DLIGHT9 l{};
            on = FALSE;
            if (FAILED(device_->GetLight(i, &l)) || FAILED(device_->GetLightEnable(i, &on)) || !on ||
                l.Type != D3DLIGHT_POINT || l.Range <= 0)
                continue;
            for (int axis = 0; axis < 3; ++axis)
                positions_[lights_][axis] = l.Position.x * v.m[0][axis] + l.Position.y * v.m[1][axis] +
                                            l.Position.z * v.m[2][axis] + v.m[3][axis];
            positions_[lights_][3] = l.Range;
            memcpy(colors_[lights_], &l.Diffuse, 16);
            attenuation_[lights_][0] = l.Attenuation0;
            attenuation_[lights_][1] = l.Attenuation1;
            attenuation_[lights_][2] = l.Attenuation2;
            ++lights_;
        }
}
bool SceneFX::prepare(UINT width, UINT height, const Settings &s) {
    if (!s.smaa)
        smaa_.invalidate();
    const bool geometry = s.ssao || s.sunRays || s.fog || s.dof || s.localLighting;
    if (!s.localLighting) {
        for (auto &t : emitterColor_)
            t.clear();
        for (auto &t : emitterPosition_)
            t.clear();
        localField_.clear();
        visibleEmitters_.clear();
    }
    depthReady_ = geometry && camera_ && depth_.capture(width, height);
    if (!geometry)
        depth_.invalidate();
    if (!depthReady_) {
        ao_.clear();
        rays_.clear();
    }
    if (!depthReady_ && !s.fxaa && !s.smaa) {
        output_.clear();
        return false;
    }
    if (!programs() || !output_.make(device_, width, height)) {
        depthReady_ = false;
        return false;
    }
    if (!depthReady_)
        return true;
    UINT scale = s.sceneQuality ? 2 : 4, w = std::max(1u, width / scale), h = std::max(1u, height / scale);
    if (!s.ssao)
        ao_.clear();
    else if (!ao_.make(device_, w, h))
        return false;
    if (!s.sunRays)
        rays_.clear();
    else if (!rays_.make(device_, w, h))
        return false;
    if (s.localLighting && emitters_ && !localField_.make(device_, w, h, D3DFMT_A16B16G16R16F))
        emitters_ = false;
    return true;
}
bool SceneFX::pass(Target &t, IDirect3DPixelShader9 *ps, IDirect3DTexture9 *image, IDirect3DTexture9 *depth) {
    for (DWORD i = 0; i < 5; ++i)
        device_->SetTexture(i, nullptr);
    D3DVIEWPORT9 vp{0, 0, t.width, t.height, 0, 1};
    if (FAILED(device_->SetRenderTarget(0, t.surface)) || FAILED(device_->SetViewport(&vp)) ||
        FAILED(device_->SetPixelShader(ps)) || FAILED(device_->SetTexture(0, image)) ||
        FAILED(device_->SetTexture(1, depth)))
        return false;
    struct V {
        float x, y, z, w, u, v;
    };
    const V quad[] = {{-.5f, -.5f, 0, 1, 0, 0},
                      {t.width - .5f, -.5f, 0, 1, 1, 0},
                      {-.5f, t.height - .5f, 0, 1, 0, 1},
                      {t.width - .5f, t.height - .5f, 0, 1, 1, 1}};
    return SUCCEEDED(device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(V)));
}
IDirect3DTexture9 *SceneFX::process(IDirect3DTexture9 *input, const Settings &s,
                                    IDirect3DSurface9 *reusable) {
    PointSampler localSampler(device_, 4);
    auto finish = [&](IDirect3DTexture9 *source) {
        if ((!s.fxaa && !s.smaa) || !output_.texture.p)
            return source;
        Target borrowed;
        Target *destination = &output_;
        if (source == output_.texture.p) {
            if (!reusable)
                return source;
            borrowed.texture.p = input;
            input->AddRef();
            borrowed.surface.p = reusable;
            reusable->AddRef();
            borrowed.width = output_.width;
            borrowed.height = output_.height;
            destination = &borrowed;
        }
        if (s.smaa)
            return smaa_.apply(source, destination->surface, output_.width, output_.height)
                       ? destination->texture.p
                       : source;
        float step[] = {1.f / output_.width, 1.f / output_.height, 0, 0},
              control[] = {s.fxaaStrength, s.fxaaThreshold, 0, 0};
        device_->SetPixelShaderConstantF(0, step, 1);
        device_->SetPixelShaderConstantF(1, control, 1);
        return pass(*destination, fxaaPS_, source) ? destination->texture.p : source;
    };
    if (!depthReady_ || !output_.texture.p)
        return finish(input);
    const float w = float(output_.width), h = float(output_.height);
    float c[20][4]{};
    c[0][0] = projection_._11;
    c[0][1] = projection_._22;
    c[0][2] = projection_._33;
    c[0][3] = projection_._43;
    c[1][0] = viewport_.X / w;
    c[1][1] = viewport_.Y / h;
    c[1][2] = viewport_.Width / w;
    c[1][3] = viewport_.Height / h;
    c[2][0] = 1 / w;
    c[2][1] = 1 / h;
    c[3][0] = s.aoStrength;
    c[3][1] = s.aoRadius;
    c[3][2] = s.aoBias;
    c[3][3] = s.aoDistance;
    c[4][0] = s.fogDensity;
    c[4][1] = s.fogStart;
    c[4][2] = s.fogHeight;
    c[4][3] = s.fogFalloff;
    c[5][0] = s.fogR;
    c[5][1] = s.fogG;
    c[5][2] = s.fogB;
    c[6][0] = s.focusDistance;
    c[6][1] = s.focusRange;
    c[6][2] = s.dofRadius;
    c[6][3] = s.dofNear;
    c[7][0] = s.ssao;
    c[7][1] = s.fog;
    c[7][2] = s.dof;
    c[7][3] = s.sunRays;
    float dir[] = {-sun_.Direction.x, -sun_.Direction.y, -sun_.Direction.z};
    float viewSun[3]{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            viewSun[i] += dir[j] * view_.m[j][i];
    if (sunValid_ && dir[1] > 0 && viewSun[2] > .05f) {
        c[8][0] = (viewSun[0] * projection_._11 / viewSun[2] * .5f + .5f) * c[1][2] + c[1][0];
        c[8][1] = (-viewSun[1] * projection_._22 / viewSun[2] * .5f + .5f) * c[1][3] + c[1][1];
        c[8][2] = Daylight(sun_.Diffuse);
        sunOnScreen_ = c[8][2] > .01f && c[8][0] >= c[1][0] && c[8][0] <= c[1][0] + c[1][2] &&
                       c[8][1] >= c[1][1] && c[8][1] <= c[1][1] + c[1][3];
    }
    c[8][3] = s.rayStrength;
    c[9][0] = s.rayLength;
    c[9][1] = s.rayDecay;
    if (sunValid_)
        memcpy(c[10], &sun_.Diffuse, 16);
    memcpy(c[11], eye_, 16);
    memcpy(c[12], &view_, 64);
    memcpy(c[16], illumination_, 16);
    c[17][0] = ao_.width ? 1.f / ao_.width : 0;
    c[17][1] = ao_.height ? 1.f / ao_.height : 0;
    c[18][0] = s.sceneQuality ? 16 : 8;
    c[17][2] = localField_.width ? 1.f / localField_.width : 0;
    c[17][3] = localField_.height ? 1.f / localField_.height : 0;
    c[19][0] = s.localLighting ? float(lights_) : 0;
    c[19][1] = s.localStrength;
    c[19][2] = s.localRange;
    if (s.localLighting && emitters_)
        reduceEmitters(s);
    c[19][3] = s.localLighting && emitters_ ? 1.f : 0.f;
    device_->SetPixelShaderConstantF(0, c[0], 20);
    device_->SetPixelShaderConstantF(20, positions_[0], 8);
    device_->SetPixelShaderConstantF(28, colors_[0], 8);
    device_->SetPixelShaderConstantF(36, attenuation_[0], 8);
    if (c[19][3] > .5f) {
        PointSampler sourceImage(device_, 0), sourcePositions(device_, 2), sourceColors(device_, 3);
        if (!visibleEmitters_.make(device_, 8, 4, D3DFMT_A16B16G16R16F) ||
            !pass(visibleEmitters_, visibilityPS_, emitterPosition_[3].texture, depth_.texture()))
            c[19][3] = 0;
        // Same camera/depth constants as the final surface pass; atlas RGB stores actual view positions.
        for (DWORD i = 0; i < 5; ++i)
            device_->SetTexture(i, nullptr);
        D3DVIEWPORT9 vp{0, 0, localField_.width, localField_.height, 0, 1};
        device_->SetRenderTarget(0, localField_.surface);
        device_->SetViewport(&vp);
        device_->SetPixelShader(localPS_);
        device_->SetTexture(1, depth_.texture());
        device_->SetTexture(2, visibleEmitters_.texture);
        device_->SetTexture(3, emitterColor_[3].texture);
        struct V {
            float x, y, z, w, u, v;
        };
        const float lw = float(vp.Width), lh = float(vp.Height);
        const V q[] = {{-.5f, -.5f, 0, 1, 0, 0},
                       {lw - .5f, -.5f, 0, 1, 1, 0},
                       {-.5f, lh - .5f, 0, 1, 0, 1},
                       {lw - .5f, lh - .5f, 0, 1, 1, 1}};
        if (FAILED(device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(V))))
            c[19][3] = 0;
        device_->SetPixelShaderConstantF(19, c[19], 1);
    }
    if (s.ssao && !pass(ao_, aoPS_, input, depth_.texture()))
        return finish(input);
    if (s.sunRays && !pass(rays_, rayPS_, input, depth_.texture()))
        return finish(input);
    for (DWORD i = 0; i < 5; ++i)
        device_->SetTexture(i, nullptr);
    D3DVIEWPORT9 vp{0, 0, output_.width, output_.height, 0, 1};
    device_->SetRenderTarget(0, output_.surface);
    device_->SetViewport(&vp);
    device_->SetPixelShader(compositePS_);
    device_->SetTexture(0, input);
    device_->SetTexture(1, depth_.texture());
    device_->SetTexture(2, ao_.texture);
    device_->SetTexture(3, rays_.texture);
    device_->SetTexture(4, c[19][3] > .5f ? localField_.texture.p : nullptr);
    struct V {
        float x, y, z, w, u, v;
    };
    const V quad[] = {{-.5f, -.5f, 0, 1, 0, 0},
                      {w - .5f, -.5f, 0, 1, 1, 0},
                      {-.5f, h - .5f, 0, 1, 0, 1},
                      {w - .5f, h - .5f, 0, 1, 1, 1}};
    return finish(SUCCEEDED(device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(V)))
                      ? output_.texture.p
                      : input);
}
bool SceneFX::emissive(IDirect3DTexture9 *texture) {
    DWORD tag = 0, size = 4;
    if (SUCCEEDED(texture->GetPrivateData(EmitterTag, &tag, &size)) && size == 4)
        return (tag & 1) != 0;
    D3DSURFACE_DESC d{};
    if (FAILED(texture->GetLevelDesc(0, &d)) || d.Pool != D3DPOOL_MANAGED || d.Usage)
        return false;
    if (d.Format != D3DFMT_DXT1 && d.Format != D3DFMT_DXT3 && d.Format != D3DFMT_DXT5 &&
        d.Format != D3DFMT_A8R8G8B8) {
        tag = 4;
        texture->SetPrivateData(EmitterTag, &tag, 4, 0);
        return false;
    }
    if (!hash_)
        BCryptOpenAlgorithmProvider(&hash_, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    D3DLOCKED_RECT lock{};
    if (!hash_ || FAILED(texture->LockRect(0, &lock, nullptr, D3DLOCK_READONLY)))
        return false;
    BCRYPT_HASH_HANDLE h = nullptr;
    bool ok = BCryptCreateHash(hash_, &h, nullptr, 0, nullptr, 0, 0) >= 0;
    UINT rows = d.Format == D3DFMT_A8R8G8B8 ? d.Height : (d.Height + 3) / 4;
    UINT bytes =
        d.Format == D3DFMT_A8R8G8B8 ? d.Width * 4 : ((d.Width + 3) / 4) * (d.Format == D3DFMT_DXT1 ? 8 : 16);
    for (UINT y = 0; ok && y < rows; ++y)
        ok = BCryptHashData(h, (PUCHAR)lock.pBits + y * lock.Pitch, bytes, 0) >= 0;
    unsigned char digest[32]{};
    ok = ok && BCryptFinishHash(h, digest, 32, 0) >= 0;
    if (h)
        BCryptDestroyHash(h);
    texture->UnlockRect(0);
    if (!ok)
        return false;
    tag = 0;
    for (auto &known : EmitterHashes)
        if (!memcmp(digest, known, 32))
            tag |= 1;
    for (auto &known : FoliageHashes)
        if (!memcmp(digest, known, 32))
            tag |= 2;
    if (!tag)
        tag = 4;
    texture->SetPrivateData(EmitterTag, &tag, 4, 0);
    return (tag & 1) != 0;
}
void SceneFX::beginFoliage(const Settings &s) {
    if ((s.nightFoliage <= 0 && !s.lightingBalance) || !camera_ || !sunValid_ || foliageActive_ || skyState_)
        return;
    float night = std::clamp(1 - LightLuminance(sun_.Diffuse) / .55f, 0.f, 1.f);
    if (night <= 0)
        return;
    DWORD lit = 0, fvf = 0;
    device_->GetRenderState(D3DRS_LIGHTING, &lit);
    device_->GetFVF(&fvf);
    if (!lit || fvf != 0x112)
        return;
    DWORD write = 0, blend = 0;
    device_->GetRenderState(D3DRS_ZWRITEENABLE, &write);
    device_->GetRenderState(D3DRS_ALPHABLENDENABLE, &blend);
    Com<IDirect3DPixelShader9> ps;
    device_->GetPixelShader(ps.out());
    if (ps.p)
        return;
    Com<IDirect3DBaseTexture9> base;
    if (FAILED(device_->GetTexture(0, base.out())) || !base.p || base->GetType() != D3DRTYPE_TEXTURE)
        return;
    auto *texture = static_cast<IDirect3DTexture9 *>(base.p);
    emissive(texture);
    DWORD tag = 0, size = 4;
    texture->GetPrivateData(EmitterTag, &tag, &size);
    float fill = (tag & 2) ? s.nightFoliage * .25f : 0.f;
    // Common opaque fixed-function props and weapons receive bounded ambient illumination
    // before texture shading. Skinned/programmed paths are covered by the shared tone balance.
    if (s.lightingBalance && write && !blend)
        fill = std::max(fill, s.lightingBalanceStrength * .12f);
    if (fill <= 0)
        return;
    DWORD ambient = 0;
    device_->GetRenderState(D3DRS_AMBIENT, &ambient);
    DWORD brighter = 0;
    for (int i = 0; i < 3; ++i) {
        int value = int((ambient >> (16 - i * 8)) & 255);
        value = std::max(value, std::min(90, value + int(255 * fill * night)));
        brighter |= DWORD(value) << (16 - i * 8);
    }
    D3DMATERIAL9 material{};
    if (FAILED(device_->GetMaterial(&material)))
        return;
    originalMaterial_ = material;
    originalAmbient_ = ambient;
    material.Ambient.r = std::max(.5f, material.Ambient.r);
    material.Ambient.g = std::max(.5f, material.Ambient.g);
    material.Ambient.b = std::max(.5f, material.Ambient.b);
    device_->SetMaterial(&material);
    device_->SetRenderState(D3DRS_AMBIENT, brighter);
    foliageActive_ = true;
}
void SceneFX::captureEmitter(const Settings &s, DrawCall draw) {
    if (!s.localLighting || !camera_ || emitterDraws_ >= 64)
        return;
    DWORD blend = 0, write = 1, source = 0, dest = 0, fvf = 0;
    device_->GetRenderState(D3DRS_ALPHABLENDENABLE, &blend);
    if (!blend)
        return;
    device_->GetRenderState(D3DRS_ZWRITEENABLE, &write);
    if (write)
        return;
    device_->GetRenderState(D3DRS_DESTBLEND, &dest);
    if (dest != D3DBLEND_ONE)
        return;
    device_->GetRenderState(D3DRS_SRCBLEND, &source);
    device_->GetFVF(&fvf);
    if (!blend || write || dest != D3DBLEND_ONE || (source != D3DBLEND_ONE && source != D3DBLEND_SRCALPHA) ||
        (fvf & D3DFVF_POSITION_MASK) != D3DFVF_XYZ)
        return;
    Com<IDirect3DPixelShader9> ps;
    device_->GetPixelShader(ps.out());
    if (ps.p)
        return;
    Com<IDirect3DBaseTexture9> base;
    if (FAILED(device_->GetTexture(0, base.out())) || !base.p || base->GetType() != D3DRTYPE_TEXTURE ||
        !emissive(static_cast<IDirect3DTexture9 *>(base.p)))
        return;
    DWORD lighting = 0, test = 0, transform = 0, stage1 = 0, coordinate = 0;
    device_->GetRenderState(D3DRS_LIGHTING, &lighting);
    device_->GetRenderState(D3DRS_ALPHATESTENABLE, &test);
    device_->GetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, &transform);
    device_->GetTextureStageState(1, D3DTSS_COLOROP, &stage1);
    device_->GetTextureStageState(0, D3DTSS_TEXCOORDINDEX, &coordinate);
    if (lighting || test || transform || coordinate || stage1 != D3DTOP_DISABLE ||
        !(fvf & D3DFVF_TEXCOUNT_MASK))
        return;
    DWORD operations[6]{};
    D3DTEXTURESTAGESTATETYPE keys[] = {D3DTSS_COLOROP, D3DTSS_COLORARG1, D3DTSS_COLORARG2,
                                       D3DTSS_ALPHAOP, D3DTSS_ALPHAARG1, D3DTSS_ALPHAARG2};
    for (int i = 0; i < 6; ++i)
        if (FAILED(device_->GetTextureStageState(0, keys[i], operations + i)))
            return;
    if (operations[0] < D3DTOP_SELECTARG1 || operations[0] > D3DTOP_ADD || operations[3] > D3DTOP_ADD)
        return;
    for (int i : {1, 2, 4, 5})
        if (((operations[i] & 15) > 3 && (operations[i] & 15) != 6) || operations[i] > 63)
            return;
    DWORD factor = 0xffffffff, constant = 0xffffffff, srgb = 0;
    device_->GetRenderState(D3DRS_TEXTUREFACTOR, &factor);
    device_->GetRenderState(D3DRS_SRGBWRITEENABLE, &srgb);
    device_->GetTextureStageState(0, D3DTSS_CONSTANT, &constant);
    auto rgba = [](DWORD packed, float *out) {
        out[0] = float((packed >> 16) & 255) / 255;
        out[1] = float((packed >> 8) & 255) / 255;
        out[2] = float(packed & 255) / 255;
        out[3] = float(packed >> 24) / 255;
    };
    float stages[4][4] = {
        {float(operations[0]), float(operations[1]), float(operations[2]), float(operations[3])},
        {float(operations[4]), float(operations[5]), 0, 0},
        {},
        {}};
    rgba(factor, stages[2]);
    rgba(constant, stages[3]);
    D3DCAPS9 caps{};
    device_->GetDeviceCaps(&caps);
    if (caps.NumSimultaneousRTs < 2 || !(caps.PrimitiveMiscCaps & D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING) ||
        !programs())
        return;
    for (auto *t : {&emitterColor_[0], &emitterPosition_[0]})
        if (!t->make(device_, 256, 128, D3DFMT_A16B16G16R16F))
            return;
    StateGuard state(device_);
    if (!state.valid())
        return;
    state.isolateTargets();
    for (DWORD i = 1; i < 5; ++i)
        device_->SetTexture(i, nullptr);
    if (FAILED(device_->SetRenderTarget(0, emitterColor_[0].surface)) ||
        FAILED(device_->SetRenderTarget(1, emitterPosition_[0].surface)))
        return;
    if (!emitters_ && FAILED(device_->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1, 0)))
        return;
    D3DVIEWPORT9 vp{0, 0, 256, 128, 0, 1};
    device_->SetViewport(&vp);
    D3DMATRIX world{};
    device_->GetTransform(D3DTS_WORLD, &world);
    D3DMATRIX matrices[] = {world, view_, projection_};
    float flags[] = {fvf & D3DFVF_DIFFUSE ? 1.f : 0.f, 0, 0, 0},
          alpha[] = {source == D3DBLEND_SRCALPHA ? 1.f : 0.f, srgb ? 1.f : 0.f, 0, 0};
    D3DMATERIAL9 material{};
    material.Diffuse = {1, 1, 1, 1};
    device_->GetMaterial(&material);
    device_->SetVertexShader(emitterVS_);
    device_->SetPixelShader(emitterPS_);
    device_->SetVertexShaderConstantF(0, &matrices[0].m[0][0], 12);
    device_->SetVertexShaderConstantF(12, flags, 1);
    device_->SetPixelShaderConstantF(0, alpha, 1);
    device_->SetPixelShaderConstantF(1, stages[0], 4);
    device_->SetPixelShaderConstantF(10, &material.Diffuse.r, 1);
    for (auto key : {D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ALPHATESTENABLE, D3DRS_STENCILENABLE,
                     D3DRS_SCISSORTESTENABLE, D3DRS_FOGENABLE, D3DRS_SRGBWRITEENABLE,
                     D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_CLIPPLANEENABLE})
        device_->SetRenderState(key, FALSE);
    device_->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    device_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
    device_->SetRenderState(D3DRS_COLORWRITEENABLE, 15);
    device_->SetRenderState(D3DRS_COLORWRITEENABLE1, 15);
    device_->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    if (SUCCEEDED(draw())) {
        emitters_ = true;
        ++emitterDraws_;
    }
}
void SceneFX::reduceEmitters(const Settings &) {
    PointSampler sourceData(device_, 0);
    for (int i = 1; i < 4; ++i) {
        const UINT factor = i == 3 ? 2 : 4;
        for (auto *map : {emitterColor_, emitterPosition_}) {
            auto &previous = map[i - 1];
            auto &next = map[i];
            if (!next.make(device_, previous.width / factor, previous.height / factor,
                           D3DFMT_A16B16G16R16F)) {
                emitters_ = false;
                return;
            }
            float step[] = {1.f / previous.width, 1.f / previous.height, float(factor), 0};
            device_->SetPixelShaderConstantF(0, step, 1);
            if (!pass(next, sumPS_, previous.texture)) {
                emitters_ = false;
                return;
            }
        }
    }
}
IDirect3DTexture9 *SceneFX::exposure(IDirect3DTexture9 *input, const Settings &s) {
    if (!s.adaptation) {
        for (auto &t : exposure_)
            t.clear();
        exposureTime_ = 0;
    }
    if (!s.lightingBalance) {
        for (auto &t : balance_)
            t.clear();
        balanceTime_ = 0;
    }
    if (!s.adaptation && !s.lightingBalance) {
        for (auto &t : luma_)
            t.clear();
        return nullptr;
    }
    if (!programs())
        return nullptr;
    for (UINT i = 0, size = 64; i < 4; ++i, size /= 4)
        if (!luma_[i].make(device_, size, size))
            return nullptr;
    D3DSURFACE_DESC desc{};
    input->GetLevelDesc(0, &desc);
    float step[] = {1.f / desc.Width, 1.f / desc.Height, 0, 0};
    device_->SetPixelShaderConstantF(0, step, 1);
    if (!pass(luma_[0], lumaPS_, input))
        return nullptr;
    for (int i = 1; i < 4; ++i) {
        step[0] = step[1] = 1.f / luma_[i - 1].width;
        device_->SetPixelShaderConstantF(0, step, 1);
        if (!pass(luma_[i], reducePS_, luma_[i - 1].texture))
            return nullptr;
    }
    auto adapt = [&](Target(&pair)[2], unsigned &index, ULONGLONG &time, float key, float low, float high,
                     float bright, float dark, bool initializeAtTarget) -> IDirect3DTexture9 * {
        bool initial = !pair[0].texture.p || !pair[1].texture.p || !time;
        for (auto &t : pair)
            if (!t.make(device_, 1, 1, D3DFMT_R32F))
                return nullptr;
        ULONGLONG now = GetTickCount64();
        float dt = !initial ? std::clamp((now - time) * .001f, 0.f, .25f) : .016f;
        float control[] = {key, low, high, dt}, speed[] = {bright, dark, 0, 0};
        if (initial) {
            control[3] = initializeAtTarget ? -2.f : -1.f;
            device_->SetPixelShaderConstantF(0, control, 1);
            if (!pass(pair[index], adaptPS_, luma_[3].texture))
                return nullptr;
            control[3] = dt;
        }
        device_->SetPixelShaderConstantF(0, control, 1);
        device_->SetPixelShaderConstantF(1, speed, 1);
        unsigned next = index ^ 1;
        if (!pass(pair[next], adaptPS_, luma_[3].texture, pair[index].texture))
            return nullptr;
        index = next;
        time = now;
        return pair[next].texture;
    };
    if (s.lightingBalance && !adapt(balance_, balanceIndex_, balanceTime_, 1, -12, 12, .9f, .9f, true)) {
        for (auto &t : balance_)
            t.clear();
        balanceTime_ = 0;
    }
    return s.adaptation ? adapt(exposure_, exposureIndex_, exposureTime_, s.adaptationKey, s.minEV, s.maxEV,
                                s.adaptBright, s.adaptDark, false)
                        : nullptr;
}
bool SceneFX::beginSky(const Settings &s, DWORD tag) {
    if (!s.sky || (tag != 3 && tag != 4) || skyState_ || !programs())
        return false;
    D3DMATRIX m[3]{};
    if (FAILED(device_->GetTransform(D3DTS_WORLD, m)) || FAILED(device_->GetTransform(D3DTS_VIEW, m + 1)) ||
        FAILED(device_->GetTransform(D3DTS_PROJECTION, m + 2)))
        return false;
    if (std::abs(m[1]._41) + std::abs(m[1]._42) + std::abs(m[1]._43) > .001f ||
        std::abs(m[2]._34 - 1) > .001f)
        return false;
    DWORD flags = 0, coordinate = 0, stage1 = 0, lighting = 0;
    device_->GetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, &flags);
    device_->GetTextureStageState(0, D3DTSS_TEXCOORDINDEX, &coordinate);
    device_->GetTextureStageState(1, D3DTSS_COLOROP, &stage1);
    device_->GetRenderState(D3DRS_LIGHTING, &lighting);
    if ((flags != D3DTTFF_DISABLE && flags != D3DTTFF_COUNT2) || coordinate || stage1 != D3DTOP_DISABLE ||
        lighting)
        return false;
    D3DMATRIX uv{};
    if (FAILED(device_->GetTransform(D3DTS_TEXTURE0, &uv)))
        return false;
    DWORD operations[6]{};
    D3DTEXTURESTAGESTATETYPE keys[] = {D3DTSS_COLOROP, D3DTSS_COLORARG1, D3DTSS_COLORARG2,
                                       D3DTSS_ALPHAOP, D3DTSS_ALPHAARG1, D3DTSS_ALPHAARG2};
    for (int i = 0; i < 6; ++i)
        if (FAILED(device_->GetTextureStageState(0, keys[i], operations + i)))
            return false;
    if (operations[0] < D3DTOP_SELECTARG1 || operations[0] > D3DTOP_ADD || operations[3] > D3DTOP_ADD)
        return false;
    for (int i : {1, 2, 4, 5})
        if (((operations[i] & 15) > 3 && (operations[i] & 15) != 6) || operations[i] > 63)
            return false;
    float stages[4][4] = {
        {float(operations[0]), float(operations[1]), float(operations[2]), float(operations[3])},
        {float(operations[4]), float(operations[5]), 0, 0},
        {},
        {}};
    DWORD packed[2] = {0xffffffff, 0xffffffff};
    device_->GetRenderState(D3DRS_TEXTUREFACTOR, packed);
    device_->GetTextureStageState(0, D3DTSS_CONSTANT, packed + 1);
    for (int i = 0; i < 2; ++i) {
        for (int channel = 0; channel < 3; ++channel)
            stages[i + 2][channel] = float((packed[i] >> (16 - 8 * channel)) & 255) / 255;
        stages[i + 2][3] = float(packed[i] >> 24) / 255;
    }
    auto state = std::make_unique<StateGuard>(device_);
    if (!state->valid())
        return false;
    D3DLIGHT9 light{};
    BOOL enabled = FALSE;
    bool lit = SUCCEEDED(device_->GetLight(0, &light)) && SUCCEEDED(device_->GetLightEnable(0, &enabled)) &&
               enabled && light.Type == D3DLIGHT_DIRECTIONAL;
    DWORD fog = 0;
    device_->GetRenderState(D3DRS_FOGCOLOR, &fog);
    float options[] = {s.skyStrength, s.skyHorizon, s.cloudContrast, s.sunHalo},
          kind[] = {tag == 4 ? 1.f : 0.f, 0, 0, 0};
    float direction[] = {-light.Direction.x, -light.Direction.y, -light.Direction.z,
                         lit && light.Direction.y < 0 ? Daylight(light.Diffuse) : 0.f};
    float illumination[4]{};
    for (int i = 0; i < 3; ++i)
        illumination[i] = lit ? std::clamp((&light.Diffuse.r)[i], .02f, 1.f)
                              : std::clamp(float((fog >> (16 - i * 8)) & 255) / 255, .02f, 1.f);
    if (FAILED(device_->SetVertexShader(skyVS_)) || FAILED(device_->SetPixelShader(skyPS_)))
        return false;
    device_->SetVertexShaderConstantF(0, &m[0].m[0][0], 12);
    device_->SetVertexShaderConstantF(12, &uv.m[0][0], 4);
    float layout[] = {flags == D3DTTFF_COUNT2 ? 1.f : 0.f, 0, 0, 0};
    device_->SetVertexShaderConstantF(16, layout, 1);
    device_->SetPixelShaderConstantF(0, options, 1);
    device_->SetPixelShaderConstantF(1, kind, 1);
    device_->SetPixelShaderConstantF(10, &light.Diffuse.r, 1);
    device_->SetPixelShaderConstantF(11, direction, 1);
    device_->SetPixelShaderConstantF(16, illumination, 1);
    device_->SetPixelShaderConstantF(20, stages[0], 4);
    device_->SetRenderState(D3DRS_FOGENABLE, FALSE);
    skyState_ = std::move(state);
    skyApplied_ = true;
    return true;
}
