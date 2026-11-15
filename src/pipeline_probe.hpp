#pragma once
#ifdef FENB_DIAGNOSTICS
#include "common.hpp"
#include <bcrypt.h>
#include <sstream>
class WaterFX;

// Bounded, opt-in graphics-only capture. No process memory, packets or gameplay state.
class PipelineProbe {
    friend struct PipelineProbeFixture;
public:
    explicit PipelineProbe(IDirect3DDevice9* device) : device_(device) {}
    ~PipelineProbe();
    void request() { requested_ = true; surfaceOnly_ = false; }
    void requestSurface() { requested_ = true; surfaceOnly_ = true; }
    void boundary();
    void abort();
    void draw(const char* api, UINT type, UINT primitives, UINT vertices, UINT stride, const void* up, UINT first=0);
    void water(WaterFX&);
    void waterEnd(WaterFX&);
    void surface(IDirect3DTexture9* before,IDirect3DTexture9* mask,IDirect3DTexture9* depth,IDirect3DSurface9* after);
private:
    bool readSurface(IDirect3DSurface9*,const std::string&,const RECT* crop=nullptr);
    void readDepth(IDirect3DTexture9*,const std::string& name="water-depth.r32f",const RECT* crop=nullptr);
    bool readVertices(const std::string&,UINT count);
    bool addBlob(const std::string&,const void*,size_t);
    void begin();
    void finish(bool aborted);
    std::string digest(const void* data, size_t rowBytes, UINT rows=1, UINT pitch=0);
    int texture(IDirect3DBaseTexture9* texture);
    int shader(IUnknown* shader, bool pixel);
    IDirect3DDevice9* device_;
    bool requested_ = false, active_ = false, truncated_ = false;
    bool surfaceOnly_ = false;
    UINT frame_ = 0, draws_ = 0, totalDraws_ = 0;
    DWORD checked_ = 0, started_ = 0;
    std::string lines_;
    std::unordered_map<void*,int> textures_, shaders_;
    std::vector<IUnknown*> retained_;
    struct Blob { std::string name; std::vector<unsigned char> bytes; };
    std::vector<Blob> blobs_;
    BCRYPT_ALG_HANDLE hashAlgorithm_ = nullptr;
    size_t replayBytes_ = 0;
    unsigned waterDraws_ = 0;
    bool waterSource_ = false, waterFinished_ = false;
    unsigned surfaceFrames_ = 0;
    std::string lastApi_;
    UINT lastType_ = 0, lastPrimitives_ = 0, lastVertices_ = 0, lastStride_ = 0, lastFirst_ = 0;
    const void* lastUP_ = nullptr; // borrowed only until the immediately following native draw
};
#endif
