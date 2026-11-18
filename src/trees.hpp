#pragma once
#include "settings.hpp"
#include "scene_tags.hpp"
#include "generated/trees.hpp"
#include <bcrypt.h>
#include <cstring>
#include <array>
#include <bit>
#include <cstdint>

// Replaces only an exactly identified native tree draw. No retained game texture cache.
class TreeFX {
    IDirect3DDevice9 *device_;
    std::array<Com<IDirect3DTexture9>,std::size(TreeAssets)> textures_;
    std::array<bool,std::size(TreeAssets)> failed_{};
    Com<IDirect3DBaseTexture9> saved_;
    BCRYPT_ALG_HANDLE hash_=nullptr;
    bool active_=false,enabled_=false,foliage_=false;
    unsigned identify(IDirect3DTexture9 *texture) {
        unsigned id=0;DWORD size=sizeof(id);
        if(SUCCEEDED(texture->GetPrivateData(TreeTag,&id,&size))&&size==sizeof(id))
            return id<=std::size(TreeAssets)?id:0;
        D3DSURFACE_DESC desc{};
        if(FAILED(texture->GetLevelDesc(0,&desc))||desc.Pool!=D3DPOOL_MANAGED||desc.Usage)return 0;
        bool candidate=false;
        for(const auto&a:TreeAssets)candidate|=a.width==desc.Width&&a.height==desc.Height;
        UINT rows=desc.Height,bytes=0;
        if(desc.Format==D3DFMT_A8R8G8B8||desc.Format==D3DFMT_X8R8G8B8)bytes=desc.Width*4;
        else if(desc.Format==D3DFMT_DXT1||desc.Format==D3DFMT_DXT3||desc.Format==D3DFMT_DXT5){
            rows=(desc.Height+3)/4;bytes=(desc.Width+3)/4*(desc.Format==D3DFMT_DXT1?8:16);
        }
        if(candidate&&bytes) {
            if(!hash_&&BCryptOpenAlgorithmProvider(&hash_,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return 0;
            D3DLOCKED_RECT lock{};
            if(FAILED(texture->LockRect(0,&lock,nullptr,D3DLOCK_READONLY)))return 0;
            BCRYPT_HASH_HANDLE hash=nullptr;unsigned char digest[32]{};
            bool ok=BCryptCreateHash(hash_,&hash,nullptr,0,nullptr,0,0)>=0;
            for(UINT row=0;ok&&row<rows;row++)ok=BCryptHashData(hash,(PUCHAR)lock.pBits+row*lock.Pitch,bytes,0)>=0;
            ok=ok&&BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
            if(hash)BCryptDestroyHash(hash);texture->UnlockRect(0);
            if(!ok)return 0;
            size_t lo=0,hi=std::size(TreeIdentities);
            while(lo<hi){const size_t mid=(lo+hi)/2;if(memcmp(TreeIdentities[mid].hash,digest,32)<0)lo=mid+1;else hi=mid;}
            if(lo<std::size(TreeIdentities)&&!memcmp(TreeIdentities[lo].hash,digest,32))id=TreeIdentities[lo].asset+1;
        }
        texture->SetPrivateData(TreeTag,&id,sizeof(id),0);return id;
    }
    IDirect3DTexture9 *replacement(unsigned index) {
        if(textures_[index].p||failed_[index])return textures_[index];
        const auto &a=TreeAssets[index];Com<IDirect3DTexture9> texture;
        if(FAILED(device_->CreateTexture(a.width,a.height,a.levels,0,D3DFORMAT(a.format),D3DPOOL_MANAGED,texture.out(),nullptr))){failed_[index]=true;return nullptr;}
        unsigned width=a.width,height=a.height,offset=a.offset;
        for(unsigned level=0;level<a.levels;level++){
            const unsigned rows=std::max(1u,(height+3)/4),bytes=std::max(1u,(width+3)/4)*(a.format==D3DFMT_DXT1?8:16);
            D3DLOCKED_RECT lock{};
            if(FAILED(texture->LockRect(level,&lock,nullptr,0))){failed_[index]=true;return nullptr;}
            for(unsigned row=0;row<rows;row++)memcpy((BYTE*)lock.pBits+row*lock.Pitch,TreeTextureData+offset+row*bytes,bytes);
            texture->UnlockRect(level);offset+=rows*bytes;width=std::max(1u,width/2);height=std::max(1u,height/2);
        }
        textures_[index].p=texture.p;texture.p=nullptr;return textures_[index];
    }
    unsigned placementAsset(unsigned index) const {
        const unsigned alternate = TreeAssets[index].alternate;
        if (alternate == index) return index;
        D3DMATRIX world{};
        if (FAILED(device_->GetTransform(D3DTS_WORLD, &world))) return index;
        // Hash the static object's translation, independent of view and draw order.
        uint32_t hash = 0;
        for (float coordinate : {world._41, world._42, world._43}) {
            if (!std::isfinite(coordinate)) return index;
            hash = (hash ^ (coordinate == 0.f ? 0u : std::bit_cast<uint32_t>(coordinate))) * 0x9e3779b1u;
        }
        hash ^= hash >> 16;
        hash *= 0x7feb352du;
        hash ^= hash >> 15;
        return hash & 1 ? alternate : index;
    }
  public:
    explicit TreeFX(IDirect3DDevice9 *device):device_(device){}
    ~TreeFX(){invalidate();if(hash_)BCryptCloseAlgorithmProvider(hash_,0);}
    void end(){if(active_){device_->SetTexture(0,saved_);active_=false;saved_.out();}}
    void invalidate(){end();for(auto&t:textures_)t.out();failed_.fill(false);enabled_=false;}
    void beginFrame(bool enabled){if(!enabled&&enabled_)invalidate();enabled_=enabled;}
    IDirect3DTexture9 *nativeFoliage() const {
        return active_ && foliage_ ? static_cast<IDirect3DTexture9 *>(saved_.p) : nullptr;
    }
    bool begin(const Settings&s){
        if (active_ || !s.enabled || !s.trees || (!s.treeBark && !s.treeFoliage)) return false;
        DWORD fvf=0,z=0;D3DMATRIX projection{};
        if(FAILED(device_->GetFVF(&fvf))||(fvf!=0x112&&fvf!=0x212)||
           FAILED(device_->GetRenderState(D3DRS_ZENABLE,&z))||!z||
           FAILED(device_->GetTransform(D3DTS_PROJECTION,&projection))||
           std::abs(projection._34)<.00001f||std::abs(projection._44)>.00001f)return false;
        Com<IDirect3DVertexShader9> vs;Com<IDirect3DPixelShader9> ps;
        if(FAILED(device_->GetVertexShader(vs.out()))||FAILED(device_->GetPixelShader(ps.out()))||vs.p||ps.p)return false;
        Com<IDirect3DBaseTexture9> original;
        if(FAILED(device_->GetTexture(0,original.out()))||!original.p||original->GetType()!=D3DRTYPE_TEXTURE)return false;
        const unsigned id=identify(static_cast<IDirect3DTexture9*>(original.p));
        if(!id)return false;
        const unsigned index = id - 1;
        if (TreeAssets[index].foliage ? !s.treeFoliage : !s.treeBark) return false;
        const unsigned selected = placementAsset(index);
        auto *texture = replacement(selected);
        if (!texture && selected != index) texture = replacement(index);
        if (!texture || FAILED(device_->SetTexture(0,texture))) return false;
        saved_.p=original.p;original.p=nullptr;active_=enabled_=true;
        foliage_=TreeAssets[index].foliage;
        return true;
    }
    size_t bytes()const{size_t bytes=0;for(size_t i=0;i<textures_.size();i++)if(textures_[i].p)bytes+=TreeAssets[i].size;return bytes;}
};
