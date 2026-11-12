#include "water.hpp"
#include "scene_fx.hpp"
#include "generated/water_catalog.hpp"
#include <generated/water_vs.hpp>
#include <generated/water_ps.hpp>
#include <generated/reflection_ps.hpp>
#include <generated/caustics.hpp>
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
    reflectionPS_.out();
    reflectionSurface_.out();
    reflection_.out();
    normals_.out();
    normalBytes_ = 0;
    normalFailed_ = false;
    depth_.invalidate();
    causticSurface_.out();
    caustics_.out();
    causticShader_.out();
    causticWidth_ = causticHeight_ = 0;
    causticFailed_ = false;
    sceneSurface_.out();
    scene_.out();
    sceneWidth_ = sceneHeight_ = 0;
    sceneFormat_ = D3DFMT_UNKNOWN;
    environment_.out();
    environmentReady_ = false;
    environmentFailed_ = false;
    environmentTime_ = 0;
    beginFrame();
    failed_ = false;
}
bool WaterFX::captureScene() {
    if (sceneAttempted_)
        return sceneReady_;
    sceneAttempted_ = true;
    Com<IDirect3DSurface9> source;
    D3DSURFACE_DESC desc{};
    if (FAILED(device_->GetRenderTarget(0, source.out())) || FAILED(source->GetDesc(&desc)) ||
        (desc.Format != D3DFMT_X8R8G8B8 && desc.Format != D3DFMT_A8R8G8B8))
        return false;
    if (!scene_.p || desc.Width != sceneWidth_ || desc.Height != sceneHeight_ ||
        desc.Format != sceneFormat_) {
        sceneSurface_.out();
        scene_.out();
        if (FAILED(device_->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format,
                                          D3DPOOL_DEFAULT, scene_.out(), nullptr)) ||
            FAILED(scene_->GetSurfaceLevel(0, sceneSurface_.out()))) {
            scene_.out();
            return false;
        }
        sceneWidth_ = desc.Width;
        sceneHeight_ = desc.Height;
        sceneFormat_ = desc.Format;
    }
    // Color-only resolve before the first matched water draw, including MSAA sources.
    // Never copy depth, read back to the CPU, or sample the live render target.
    sceneReady_ = SUCCEEDED(device_->StretchRect(source, nullptr, sceneSurface_, nullptr, D3DTEXF_NONE));
    return sceneReady_;
}
bool WaterFX::normalTexture() {
    if (normals_.p)
        return true;
    if (normalFailed_)
        return false;
    constexpr UINT size = 128, levels = 8;
    if (FAILED(device_->CreateTexture(size, size, levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, normals_.out(),
                                      nullptr))) {
        normalFailed_ = true;
        return false;
    }
    // A periodic wave spectrum, generated once and sampled as two drifting, rotated layers.
    struct Wave {
        float x, y, phase, amplitude;
    };
    Wave waves[32]{};
    DWORD seed = 0x71a934d5;
    auto random = [&]() {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return seed;
    };
    float energy = 0;
    for (auto &wave : waves) {
        do {
            wave.x = float(int(random() % 33) - 16);
            wave.y = float(int(random() % 33) - 16);
        } while (wave.x * wave.x + wave.y * wave.y < 4);
        float length = std::sqrt(wave.x * wave.x + wave.y * wave.y);
        wave.amplitude = std::pow(length, -.7f);
        wave.phase = float(random() & 65535) * (6.2831853f / 65536);
        energy += wave.amplitude * wave.amplitude;
    }
    float scale = .6f / std::sqrt(energy * .5f);
    auto encode = [](float value) { return DWORD(std::clamp(value * .5f + .5f, 0.f, 1.f) * 255 + .5f); };
    std::vector<DWORD> pixels(size * size);
    for (UINT y = 0; y < size; ++y)
        for (UINT x = 0; x < size; ++x) {
            float sx = 0, sy = 0;
            for (const auto &wave : waves) {
                float phase = 6.2831853f * (wave.x * x + wave.y * y) / size + wave.phase;
                float derivative =
                    std::cos(phase) * wave.amplitude / std::sqrt(wave.x * wave.x + wave.y * wave.y);
                sx += wave.x * derivative;
                sy += wave.y * derivative;
            }
            pixels[y * size + x] = 0xff000080 | (encode(sx * scale) << 16) | (encode(sy * scale) << 8);
        }
    normalBytes_ = 0;
    for (UINT level = 0, width = size; level < levels; ++level, width /= 2) {
        D3DLOCKED_RECT lock{};
        if (FAILED(normals_->LockRect(level, &lock, nullptr, 0))) {
            normals_.out();
            normalBytes_ = 0;
            normalFailed_ = true;
            return false;
        }
        for (UINT y = 0; y < width; ++y)
            memcpy((BYTE *)lock.pBits + y * lock.Pitch, pixels.data() + y * width, width * 4);
        normals_->UnlockRect(level);
        normalBytes_ += size_t(width) * width * 4;
        if (width == 1)
            break;
        std::vector<DWORD> next(width * width / 4);
        for (UINT y = 0; y < width / 2; ++y)
            for (UINT x = 0; x < width / 2; ++x) {
                DWORD packed = 0xff000080;
                for (UINT shift : {8u, 16u}) {
                    UINT sum = 0;
                    for (UINT dy = 0; dy < 2; ++dy)
                        for (UINT dx = 0; dx < 2; ++dx)
                            sum += (pixels[(y * 2 + dy) * width + x * 2 + dx] >> shift) & 255;
                    packed |= ((sum + 2) / 4) << shift;
                }
                next[y * (width / 2) + x] = packed;
            }
        pixels = std::move(next);
    }
    return true;
}
DWORD WaterFX::classify(IDirect3DTexture9 *texture) {
    DWORD tag = 0, size = sizeof(tag);
    if (SUCCEEDED(texture->GetPrivateData(WaterTag, &tag, &size)) && size == sizeof(tag))
        return tag;
    D3DSURFACE_DESC desc{};
    if (FAILED(texture->GetLevelDesc(0, &desc)))
        return false;
    tag = 2;
    bool waterSize = desc.Width == 128 && desc.Height == 128;
    bool skySize = (desc.Width == 2048 && desc.Height == 512) || (desc.Width == 1024 && desc.Height == 256) ||
                   (desc.Width == 512 && desc.Height == 512) || (desc.Width == 256 && desc.Height == 256);
    if ((waterSize || skySize) && desc.Pool == D3DPOOL_MANAGED && !desc.Usage &&
        ((waterSize && desc.Format == D3DFMT_A1R5G5B5) || desc.Format == D3DFMT_DXT1 ||
         (skySize && (desc.Format == D3DFMT_DXT3 || desc.Format == D3DFMT_DXT5)))) {
        if (!hash_)
            BCryptOpenAlgorithmProvider(&hash_, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        D3DLOCKED_RECT lock{};
        if (hash_ && SUCCEEDED(texture->LockRect(0, &lock, nullptr, D3DLOCK_READONLY))) {
            BCRYPT_HASH_HANDLE hash = nullptr;
            if (BCryptCreateHash(hash_, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
                bool compressed = desc.Format != D3DFMT_A1R5G5B5;
                UINT rows = compressed ? (desc.Height + 3) / 4 : desc.Height,
                     bytes = compressed ? ((desc.Width + 3) / 4) * (desc.Format == D3DFMT_DXT1 ? 8 : 16)
                                        : desc.Width * 2;
                bool ok = true;
                for (UINT y = 0; y < rows; ++y)
                    if (BCryptHashData(hash, (PUCHAR)lock.pBits + size_t(y) * lock.Pitch, bytes, 0) < 0) {
                        ok = false;
                        break;
                    }
                unsigned char digest[32]{};
                if (ok && BCryptFinishHash(hash, digest, 32, 0) >= 0) {
                    for (auto &known : WaterHashes)
                        if (!memcmp(digest, known, 32))
                            tag = 1;
                    for (auto &known : SkyHashes)
                        if (!memcmp(digest, known, 32))
                            tag = 3;
                    for (auto &known : CloudHashes)
                        if (!memcmp(digest, known, 32))
                            tag = 4;
                }
                BCryptDestroyHash(hash);
            }
            texture->UnlockRect(0);
        } else
            return false; // transient lock failure: do not permanently blacklist the resource
    }
    texture->SetPrivateData(WaterTag, &tag, sizeof(tag), 0);
    return tag;
}
void WaterFX::captureEnvironment(D3DPRIMITIVETYPE type, UINT start, UINT count, SceneFX *sky,
                                 const Settings *settings) {
    if (state_ || environmentFailed_)
        return;
    Com<IDirect3DPixelShader9> ps;
    Com<IDirect3DBaseTexture9> texture;
    DWORD z = 1, write = 1;
    device_->GetRenderState(D3DRS_ZENABLE, &z);
    device_->GetRenderState(D3DRS_ZWRITEENABLE, &write);
    if (z || write || FAILED(device_->GetPixelShader(ps.out())) || ps.p ||
        FAILED(device_->GetTexture(0, texture.out())) || !texture.p || texture->GetType() != D3DRTYPE_TEXTURE)
        return;
    DWORD tag = classify(static_cast<IDirect3DTexture9 *>(texture.p));
    if (tag != 3 && tag != 4)
        return;
    D3DMATRIX view{}, projection{};
    if (FAILED(device_->GetTransform(D3DTS_VIEW, &view)) ||
        FAILED(device_->GetTransform(D3DTS_PROJECTION, &projection)) ||
        std::abs(view._41) + std::abs(view._42) + std::abs(view._43) > .001f ||
        std::abs(projection._34 - 1) > .001f)
        return;
    const bool firstSky = tag == 3 && !skySeen_;
    if (firstSky) {
        skySeen_ = true;
        environmentUpdating_ =
            !environment_.p || !environmentReady_ || GetTickCount() - environmentTime_ >= 250;
        if (environmentUpdating_)
            environmentReady_ = false;
    }
    if (!skySeen_ || !environmentUpdating_)
        return;
    if (!environment_.p && FAILED(device_->CreateCubeTexture(128, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                                             D3DPOOL_DEFAULT, environment_.out(), nullptr))) {
        environmentFailed_ = true;
        return;
    }
    StateGuard state(device_);
    if (!state.valid())
        return;
    state.isolateTargets();
    device_->SetTexture(2, nullptr);
    device_->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device_->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device_->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    D3DMATRIX cubeProjection{};
    cubeProjection._11 = cubeProjection._22 = 1;
    cubeProjection._33 = projection._33;
    cubeProjection._34 = 1;
    cubeProjection._43 = projection._43;
    device_->SetTransform(D3DTS_PROJECTION, &cubeProjection);
    constexpr float directions[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    constexpr float ups[6][3] = {{0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0}};
    DWORD fog = 0;
    device_->GetRenderState(D3DRS_FOGCOLOR, &fog);
    bool ok = true;
    for (UINT face = 0; face < 6; ++face) {
        Com<IDirect3DSurface9> target;
        const auto &f = directions[face], &u = ups[face];
        float right[] = {u[1] * f[2] - u[2] * f[1], u[2] * f[0] - u[0] * f[2], u[0] * f[1] - u[1] * f[0]};
        D3DMATRIX m{};
        for (int axis = 0; axis < 3; ++axis) {
            m.m[axis][0] = right[axis];
            m.m[axis][1] = u[axis];
            m.m[axis][2] = f[axis];
        }
        m._44 = 1;
        D3DVIEWPORT9 vp{0, 0, 128, 128, 0, 1};
        if (FAILED(environment_->GetCubeMapSurface(D3DCUBEMAP_FACES(face), 0, target.out())) ||
            FAILED(device_->SetRenderTarget(0, target)) || FAILED(device_->SetViewport(&vp)) ||
            FAILED(device_->SetTransform(D3DTS_VIEW, &m)) ||
            (firstSky && FAILED(device_->Clear(0, nullptr, D3DCLEAR_TARGET, fog | 0xff000000, 1, 0)))) {
            ok = false;
            break;
        }
        if (sky && settings)
            sky->beginSky(*settings, tag);
        const HRESULT draw = device_->DrawPrimitive(type, start, count);
        if (sky)
            sky->endSky();
        if (FAILED(draw)) {
            ok = false;
            break;
        }
    }
    environmentReady_ = ok;
    // A transition may blend several skyboxes before the cloud layer. Latch the update for
    // the entire frame: restarting the timer/clearing per draw drops all but one layer.
    // Never publish a partial capture after a failed face, even if a later layer succeeds.
    if (!ok)
        environmentUpdating_ = false;
    if (ok)
        environmentTime_ = GetTickCount();
}
bool WaterFX::prepareCaustics(const Settings &s, const D3DMATRIX &view, const float *eye, const float *camera,
                              const float *viewport, const float *dir, const float *light) {
    if (causticAttempted_)
        return causticReady_;
    causticAttempted_ = true;
    if (causticFailed_ || s.waterCaustics <= 0 || dir[3] == 0 || dir[1] <= .05f)
        return false;
    const UINT width = std::max(1u, sceneWidth_ / s.waterCausticScale);
    const UINT height = std::max(1u, sceneHeight_ / s.waterCausticScale);
    if (!causticShader_.p && FAILED(device_->CreatePixelShader(ShaderCausticFieldPS, causticShader_.out()))) {
        causticFailed_ = true;
        return false;
    }
    if (!caustics_.p || width != causticWidth_ || height != causticHeight_) {
        causticSurface_.out();
        caustics_.out();
        if (FAILED(device_->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                          D3DPOOL_DEFAULT, caustics_.out(), nullptr)) ||
            FAILED(caustics_->GetSurfaceLevel(0, causticSurface_.out()))) {
            caustics_.out();
            causticFailed_ = true;
            return false;
        }
        causticWidth_ = width;
        causticHeight_ = height;
    }
    StateGuard state(device_);
    if (!state.valid())
        return false;
    state.isolateTargets();
    for (DWORD sampler = 0; sampler < 6; ++sampler)
        device_->SetTexture(sampler, nullptr);
    const D3DVIEWPORT9 vp{0, 0, width, height, 0, 1};
    float waves[] = {s.waterWaves, s.waterLength, s.waterSpeed, s.waterRipples};
    float optics[] = {s.waterRippleScale, s.waterCaustics, 2.f, s.waterDispersion};
    if (FAILED(device_->SetRenderTarget(0, causticSurface_)) || FAILED(device_->SetViewport(&vp)) ||
        FAILED(device_->SetVertexShader(nullptr)) || FAILED(device_->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1)) ||
        FAILED(device_->SetPixelShader(causticShader_)) ||
        FAILED(device_->SetPixelShaderConstantF(0, eye, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(1, camera, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(2, viewport, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(3, &view.m[0][0], 4)) ||
        FAILED(device_->SetPixelShaderConstantF(7, waves, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(8, optics, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(9, dir, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(10, light, 1)) ||
        FAILED(device_->SetTexture(0, depth_.texture())) || FAILED(device_->SetTexture(1, normals_)))
        return false;
    for (auto key : {D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE,
                     D3DRS_STENCILENABLE, D3DRS_SCISSORTESTENABLE, D3DRS_FOGENABLE, D3DRS_SRGBWRITEENABLE,
                     D3DRS_CLIPPLANEENABLE})
        device_->SetRenderState(key, FALSE);
    device_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device_->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    device_->SetRenderState(D3DRS_COLORWRITEENABLE, 15);
    for (DWORD sampler : {0u, 1u}) {
        device_->SetSamplerState(sampler, D3DSAMP_MINFILTER, sampler ? D3DTEXF_LINEAR : D3DTEXF_POINT);
        device_->SetSamplerState(sampler, D3DSAMP_MAGFILTER, sampler ? D3DTEXF_LINEAR : D3DTEXF_POINT);
        device_->SetSamplerState(sampler, D3DSAMP_MIPFILTER, sampler ? D3DTEXF_LINEAR : D3DTEXF_NONE);
        device_->SetSamplerState(sampler, D3DSAMP_ADDRESSU, sampler ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
        device_->SetSamplerState(sampler, D3DSAMP_ADDRESSV, sampler ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
        device_->SetSamplerState(sampler, D3DSAMP_SRGBTEXTURE, FALSE);
    }
    struct Vertex {
        float x, y, z, w, u, v;
    };
    const Vertex quad[] = {{-.5f, -.5f, 0, 1, 0, 0},
                           {width - .5f, -.5f, 0, 1, 1, 0},
                           {-.5f, height - .5f, 0, 1, 0, 1},
                           {width - .5f, height - .5f, 0, 1, 1, 1}};
    causticReady_ = SUCCEEDED(device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex)));
    return causticReady_;
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
    if (classify(static_cast<IDirect3DTexture9 *>(base.p)) != 1)
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
    bool needScene = s.waterRefraction > 0 || s.waterDepth > 0 || s.waterSSR > 0 || s.waterCaustics > 0;
    bool refract = needScene && captureScene();
    if (!needScene && scene_.p) {
        sceneSurface_.out();
        scene_.out();
        sceneReady_ = false;
    }
    float screen[] = {sceneWidth_ ? 1.f / sceneWidth_ : 0, sceneHeight_ ? 1.f / sceneHeight_ : 0,
                      refract ? 1.f : 0.f, s.waterRefraction};
    bool environment = s.waterSky > 0 && skySeen_ && environmentReady_ && environment_.p;
    float detail[] = {s.waterTexture, s.waterWaves, s.waterLength, environment ? 1.f : 0.f};
    const auto &projection = matrices[2];
    bool perspective = std::abs(projection._34 - 1) < .001f && std::abs(projection._44) < .001f &&
                       std::abs(projection._31) + std::abs(projection._32) < .001f && projection._11 > 0 &&
                       projection._22 > 0;
    bool depth = refract && perspective && (s.waterDepth > 0 || s.waterSSR > 0 || s.waterCaustics > 0) &&
                 depth_.capture(sceneWidth_, sceneHeight_);
    // Shore softness describes the thin contact edge, not a multi-unit band without a surface.
    float optics[] = {depth ? 1.f : 0.f, s.waterDepth, s.waterShore * .1f, s.waterSSR};
    traceReady_ = depth && s.waterSSR > 0;
    if (!traceReady_) {
        reflectionSurface_.out();
        reflection_.out();
    } else {
        D3DSURFACE_DESC reflectionDesc{};
        if (reflection_.p &&
            (!reflectionSurface_.p || FAILED(reflection_->GetLevelDesc(0, &reflectionDesc)) ||
             reflectionDesc.Width != sceneWidth_ || reflectionDesc.Height != sceneHeight_)) {
            reflectionSurface_.out();
            reflection_.out();
        }
        if ((!reflectionPS_.p &&
             FAILED(device_->CreatePixelShader(ShaderReflectionPS, reflectionPS_.out()))) ||
            (!reflection_.p &&
             (FAILED(device_->CreateTexture(sceneWidth_, sceneHeight_, 1, D3DUSAGE_RENDERTARGET,
                                            D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, reflection_.out(), nullptr)) ||
              FAILED(reflection_->GetSurfaceLevel(0, reflectionSurface_.out())))))
            traceReady_ = false;
    }
    if (!traceReady_)
        optics[3] = 0;
    float camera[] = {projection._11, projection._22, projection._33, projection._43};
    D3DVIEWPORT9 viewport{};
    device_->GetViewport(&viewport);
    float viewportUV[] = {viewport.X * screen[0], viewport.Y * screen[1], viewport.Width * screen[0],
                          viewport.Height * screen[1]};
    bool normalMap = normalTexture();
    if (!normalMap)
        return false;
    float trace[] = {s.waterSSRQuality ? 24.f : 12.f, 80.f, normalMap ? 1.f : 0.f,
                     2 * depth_.quantum() / std::max(.01f, std::abs(projection._43))};
    D3DLIGHT9 light{};
    BOOL on = FALSE;
    bool lit = SUCCEEDED(device_->GetLight(0, &light)) && SUCCEEDED(device_->GetLightEnable(0, &on)) && on &&
               light.Type == D3DLIGHT_DIRECTIONAL;
    float dir[] = {-light.Direction.x, -light.Direction.y, -light.Direction.z, lit ? 1.f : 0.f};
    float lightColor[] = {light.Diffuse.r, light.Diffuse.g, light.Diffuse.b, 1};
    bool caustics = depth && prepareCaustics(s, v, eye, camera, viewportUV, dir, lightColor);
    float absorption = -std::log(std::max(.001f, 1 - s.waterOpacity)) * .14f * s.waterDepth;
    float subsurface[] = {caustics ? 1.f : 0.f, absorption, absorption * .35f, 0};
    DWORD lighting = 0, ambientColor = 0;
    device_->GetRenderState(D3DRS_LIGHTING, &lighting);
    device_->GetRenderState(D3DRS_AMBIENT, &ambientColor);
    D3DMATERIAL9 material{};
    material.Ambient = material.Diffuse = {1, 1, 1, 1};
    device_->GetMaterial(&material);
    float ambient[] = {1, 1, 1, 0}, diffuse[] = {0, 0, 0, 0};
    const float global[] = {float((ambientColor >> 16) & 255) / 255, float((ambientColor >> 8) & 255) / 255,
                            float(ambientColor & 255) / 255};
    auto channel = [](const D3DCOLORVALUE &value, int i) {
        return i == 0 ? value.r : i == 1 ? value.g : value.b;
    };
    if (lighting) {
        for (int i = 0; i < 3; ++i) {
            ambient[i] = std::clamp(channel(material.Ambient, i) *
                                            (global[i] + (lit ? channel(light.Ambient, i) : 0)) +
                                        channel(material.Emissive, i),
                                    0.f, 1.f);
            diffuse[i] = lit ? channel(material.Diffuse, i) * lightColor[i] : 0;
        }
    }
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
    auto multiply = [](const D3DMATRIX &a, const D3DMATRIX &b) {
        D3DMATRIX result{};
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column)
                for (int k = 0; k < 4; ++k)
                    result.m[row][column] += a.m[row][k] * b.m[k][column];
        return result;
    };
    auto clip = multiply(multiply(matrices[0], matrices[1]), matrices[2]);
    if (FAILED(device_->SetVertexShader(vs_)) || FAILED(device_->SetPixelShader(ps_)) ||
        FAILED(device_->SetVertexShaderConstantF(0, &matrices[0].m[0][0], 12)) ||
        FAILED(device_->SetVertexShaderConstantF(12, eye, 1)) ||
        FAILED(device_->SetVertexShaderConstantF(13, waves, 1)) ||
        FAILED(device_->SetVertexShaderConstantF(14, &clip.m[0][0], 4)) ||
        FAILED(device_->SetPixelShaderConstantF(0, eye, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(1, surface, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(2, sun, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(3, dir, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(4, lightColor, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(5, fogColor, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(6, fog, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(7, extra, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(8, screen, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(9, detail, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(10, &matrices[1].m[0][0], 4)) ||
        FAILED(device_->SetPixelShaderConstantF(14, optics, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(15, camera, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(16, viewportUV, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(17, trace, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(18, ambient, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(19, diffuse, 1)) ||
        FAILED(device_->SetPixelShaderConstantF(20, subsurface, 1)) ||
        FAILED(device_->SetTexture(1, refract ? scene_.p : nullptr)) ||
        FAILED(device_->SetTexture(2, environment ? environment_.p : nullptr)) ||
        FAILED(device_->SetTexture(3, depth ? depth_.texture() : nullptr)) ||
        FAILED(device_->SetTexture(4, normalMap ? normals_.p : nullptr)) ||
        FAILED(device_->SetTexture(5, caustics ? caustics_.p : nullptr))) {
        end();
        return false;
    }
    device_->SetRenderState(D3DRS_FOGENABLE, FALSE); // PS performs the original fog exactly once
    for (DWORD sampler : {1u, 2u, 5u}) {
        for (auto key : {D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER})
            device_->SetSamplerState(sampler, key, D3DTEXF_LINEAR);
        device_->SetSamplerState(sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device_->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device_->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        device_->SetSamplerState(sampler, D3DSAMP_SRGBTEXTURE, FALSE);
    }
    for (auto key : {D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER})
        device_->SetSamplerState(3, key, D3DTEXF_POINT);
    device_->SetSamplerState(3, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    device_->SetSamplerState(3, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device_->SetSamplerState(3, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    device_->SetSamplerState(3, D3DSAMP_SRGBTEXTURE, FALSE);
    for (auto key : {D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER})
        device_->SetSamplerState(4, key, D3DTEXF_LINEAR);
    device_->SetSamplerState(4, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    device_->SetSamplerState(4, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    device_->SetSamplerState(4, D3DSAMP_SRGBTEXTURE, FALSE);
    return true;
}
void WaterFX::reflect(DrawCall draw) {
    if (!state_ || !traceReady_)
        return;
    bool ok = false;
    {
        StateGuard guard(device_);
        D3DVIEWPORT9 viewport{};
        if (guard.valid()) {
            guard.isolateTargets();
            device_->GetViewport(&viewport);
            device_->SetTexture(6, nullptr);
            if (SUCCEEDED(device_->SetRenderTarget(0, reflectionSurface_)) &&
                SUCCEEDED(device_->SetViewport(&viewport)) &&
                SUCCEEDED(device_->SetPixelShader(reflectionPS_))) {
                device_->SetRenderState(D3DRS_ZENABLE, FALSE);
                device_->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
                device_->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
                device_->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
                device_->SetRenderState(D3DRS_COLORWRITEENABLE, 15);
                ok = SUCCEEDED(draw());
            }
        }
    }
    if (ok) {
        device_->SetTexture(6, reflection_);
        for (auto key : {D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER})
            device_->SetSamplerState(6, key, D3DTEXF_POINT);
        device_->SetSamplerState(6, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device_->SetSamplerState(6, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device_->SetSamplerState(6, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        device_->SetSamplerState(6, D3DSAMP_SRGBTEXTURE, FALSE);
    } else {
        float optics[4]{};
        device_->GetPixelShaderConstantF(14, optics, 1);
        optics[3] = 0;
        device_->SetPixelShaderConstantF(14, optics, 1);
    }
}
