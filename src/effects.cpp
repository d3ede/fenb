#include "effects.hpp"
#include <generated/shaders.hpp>

bool Effects::makeTexture(UINT w, UINT h, IDirect3DTexture9** t, IDirect3DSurface9** s) {
    if (FAILED(device_->CreateTexture(w,h,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,t,nullptr))) return false;
    if (FAILED((*t)->GetSurfaceLevel(0,s))) { Drop(*t); return false; }
    return true;
}
bool Effects::shaders() {
    if (shaderFailure_) return false;
    if (bright_ && blur_ && composite_) return true;
    if (FAILED(device_->CreatePixelShader(ShaderBright,&bright_)) ||
        FAILED(device_->CreatePixelShader(ShaderBlur,&blur_)) ||
        FAILED(device_->CreatePixelShader(ShaderComposite,&composite_))) {
        Drop(bright_); Drop(blur_); Drop(composite_); shaderFailure_=true;
        Log("Effects disabled: Shader Model 3.0 shaders unavailable."); return false;
    }
    return true;
}
void Effects::invalidate() {
    Drop(frameSurface_); Drop(frame_);
    for (int i=0;i<2;++i) { Drop(bloomSurface_[i]); Drop(bloom_[i]); }
    Drop(bright_); Drop(blur_); Drop(composite_);
    width_=height_=bw_=bh_=0; bytes_=0; scale_=0; shaderFailure_=false;
}
bool Effects::prepare(IDirect3DSurface9* backbuffer, const Settings& settings) {
    if (!settings.enabled || (!settings.bloom && !settings.color)) { if(frame_) invalidate(); return false; }
    D3DSURFACE_DESC desc{}; if (FAILED(backbuffer->GetDesc(&desc))) return false;
    int scale = settings.bloom ? settings.bloomScale : 0;
    if (width_ != desc.Width || height_ != desc.Height || scale_ != scale) {
        invalidate(); width_=desc.Width; height_=desc.Height; scale_=scale;
        if (!makeTexture(width_,height_,&frame_,&frameSurface_)) { invalidate(); return false; }
        bytes_ = size_t(width_)*height_*4;
        if (scale) {
            bw_=std::max(1u,width_/scale); bh_=std::max(1u,height_/scale);
            for (int i=0;i<2;++i) if (!makeTexture(bw_,bh_,&bloom_[i],&bloomSurface_[i])) { invalidate(); return false; }
            bytes_ += size_t(bw_)*bh_*8;
        }
    }
    if (!shaders()) return false;
    // Color-only resolve. BeginScene restrictions apply to depth/stencil copies,
    // which FENB does not perform. This also supports the pre-HUD scene boundary.
    return SUCCEEDED(device_->StretchRect(backbuffer,nullptr,frameSurface_,nullptr,D3DTEXF_NONE));
}
void Effects::pass(IDirect3DSurface9* target, UINT w, UINT h, IDirect3DPixelShader9* shader,
                   IDirect3DTexture9* input, IDirect3DTexture9* second) {
    // Unbind samplers BEFORE a texture becomes a render target (ping-pong hazard).
    device_->SetTexture(0,nullptr); device_->SetTexture(1,nullptr);
    device_->SetRenderTarget(0,target);
    D3DVIEWPORT9 vp{0,0,w,h,0,1}; device_->SetViewport(&vp);
    device_->SetPixelShader(shader); device_->SetTexture(0,input); device_->SetTexture(1,second);
    struct Vertex { float x,y,z,rhw,u,v; };
    // D3D9 pixel centers require the half-pixel correction.
    Vertex v[]={{-.5f,-.5f,0,1,0,0},{float(w)-.5f,-.5f,0,1,1,0},
                {-.5f,float(h)-.5f,0,1,0,1},{float(w)-.5f,float(h)-.5f,0,1,1,1}};
    device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex));
}
void Effects::draw(IDirect3DSurface9* backbuffer, const Settings& s) {
    device_->SetVertexShader(nullptr); device_->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);
    device_->SetRenderState(D3DRS_ZENABLE,FALSE); device_->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
    device_->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE); device_->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    device_->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
    device_->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE); device_->SetRenderState(D3DRS_FOGENABLE,FALSE);
    device_->SetRenderState(D3DRS_LIGHTING,FALSE); device_->SetRenderState(D3DRS_STENCILENABLE,FALSE);
    device_->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE); device_->SetRenderState(D3DRS_CLIPPING,FALSE);
    device_->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
    device_->SetRenderState(D3DRS_COLORWRITEENABLE,15); device_->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);
    for (DWORD i=0;i<2;++i) {
        device_->SetSamplerState(i,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
        device_->SetSamplerState(i,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
        device_->SetSamplerState(i,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
        device_->SetSamplerState(i,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
        device_->SetSamplerState(i,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
        device_->SetSamplerState(i,D3DSAMP_SRGBTEXTURE,FALSE);
    }
    float c[5][4]={{s.bloomStrength,std::pow(s.threshold,2.2f),s.knee,s.radius},
        {s.exposure,s.contrast,s.saturation,s.gamma}, {s.temperature,s.tint,s.shadows,s.highlights},
        {s.bloom?1.f:0.f,s.color?1.f:0.f,0,0},{0,0,0,0}};
    device_->SetPixelShaderConstantF(0,&c[0][0],5);
    if (s.bloom) {
        float step[4]={1.f/width_,1.f/height_,0,0}; device_->SetPixelShaderConstantF(4,step,1);
        pass(bloomSurface_[0],bw_,bh_,bright_,frame_);
        step[0]=1.f/bw_; step[1]=0; device_->SetPixelShaderConstantF(4,step,1);
        pass(bloomSurface_[1],bw_,bh_,blur_,bloom_[0]);
        step[0]=0; step[1]=1.f/bh_; device_->SetPixelShaderConstantF(4,step,1);
        pass(bloomSurface_[0],bw_,bh_,blur_,bloom_[1]);
    }
    pass(backbuffer,width_,height_,composite_,frame_,bloom_[0]);
    device_->SetTexture(0,nullptr); device_->SetTexture(1,nullptr);
}
