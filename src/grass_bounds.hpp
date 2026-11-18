#pragma once
#include "common.hpp"
#include <cstring>

// Read positions only while the client owns a writable Lock pointer. Never lock
// a WRITEONLY buffer for reading or copy terrain back from the GPU.
namespace grass_bounds {
inline constexpr GUID Tag={0x3477f811,0x2bde,0x4743,{0xaa,0x62,0x31,0x56,0x0d,0xe2,0x77,0x41}};
constexpr unsigned Stride=40,Block=81; // Native 9x9 terrain patches, FVF 0x212.
struct Box {float lo[3]{},hi[3]{};bool valid=false;};
class Bounds final:public IUnknown {
    std::atomic<ULONG> references_{1};
public:
    std::vector<Box> boxes;
    explicit Bounds(unsigned vertices):boxes((vertices+Block-1)/Block){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void **out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(iid!=IID_IUnknown)return E_NOINTERFACE;*out=this;AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return ++references_;}
    ULONG STDMETHODCALLTYPE Release() override {auto n=--references_;if(!n)delete this;return n;}
};
inline Bounds *get(IDirect3DVertexBuffer9 *buffer){
    Bounds *p=nullptr;DWORD size=sizeof(p);
    return SUCCEEDED(buffer->GetPrivateData(Tag,&p,&size))&&size==sizeof(p)?p:nullptr;
}
struct Write {
    const unsigned char *data=nullptr;UINT offset=0,bytes=0;bool tracked=false;
    void begin(IDirect3DVertexBuffer9 *buffer,UINT first,UINT size,void *pointer,DWORD flags,bool enabled){
        data=nullptr;
        if(flags&D3DLOCK_READONLY)return;
        if(!enabled){if(tracked)buffer->FreePrivateData(Tag);tracked=false;return;}
        D3DVERTEXBUFFER_DESC desc{};
        if(FAILED(buffer->GetDesc(&desc))||desc.Size>8*1024*1024||desc.Size%(Stride*Block)||
            (desc.FVF&&desc.FVF!=0x212)||desc.Pool!=D3DPOOL_MANAGED||first>desc.Size)return;
        if(flags&D3DLOCK_DISCARD)buffer->FreePrivateData(Tag);
        tracked=true;offset=first;bytes=size?size:desc.Size-first;
        if(!pointer||bytes>desc.Size-first||first%Stride||bytes%Stride){buffer->FreePrivateData(Tag);return;}
        data=static_cast<const unsigned char *>(pointer);
    }
    void finish(IDirect3DVertexBuffer9 *buffer){
        if(!data)return;
        D3DVERTEXBUFFER_DESC desc{};if(FAILED(buffer->GetDesc(&desc))){data=nullptr;return;}
        Com<Bounds> bounds;bounds.p=get(buffer);
        if(!bounds.p){bounds.p=new Bounds(desc.Size/Stride);
            if(FAILED(buffer->SetPrivateData(Tag,bounds.p,sizeof(IUnknown *),D3DSPD_IUNKNOWN))){data=nullptr;return;}}
        const unsigned first=offset/Stride,end=first+bytes/Stride;
        for(unsigned b=first/Block;b<(end+Block-1)/Block&&b<bounds->boxes.size();++b){
            auto &box=bounds->boxes[b];box.valid=false;
            const unsigned lo=b*Block,hi=std::min((b+1)*Block,desc.Size/Stride);
            if(lo<first||hi>end)continue;
            for(int axis=0;axis<3;++axis){box.lo[axis]=1e30f;box.hi[axis]=-1e30f;}
            bool valid=true;
            for(unsigned v=lo;v<hi&&valid;++v){float p[3];memcpy(p,data+(v-first)*Stride,sizeof(p));
                for(int axis=0;axis<3;++axis){if(!std::isfinite(p[axis])||std::abs(p[axis])>1e7f){valid=false;break;}
                    box.lo[axis]=std::min(box.lo[axis],p[axis]);box.hi[axis]=std::max(box.hi[axis],p[axis]);}}
            box.valid=valid;
        }
        data=nullptr;
    }
};
inline bool outside(IDirect3DVertexBuffer9 *buffer,UINT offset,UINT stride,int64_t first,UINT count,
                    const D3DMATRIX &world,const float *map){
    if(stride!=Stride||offset%Stride||first<0||!count)return false;
    Com<Bounds> bounds;bounds.p=get(buffer);if(!bounds.p)return false;
    const uint64_t begin=uint64_t(first)+offset/Stride,end=begin+count;
    if(end>uint64_t(bounds->boxes.size())*Block)return false;
    float low[3]={1e30f,1e30f,1e30f},high[3]={-1e30f,-1e30f,-1e30f};
    for(size_t b=size_t(begin/Block);b<size_t((end+Block-1)/Block);++b){const auto &box=bounds->boxes[b];
        if(!box.valid)return false;
        for(int axis=0;axis<3;++axis){low[axis]=std::min(low[axis],box.lo[axis]);high[axis]=std::max(high[axis],box.hi[axis]);}}
    if(world._14||world._24||world._34||world._44!=1)return false;
    float minX=1e30f,maxX=-1e30f,minZ=1e30f,maxZ=-1e30f;
    for(unsigned c=0;c<8;++c){const float x=c&1?high[0]:low[0],y=c&2?high[1]:low[1],z=c&4?high[2]:low[2];
        const float wx=x*world._11+y*world._21+z*world._31+world._41,wz=x*world._13+y*world._23+z*world._33+world._43;
        if(!std::isfinite(wx)||!std::isfinite(wz))return false;
        minX=std::min(minX,wx);maxX=std::max(maxX,wx);minZ=std::min(minZ,wz);maxZ=std::max(maxZ,wz);}
    const float radius=map[2]+1;
    return minX>map[0]+radius||maxX<map[0]-radius||minZ>map[1]+radius||maxZ<map[1]-radius;
}
}
