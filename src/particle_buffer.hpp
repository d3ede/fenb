#pragma once
#include "draw_call.hpp"
#include <array>

// The native SFX fan is an 80-byte WRITEONLY system-memory buffer. Copy its
// writable upload before Unlock; never lock a GPU/WRITEONLY buffer for reading.
namespace particle_buffer {
inline constexpr GUID Tag={0xa7659301,0x44da,0x4c61,{0x8b,0x69,0x11,0x9d,0x4b,0x29,0x9c,0xe2}};
struct Snapshot { UINT size=0;std::array<BYTE,96> bytes{}; };
struct Write {
    const void *data=nullptr;UINT size=0;bool candidate=false;
    explicit Write(IDirect3DVertexBuffer9 *buffer){
        D3DVERTEXBUFFER_DESC d{};
        candidate=SUCCEEDED(buffer->GetDesc(&d))&&
            ((d.FVF==0x102&&d.Size==80)||(d.FVF==0x142&&d.Size==96));
    }
    void begin(IDirect3DVertexBuffer9 *buffer,UINT offset,UINT bytes,void *pointer,DWORD flags){
        data=nullptr;if(!candidate||flags&D3DLOCK_READONLY)return;
        buffer->FreePrivateData(Tag);
        D3DVERTEXBUFFER_DESC d{};
        if(SUCCEEDED(buffer->GetDesc(&d))&&offset==0&&(!bytes||bytes==d.Size)){
            data=pointer;size=d.Size;
        }
    }
    void finish(IDirect3DVertexBuffer9 *buffer){
        if(!data)return;
        Snapshot snapshot;snapshot.size=size;std::memcpy(snapshot.bytes.data(),data,size);data=nullptr;
        buffer->SetPrivateData(Tag,&snapshot,sizeof(snapshot),0);
    }
};
inline bool read(IDirect3DDevice9 *device,const SurfaceDraw &draw,ParticleDraw &out,Snapshot &snapshot){
    if(draw.indexed||draw.first||draw.primitives!=2||
       (draw.type!=D3DPT_TRIANGLEFAN&&draw.type!=D3DPT_TRIANGLESTRIP))return false;
    Com<IDirect3DVertexBuffer9> buffer;UINT offset=0,stride=0;DWORD bytes=sizeof(snapshot);
    if(FAILED(device->GetStreamSource(0,buffer.out(),&offset,&stride))||!buffer.p||offset||
       FAILED(buffer->GetPrivateData(Tag,&snapshot,&bytes))||bytes!=sizeof(snapshot)||
       (stride!=20&&stride!=24)||snapshot.size!=stride*4)return false;
    out={draw.type,draw.primitives,snapshot.bytes.data(),stride};return true;
}
}
