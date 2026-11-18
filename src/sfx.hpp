#pragma once
#include "settings.hpp"
#include "scene_tags.hpp"
#include "generated/sfx_catalog.hpp"
#include <generated/sfx_shaders.hpp>
#include <bcrypt.h>
#include <cstring>
#include "refinement_trail.hpp"
#include "particle_buffer.hpp"

// Material styling plus scoped reconstruction of recognized native refinement trails.
class SfxFX {
    friend struct SfxFXFixture;
    IDirect3DDevice9 *device_;
    BCRYPT_ALG_HANDLE hash_ = nullptr;
    Com<IDirect3DVertexShader9> vs_;
    Com<IDirect3DPixelShader9> ps_, auraPS_, refinementPS_, cloudPS_, discardPS_;
    Com<IDirect3DTexture9> trailArt_;
    std::vector<refinement_trail::Puff> trailPuffs_;
    std::vector<refinement_trail::Vertex> trailVertices_;
    native_refinement::Context *trailContext_=nullptr;
    float trailConstants_[11][4]{};
    DWORD trailSource_=D3DBLEND_SRCALPHA;
    bool trailRange_=false;
    unsigned trailPoints_=0;
    void finishTrail() {
        if(!trailContext_)return;
        refinement_trail::mesh(trailPuffs_,view_,trailVertices_);
        StateGuard state(device_);
        if(state.valid()&&!trailVertices_.empty()) {
            D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
            float layout[]={1,0,trailRange_?1.f:0.f,0};
            device_->SetFVF(0x142);device_->SetVertexShader(vs_);device_->SetPixelShader(cloudPS_);
            device_->SetTexture(0,trailArt_);
            device_->SetVertexShaderConstantF(0,&projection_.m[0][0],4);
            device_->SetVertexShaderConstantF(4,&identity.m[0][0],4);
            device_->SetVertexShaderConstantF(8,&identity.m[0][0],4);
            device_->SetVertexShaderConstantF(12,layout,1);
            device_->SetPixelShaderConstantF(0,trailConstants_[0],11);
            device_->SetViewport(&viewport_);
            device_->SetRenderState(D3DRS_LIGHTING,FALSE);device_->SetRenderState(D3DRS_FOGENABLE,FALSE);
            device_->SetRenderState(D3DRS_ZENABLE,TRUE);device_->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
            device_->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);device_->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
            device_->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
            device_->SetRenderState(D3DRS_SRCBLEND,trailSource_);device_->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);
            device_->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);device_->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
            device_->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(trailVertices_.size()/3),trailVertices_.data(),sizeof(refinement_trail::Vertex));
        }
        trailContext_=nullptr;trailPuffs_.clear();trailVertices_.clear();trailArt_.out();
    }
    bool captureTrail(ParticleDraw draw,DWORD fvf,const D3DMATRIX &world,
                      IDirect3DTexture9 *texture,const float c[11][4],DWORD src,DWORD dst) {
        auto *context=native_refinement::current;
        if(!context||!context->model||dst!=D3DBLEND_ONE||trailPoints_+draw.primitives/2>2048||
           (trailContext_&&trailContext_!=context)||(context->consumer&&context->consumer!=this)||
           (trailArt_.p&&trailArt_.p!=texture)||trailPuffs_.size()+draw.primitives/2>256)return false;
        if(c[1][0]<2||c[1][0]>6||c[1][3]<2||c[1][3]>6)return false;
        for(float arg:{c[1][1],c[1][2],c[2][0],c[2][1]})if(arg<0||arg>6||arg==4||arg==5)return false;
        if((!cloudPS_.p&&FAILED(device_->CreatePixelShader(ShaderRefinementCloudPS,cloudPS_.out())))||
           (!discardPS_.p&&FAILED(device_->CreatePixelShader(ShaderRefinementDiscardPS,discardPS_.out()))))return false;
        if(trailPuffs_.capacity()<256)trailPuffs_.reserve(256);
        if(trailVertices_.capacity()<12288)trailVertices_.reserve(12288);
        particle_buffer::Snapshot snapshot;
        if(!draw.data&&!particle_buffer::read(device_,draw.buffered,draw,snapshot))return false;
        const size_t before=trailPuffs_.size();
        if(!refinement_trail::extract(draw,fvf,world,{1,1,1,1},trailPuffs_))return false;
        auto operation=[](float op,float a,float b){return std::clamp(op<2.5f?a:op<3.5f?b:a*b*(op>5.5f?4:op>4.5f?2:1),0.f,1.f);};
        for(size_t i=before;i<trailPuffs_.size();++i){
            auto &color=trailPuffs_[i].color;const auto diffuse=color;
            auto arg=[&](float value,int channel){return value==2?1.f:value==3?c[3][channel]:value==6?c[4][channel]:diffuse[channel];};
            for(int channel=0;channel<3;++channel)color[channel]=operation(c[1][0],arg(c[1][1],channel),arg(c[1][2],channel));
            color[3]=operation(c[1][3],arg(c[2][0],3),arg(c[2][1],3));
        }
        trailPoints_+=unsigned(trailPuffs_.size()-before);
        if(!trailContext_){
            trailContext_=context;context->consumer=this;
            context->finish=[](void *owner){static_cast<SfxFX *>(owner)->finishTrail();};
            texture->AddRef();trailArt_.p=texture;trailSource_=src;
            DWORD range=0;device_->GetRenderState(D3DRS_RANGEFOGENABLE,&range);trailRange_=range!=0;
            std::memcpy(trailConstants_,c,sizeof(trailConstants_));
        }
        return true;
    }
    Com<IDirect3DTexture9> background_;
    Com<IDirect3DSurface9> backgroundSurface_;
    UINT width_ = 0, height_ = 0;
    bool camera_ = false, snapshot_ = false, used_ = false;
    unsigned draws_ = 0;
    D3DMATRIX view_{}, projection_{};
    D3DVIEWPORT9 viewport_{};
    // Material replacement never changes targets, transforms, streams or the vertex declaration.
    // A full D3DSBT_ALL block per particle restores render targets and stalls the driver.
    bool active_ = false, wakeState_ = false;
    float savedVertex_[13][4]{}, savedPixel_[11][4]{};
    DWORD savedFog_ = 0, savedFilters_[2]{}, savedWakeSampler_[4]{}, savedDestination_ = 0;
    Com<IDirect3DBaseTexture9> savedTexture_;
    static constexpr D3DSAMPLERSTATETYPE filters_[] = {D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER};
    static constexpr D3DSAMPLERSTATETYPE wakeSampler_[] = {D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER,
                                                           D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV};
    bool saveState(bool wake) {
        if (FAILED(device_->GetVertexShaderConstantF(0, savedVertex_[0], 13)) ||
            FAILED(device_->GetPixelShaderConstantF(0, savedPixel_[0], 11)) ||
            FAILED(device_->GetRenderState(D3DRS_FOGENABLE, &savedFog_)))
            return false;
        for (int i = 0; i < 2; ++i)
            if (FAILED(device_->GetSamplerState(0, filters_[i], savedFilters_ + i)))
                return false;
        if (wake) {
            if (FAILED(device_->GetTexture(1, savedTexture_.out())) ||
                FAILED(device_->GetRenderState(D3DRS_DESTBLEND, &savedDestination_)))
                return false;
            for (int i = 0; i < 4; ++i)
                if (FAILED(device_->GetSamplerState(1, wakeSampler_[i], savedWakeSampler_ + i))) {
                    savedTexture_.out();
                    return false;
                }
        }
        wakeState_ = wake;
        active_ = true;
        return true;
    }
    static D3DMATRIX multiply(const D3DMATRIX &a, const D3DMATRIX &b) {
        D3DMATRIX result{};
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                for (int k = 0; k < 4; ++k)
                    result.m[r][c] += a.m[r][k] * b.m[k][c];
        return result;
    }
    unsigned classify(IDirect3DTexture9 *texture) {
        DWORD kind = 0, size = 4;
        if (SUCCEEDED(texture->GetPrivateData(SfxTag, &kind, &size)) && size == 4)
            return kind == 4 ? 0 : kind;
        D3DSURFACE_DESC desc{};
        if (FAILED(texture->GetLevelDesc(0, &desc)) || desc.Pool != D3DPOOL_MANAGED || desc.Usage)
            return 0;
        UINT rows = 0, bytes = 0;
        if (desc.Format == D3DFMT_A8R8G8B8 || desc.Format == D3DFMT_X8R8G8B8) {
            rows = desc.Height;
            bytes = desc.Width * 4;
        } else if (desc.Format == D3DFMT_A1R5G5B5 || desc.Format == D3DFMT_A4R4G4B4) {
            rows = desc.Height;
            bytes = desc.Width * 2;
        } else if (desc.Format == D3DFMT_DXT1 || desc.Format == D3DFMT_DXT3 || desc.Format == D3DFMT_DXT5) {
            rows = (desc.Height + 3) / 4;
            bytes = (desc.Width + 3) / 4 * (desc.Format == D3DFMT_DXT1 ? 8 : 16);
        } else {
            kind = 4;
            texture->SetPrivateData(SfxTag, &kind, 4, 0);
            return 0;
        }
        if (!hash_)
            BCryptOpenAlgorithmProvider(&hash_, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        D3DLOCKED_RECT lock{};
        if (!hash_ || FAILED(texture->LockRect(0, &lock, nullptr, D3DLOCK_READONLY)))
            return 0;
        BCRYPT_HASH_HANDLE hash = nullptr;
        unsigned char digest[32]{};
        bool ok = BCryptCreateHash(hash_, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
        for (UINT row = 0; ok && row < rows; ++row)
            ok = BCryptHashData(hash, (PUCHAR)lock.pBits + row * lock.Pitch, bytes, 0) >= 0;
        ok = ok && BCryptFinishHash(hash, digest, 32, 0) >= 0;
        if (hash)
            BCryptDestroyHash(hash);
        texture->UnlockRect(0);
        if (!ok)
            return 0;
        size_t lo = 0, hi = sizeof(SfxIdentities) / sizeof(SfxIdentities[0]);
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            if (memcmp(SfxIdentities[mid].hash, digest, 32) < 0)
                lo = mid + 1;
            else
                hi = mid;
        }
        kind = lo < sizeof(SfxIdentities) / sizeof(SfxIdentities[0]) &&
                       !memcmp(SfxIdentities[lo].hash, digest, 32)
                   ? SfxIdentities[lo].kind
                   : 4;
        texture->SetPrivateData(SfxTag, &kind, 4, 0);
        return kind == 4 ? 0 : kind;
    }

  public:
    explicit SfxFX(IDirect3DDevice9 *d) : device_(d) {}
    ~SfxFX() {
        native_refinement::request(this,false);
        invalidate();
        if (hash_)
            BCryptCloseAlgorithmProvider(hash_, 0);
    }
    void invalidate() {
        end();
        vs_.out();
        ps_.out();
        auraPS_.out();
        refinementPS_.out();
        cloudPS_.out();discardPS_.out();trailArt_.out();
        if(trailContext_){trailContext_->consumer=nullptr;trailContext_->finish=nullptr;trailContext_=nullptr;}
        trailPuffs_.clear();trailVertices_.clear();
        backgroundSurface_.out();
        background_.out();
        width_ = height_ = 0;
        camera_ = snapshot_ = used_ = false;
    }
    void beginFrame(bool enabled,bool detailedRefinement=false) {
        native_refinement::request(this,enabled&&detailedRefinement);
        if (!enabled || !used_) {
            backgroundSurface_.out();
            background_.out();
            width_ = height_ = 0;
        }
        camera_ = snapshot_ = used_ = false;
        draws_ = 0;
        trailPoints_=0;
    }
    size_t bytes() const {
        return background_.p ? size_t(width_) * height_ * 4 : 0;
    }
    void observe() {
        if (camera_)
            return;
        DWORD write = 0;
        device_->GetRenderState(D3DRS_ZWRITEENABLE, &write);
        if (!write)
            return;
        D3DMATRIX v{}, p{};
        D3DVIEWPORT9 vp{};
        if (FAILED(device_->GetTransform(D3DTS_VIEW, &v)) ||
            FAILED(device_->GetTransform(D3DTS_PROJECTION, &p)) || FAILED(device_->GetViewport(&vp)) ||
            std::abs(p._34 - 1) > .001f || std::abs(p._44) > .001f || p._11 <= 0 || p._22 <= 0)
            return;
        view_ = v;
        projection_ = p;
        viewport_ = vp;
        camera_ = true;
    }
    bool snapshotWorld(bool refresh = false) {
        if (refresh)
            snapshot_ = false;
        if (snapshot_)
            return true;
        Com<IDirect3DSurface9> target;
        D3DSURFACE_DESC desc{};
        if (FAILED(device_->GetRenderTarget(0, target.out())) || FAILED(target->GetDesc(&desc)))
            return false;
        if (background_.p && (width_ != desc.Width || height_ != desc.Height)) {
            backgroundSurface_.out();
            background_.out();
        }
        if (!background_.p &&
            (FAILED(device_->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format,
                                           D3DPOOL_DEFAULT, background_.out(), nullptr)) ||
             FAILED(background_->GetSurfaceLevel(0, backgroundSurface_.out())))) {
            backgroundSurface_.out();
            background_.out();
            return false;
        }
        width_ = desc.Width;
        height_ = desc.Height;
        snapshot_ =
            SUCCEEDED(device_->StretchRect(target, nullptr, backgroundSurface_, nullptr, D3DTEXF_NONE));
        used_ |= snapshot_;
        return snapshot_;
    }
    bool begin(const Settings &s, bool waterVisible, bool canSnapshot = true,ParticleDraw draw={}) {
        if ((!s.sfx && !(s.water && s.sfxWake > 0 && waterVisible)) || !camera_ || active_)
            return false;
        DWORD fvf = 0, z = 0, write = 0, blend = 0, src = 0, dst = 0, lit = 0, test = 0, stage1 = 0,
              coord = 0, transform = 0;
        device_->GetFVF(&fvf);
        if ((fvf & D3DFVF_POSITION_MASK) != D3DFVF_XYZ || ((fvf & D3DFVF_TEXCOUNT_MASK) != D3DFVF_TEX1))
            return false;
        device_->GetRenderState(D3DRS_ZENABLE, &z);
        device_->GetRenderState(D3DRS_ZWRITEENABLE, &write);
        device_->GetRenderState(D3DRS_ALPHABLENDENABLE, &blend);
        device_->GetRenderState(D3DRS_SRCBLEND, &src);
        device_->GetRenderState(D3DRS_DESTBLEND, &dst);
        device_->GetRenderState(D3DRS_LIGHTING, &lit);
        device_->GetRenderState(D3DRS_ALPHATESTENABLE, &test);
        if (write || !blend || lit || test || (src != D3DBLEND_SRCALPHA && src != D3DBLEND_ONE) ||
            (dst != D3DBLEND_ONE && !(dst == D3DBLEND_INVSRCALPHA && src == D3DBLEND_SRCALPHA)))
            return false;
        DWORD blendOp = 0, separate = 0;
        device_->GetRenderState(D3DRS_BLENDOP, &blendOp);
        device_->GetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, &separate);
        if (blendOp != D3DBLENDOP_ADD || separate)
            return false;
        Com<IDirect3DPixelShader9> original;
        device_->GetPixelShader(original.out());
        if (original.p)
            return false;
        Com<IDirect3DVertexShader9> originalVertex;
        device_->GetVertexShader(originalVertex.out());
        if (originalVertex.p)
            return false;
        Com<IDirect3DBaseTexture9> base;
        if (FAILED(device_->GetTexture(0, base.out())) || !base.p || base->GetType() != D3DRTYPE_TEXTURE)
            return false;
        auto *texture = static_cast<IDirect3DTexture9 *>(base.p);
        unsigned kind = classify(texture);
        if (!kind || (!s.sfx && kind != 3) || (!z && kind != 5))
            return false;
        D3DMATRIX world{}, view{}, projection{}, uv{};
        D3DVIEWPORT9 vp{};
        device_->GetTransform(D3DTS_WORLD, &world);
        device_->GetTransform(D3DTS_VIEW, &view);
        device_->GetTransform(D3DTS_PROJECTION, &projection);
        device_->GetTransform(D3DTS_TEXTURE0, &uv);
        device_->GetViewport(&vp);
        if (vp.X != viewport_.X || vp.Y != viewport_.Y || vp.Width != viewport_.Width ||
            vp.Height != viewport_.Height)
            return false;
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) {
                if (std::abs(projection.m[r][c] - projection_.m[r][c]) > .00001f)
                    return false;
                if (r < 3 && c < 3 && std::abs(view.m[r][c] - view_.m[r][c]) > .00001f)
                    return false;
            }
        device_->GetTextureStageState(1, D3DTSS_COLOROP, &stage1);
        device_->GetTextureStageState(0, D3DTSS_TEXCOORDINDEX, &coord);
        device_->GetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, &transform);
        if (stage1 != D3DTOP_DISABLE || coord ||
            (transform != D3DTTFF_DISABLE && transform != D3DTTFF_COUNT2))
            return false;
        DWORD operations[6]{}, factor = 0, constant = 0;
        const D3DTEXTURESTAGESTATETYPE keys[] = {D3DTSS_COLOROP, D3DTSS_COLORARG1, D3DTSS_COLORARG2,
                                                 D3DTSS_ALPHAOP, D3DTSS_ALPHAARG1, D3DTSS_ALPHAARG2};
        for (int i = 0; i < 6; ++i)
            device_->GetTextureStageState(0, keys[i], operations + i);
        if (operations[0] < D3DTOP_SELECTARG1 || operations[0] > D3DTOP_ADD || operations[3] > D3DTOP_ADD)
            return false;
        for (int i : {1, 2, 4, 5})
            if (((operations[i] & 15) > 3 && (operations[i] & 15) != 6) || operations[i] > 63)
                return false;
        device_->GetRenderState(D3DRS_TEXTUREFACTOR, &factor);
        device_->GetTextureStageState(0, D3DTSS_CONSTANT, &constant);
        float scale = std::sqrt(world._11 * world._11 + world._12 * world._12 + world._13 * world._13);
        bool wake = kind == 3 && waterVisible && fvf == 0x102 && std::abs(world._22) < .001f &&
                    std::abs(world._32) > .01f && src == D3DBLEND_SRCALPHA && dst == D3DBLEND_ONE &&
                    operations[0] == D3DTOP_SELECTARG1 && operations[1] == D3DTA_TEXTURE &&
                    operations[3] == D3DTOP_MODULATE && operations[4] == D3DTA_DIFFUSE &&
                    operations[5] == D3DTA_DIFFUSE;
        if (kind == 3 && !wake)
            kind = 1;
        if (!wake && !s.sfx)
            return false;
        if (wake && (s.sfxWake <= 0 || (!snapshot_ && !canSnapshot) || !snapshotWorld()))
            return false;
        const bool aura = kind == 6 || kind == 7 || kind == 8;
        const bool refinement = kind == 7 && s.sceneQuality && s.sfxAura > 0 &&
                                (!transform || std::abs(uv._12) + std::abs(uv._21) < .00001f);
        if ((!vs_.p && FAILED(device_->CreateVertexShader(ShaderSfxVS, vs_.out()))) ||
            (refinement && !refinementPS_.p &&
             FAILED(device_->CreatePixelShader(ShaderRefinementPS, refinementPS_.out()))) ||
            (aura ? (!auraPS_.p && FAILED(device_->CreatePixelShader(ShaderAuraPS, auraPS_.out()))) :
                         (!ps_.p && FAILED(device_->CreatePixelShader(ShaderSfxPS, ps_.out())))))
            return false;
        if (!saveState(wake))
            return false;
        auto worldView = multiply(world, view), clip = multiply(worldView, projection);
        DWORD range = 0;
        device_->GetRenderState(D3DRS_RANGEFOGENABLE, &range);
        float layout[] = {fvf & D3DFVF_DIFFUSE ? 1.f : 0.f, transform ? 1.f : 0.f, range ? 1.f : 0.f, 0};
        float c[11][4]{};
        c[0][0] = s.sfxIntensity;
        c[0][1] = s.sfxGlow;
        c[0][2] = s.sfxSaturation;
        c[0][3] = s.sfxSoftness;
        c[1][0] = float(operations[0]);
        c[1][1] = float(operations[1]);
        c[1][2] = float(operations[2]);
        c[1][3] = float(operations[3]);
        c[2][0] = float(operations[4]);
        c[2][1] = float(operations[5]);
        c[2][2] = src == D3DBLEND_SRCALPHA ? 1.f : 0.f;
        c[2][3] = s.sceneQuality ? 1.f : 0.f;
        auto rgba = [](DWORD value, float *out) {
            for (int i = 0; i < 3; ++i)
                out[i] = float((value >> (16 - i * 8)) & 255) / 255;
            out[3] = float(value >> 24) / 255;
        };
        rgba(factor, c[3]);
        rgba(constant, c[4]);
        c[5][2] = c[5][3] = 1;
        if (transform) {
            if (std::abs(uv._12) + std::abs(uv._21) < .00001f) {
                c[5][0] = std::min(uv._31, uv._31 + uv._11);
                c[5][1] = std::min(uv._32, uv._32 + uv._22);
                c[5][2] = std::max(uv._31, uv._31 + uv._11);
                c[5][3] = std::max(uv._32, uv._32 + uv._22);
            } else
                c[0][1] = c[0][3] = 0;
        }
        D3DSURFACE_DESC desc{};
        texture->GetLevelDesc(0, &desc);
        c[6][0] = 1.f / desc.Width;
        c[6][1] = 1.f / desc.Height;
        c[6][2] = float(kind);
        c[6][3] = aura ? 1.f : wake ? 1.f / (1 + scale * .45f) :
                                               1.f / (1 + std::max(0.f, float(draws_) - 64) * .015f);
        c[7][0] = width_ ? 1.f / width_ : 0;
        c[7][1] = height_ ? 1.f / height_ : 0;
        c[7][2] = snapshot_ ? 1.f : 0;
        c[7][3] = kind == 5 ? float(GetTickCount64() % 3600000) * .001f : s.sfxWake;
        if (kind == 7)
            c[7][3] = float(GetTickCount64() % 120000) * (6.28318530718f / 120000);
        DWORD fog = 0, mode = 0, packed = 0, start = 0, end = 0, density = 0;
        device_->GetRenderState(D3DRS_FOGENABLE, &fog);
        device_->GetRenderState(D3DRS_FOGTABLEMODE, &mode);
        if (!mode)
            device_->GetRenderState(D3DRS_FOGVERTEXMODE, &mode);
        device_->GetRenderState(D3DRS_FOGCOLOR, &packed);
        device_->GetRenderState(D3DRS_FOGSTART, &start);
        device_->GetRenderState(D3DRS_FOGEND, &end);
        device_->GetRenderState(D3DRS_FOGDENSITY, &density);
        rgba(packed, c[8]);
        c[9][0] = fog && mode ? 1.f : 0.f;
        c[9][1] = float(mode);
        memcpy(&c[9][2], &start, 4);
        memcpy(&c[9][3], &end, 4);
        memcpy(c[10], &density, 4);
        c[10][1] = aura ? s.sfxAura : 0;
        if((kind==7&&s.sfxAuraCustomColor)||(kind==8&&s.sfxWeaponCustomColor)){
            c[10][2]=1;
            c[7][0]=kind==8?s.sfxWeaponR:s.sfxAuraR;
            c[7][1]=kind==8?s.sfxWeaponG:s.sfxAuraG;
            c[7][2]=kind==8?s.sfxWeaponB:s.sfxAuraB;
        }
        device_->SetVertexShader(vs_);
        const bool fullTexture=std::abs(c[5][0])+std::abs(c[5][1])+std::abs(c[5][2]-1)+std::abs(c[5][3]-1)<.00001f;
        const bool cloud=refinement&&fullTexture&&captureTrail(draw,fvf,world,texture,c,src,dst);
        device_->SetPixelShader(cloud?discardPS_.p:refinement ? refinementPS_.p : aura ? auraPS_.p : ps_.p);
        device_->SetVertexShaderConstantF(0, &clip.m[0][0], 4);
        device_->SetVertexShaderConstantF(4, &uv.m[0][0], 4);
        device_->SetVertexShaderConstantF(8, &worldView.m[0][0], 4);
        device_->SetVertexShaderConstantF(12, layout, 1);
        device_->SetPixelShaderConstantF(0, c[0], 11);
        device_->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device_->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device_->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        if (wake) {
            device_->SetTexture(1, background_);
            device_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            for (auto key : {D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER})
                device_->SetSamplerState(1, key, D3DTEXF_LINEAR);
            device_->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            device_->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        }
        ++draws_;
        return true;
    }
    void end(bool up = false, bool indexed = false) {
        if (!active_)
            return;
        device_->SetVertexShader(nullptr);
        device_->SetPixelShader(nullptr);
        device_->SetVertexShaderConstantF(0, savedVertex_[0], 13);
        device_->SetPixelShaderConstantF(0, savedPixel_[0], 11);
        device_->SetRenderState(D3DRS_FOGENABLE, savedFog_);
        for (int i = 0; i < 2; ++i)
            device_->SetSamplerState(0, filters_[i], savedFilters_[i]);
        if (wakeState_) {
            device_->SetTexture(1, savedTexture_);
            savedTexture_.out();
            device_->SetRenderState(D3DRS_DESTBLEND, savedDestination_);
            for (int i = 0; i < 4; ++i)
                device_->SetSamplerState(1, wakeSampler_[i], savedWakeSampler_[i]);
        }
        // Draw*UP already performs the native stream/index clearing. No unrelated state is rebound.
        active_ = wakeState_ = false;
    }
};
