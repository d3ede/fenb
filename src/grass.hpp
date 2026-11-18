#pragma once
#include "settings.hpp"
#include "draw_call.hpp"
#include "state_guard.hpp"
#include "scene_tags.hpp"
#include <bcrypt.h>
#include <array>
#ifdef FENB_DIAGNOSTICS
class PipelineProbe;
#endif

// World-anchored, instanced blade geometry. The native terrain is only replayed
// into a bounded local height/coverage map; client geometry and depth stay native.
class GrassFX {
    friend struct GrassFXFixture;
#ifdef FENB_DIAGNOSTICS
    PipelineProbe *probe_=nullptr;
#endif
    IDirect3DDevice9 *device_;
    BCRYPT_ALG_HANDLE hash_=nullptr;
    Com<IDirect3DTexture9> ground_,contacts_;
    Com<IDirect3DSurface9> groundSurface_,contactSurface_,worldTarget_,worldDepth_;
    Com<IDirect3DVertexShader9> captureVS_,bladeVS_;
    Com<IDirect3DPixelShader9> capturePS_,bladePS_;
    Com<IDirect3DVertexDeclaration9> declaration_;
    struct Band {
        Com<IDirect3DVertexBuffer9> mesh,instances;
        Com<IDirect3DIndexBuffer9> indices;
        unsigned vertices=0,triangles=0,count=0;
        float spacing=0,inner=0,outer=0,factor=1;
    };
    std::array<Band,3> bands_;
    struct Command {
        Com<IDirect3DVertexBuffer9> vertices;
        Com<IDirect3DIndexBuffer9> indices;
        Com<IDirect3DBaseTexture9> art,blendMap;
        SurfaceDraw draw;
        D3DMATRIX matrices[3]{};
        DWORD fvf=0,samplers[2][9]{};
        UINT offset=0,stride=0;
        float material[4]{};
        bool blend=false,prop=false;
        float contactMode=0;
        void clear(){vertices.out();indices.out();art.out();blendMap.out();}
    };
    std::array<Command,256> commands_;
    std::vector<Command *> pending_;
    Settings frame_;
    D3DMATRIX view_{},projection_{};
    D3DVIEWPORT9 viewport_{};
    float eye_[4]{},center_[4]{},map_[4]{},sunDirection_[4]{},sunColor_[4]{},ambient_[4]{},fog_[4]{},fogColor_[4]{};
    bool centered_=false;
    uint64_t worldKey_=0;
    bool winter_=false,contactClear_=true,contactTarget_=false;
    unsigned resolution_=0;
    DWORD worldSrgb_=FALSE;
    int builtQuality_=-1,builtStyle_=-1;
    float builtDensity_=0,builtDistance_=0;
    bool enabled_=false,started_=false,rendered_=false,clear_=true,failed_=false,checked_=false,capable_=false;
    size_t bufferBytes_=0;
    unsigned classify(IDirect3DTexture9 *);
    bool programs();
    bool initialize(const Settings &);
    bool geometry();
    bool target(bool contacts=false);
    void apply(const Command &,bool heightOnly=false);
    void replay(const Command &);
public:
    explicit GrassFX(IDirect3DDevice9 *d):device_(d){}
    ~GrassFX();
    bool supported();
    void beginFrame(bool enabled,const float *player=nullptr,uint64_t worldKey=0);
    void invalidate();
    void capture(const Settings &,DrawCall,SurfaceDraw={},bool water=false);
    void flush();
    bool pending() const {return !pending_.empty();}
    void draw(const Settings &,bool ownScene=false);
    bool beforeRefinement(const Settings &,bool ownScene=false);
    void beforeTranslucency(const Settings &,bool ownScene=false);
#ifdef FENB_DIAGNOSTICS
    void probe(PipelineProbe &,bool after);
    void probeTarget(PipelineProbe *probe){probe_=probe;}
#endif
    size_t bytes() const {return (ground_.p?size_t(resolution_)*resolution_*20:0)+bufferBytes_;}
};
