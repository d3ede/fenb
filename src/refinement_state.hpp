#pragma once
#include "common.hpp"

// A mist batch changes only these bindings. Avoid a full state block, render
// target rebinding and depth-surface rebinding for every character in a crowd.
class RefinementState {
    IDirect3DDevice9 *device_;
    Com<IDirect3DVertexDeclaration9> declaration_;
    Com<IDirect3DVertexShader9> vertex_;
    Com<IDirect3DPixelShader9> pixel_;
    Com<IDirect3DBaseTexture9> texture_;
    Com<IDirect3DVertexBuffer9> stream_;
    UINT offset_=0,stride_=0;
    DWORD fvf_=0,values_[10]{};
    float vs_[13][4]{},ps_[11][4]{};
    D3DVIEWPORT9 viewport_{};
    bool valid_=false;
    static constexpr D3DRENDERSTATETYPE states_[]={D3DRS_FOGENABLE,D3DRS_ZENABLE,
        D3DRS_ZWRITEENABLE,D3DRS_ALPHATESTENABLE,D3DRS_CULLMODE,D3DRS_ALPHABLENDENABLE,
        D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_BLENDOP,D3DRS_SEPARATEALPHABLENDENABLE};
public:
    explicit RefinementState(IDirect3DDevice9 *device):device_(device){
        if(FAILED(device->GetFVF(&fvf_))||FAILED(device->GetVertexDeclaration(declaration_.out()))||
           FAILED(device->GetVertexShader(vertex_.out()))||FAILED(device->GetPixelShader(pixel_.out()))||
           FAILED(device->GetTexture(0,texture_.out()))||
           FAILED(device->GetStreamSource(0,stream_.out(),&offset_,&stride_))||
           FAILED(device->GetVertexShaderConstantF(0,vs_[0],13))||
           FAILED(device->GetPixelShaderConstantF(0,ps_[0],11))||FAILED(device->GetViewport(&viewport_)))return;
        for(unsigned i=0;i<std::size(states_);++i)
            if(FAILED(device->GetRenderState(states_[i],&values_[i])))return;
        valid_=true;
    }
    ~RefinementState(){
        if(!valid_)return;
        if(fvf_)device_->SetFVF(fvf_);else device_->SetVertexDeclaration(declaration_);
        device_->SetVertexShader(vertex_);device_->SetPixelShader(pixel_);
        device_->SetTexture(0,texture_);device_->SetStreamSource(0,stream_,offset_,stride_);
        device_->SetVertexShaderConstantF(0,vs_[0],13);device_->SetPixelShaderConstantF(0,ps_[0],11);
        for(unsigned i=0;i<std::size(states_);++i)device_->SetRenderState(states_[i],values_[i]);
        device_->SetViewport(&viewport_);
    }
    bool valid()const{return valid_;}
};
