#pragma once
#include "common.hpp"
#include <array>
#include <cstdio>

namespace skate_preview {
constexpr UINT Width = 1000, Height = 650;
constexpr float Pi = 3.14159265f;
enum class Effect { Mist, Ribbons, Stardust, Count };
enum class Mask { Vapor, Halo, Spark, Count };
struct Vec {
    float x, y, z;
    Vec operator+(Vec b) const { return {x+b.x, y+b.y, z+b.z}; }
    Vec operator-(Vec b) const { return {x-b.x, y-b.y, z-b.z}; }
    Vec operator*(float k) const { return {x*k,y*k,z*k}; }
};
inline float dot(Vec a, Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec cross(Vec a, Vec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Vec normal(Vec v) { return v*(1.f/std::sqrt(dot(v,v))); }
inline float unit(unsigned seed) {
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    return float(seed & 0xffffu)/65535.f;
}
inline float smooth(float x) { x=std::clamp(x,0.f,1.f); return x*x*(3.f-2.f*x); }
inline DWORD color(DWORD rgb, float opacity) {
    return (rgb & 0xffffffu) | (DWORD(std::clamp(opacity,0.f,1.f)*255.f+.5f)<<24);
}
inline Vec path(float t, bool turning) {
    return {turning ? 2.9f*std::sin(t*2.15f) : .19f*t,1.15f+.12f*std::sin(t*4),t*9.f};
}
inline Vec heading(float t, bool turning) {
    return normal(Vec{turning ? 6.235f*std::cos(t*2.15f) : .19f,0,9});
}
struct Vertex { float x,y,z,rhw; DWORD tint; float u,v; };

class Scene {
    IDirect3DDevice9 *device_;
    std::array<Com<IDirect3DTexture9>,size_t(Mask::Count)> masks_;
    std::vector<Vertex> vertices_;
    Vec eye_{},right_{},up_{},forward_{};
    bool ok_ = true;

    Vertex project(Vec p, DWORD tint, float u=0, float v=0) const {
        Vec rel=p-eye_;
        const float depth=dot(rel,forward_), reciprocal=1.f/std::max(.2f,depth);
        const float cameraScale=705*reciprocal;
        return {Width*.5f+dot(rel,right_)*cameraScale-.5f,
                Height*.5f-dot(rel,up_)*cameraScale-.5f,
                1.002f-.2f*reciprocal,reciprocal,tint,u,v};
    }
    void quad(Vertex a,Vertex b,Vertex c,Vertex d) {
        const Vertex triangles[]={a,b,c,c,b,d};
        vertices_.insert(vertices_.end(),std::begin(triangles),std::end(triangles));
    }
    void draw(IDirect3DTexture9 *texture=nullptr) {
        if(vertices_.empty())return;
        device_->SetTexture(0,texture);
        device_->SetTextureStageState(0,D3DTSS_COLOROP,texture?D3DTOP_MODULATE:D3DTOP_SELECTARG2);
        device_->SetTextureStageState(0,D3DTSS_ALPHAOP,texture?D3DTOP_MODULATE:D3DTOP_SELECTARG2);
        ok_ &= SUCCEEDED(device_->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(vertices_.size()/3),vertices_.data(),sizeof(Vertex)));
        vertices_.clear();
    }
    void ground(float time) {
        const int center=int(std::floor(time*9/1.3f));
        for(int row=center-16;row<center+23;++row) {
            for(int column=-24;column<24;++column) {
                const float x=(column+.5f*(row&1))*1.6f,z=row*1.3f;
                const unsigned seed=unsigned(row)*2677u+unsigned(column)*1559u;
                const int tone= int(unit(seed)*19);
                DWORD top=D3DCOLOR_XRGB(43+tone,49+tone,59+tone);
                const DWORD light=D3DCOLOR_XRGB(62+tone,68+tone,78+tone);
                const DWORD dark=D3DCOLOR_XRGB(34+tone,39+tone,47+tone);
                if(dot(Vec{x,0,z}-eye_,forward_)<2.5f)continue;
                const auto a=project({x+.035f,0,z+.035f},light);
                const auto b=project({x+1.565f,0,z+.035f},top);
                const auto c=project({x+.035f,0,z+1.265f},top);
                const auto d=project({x+1.565f,0,z+1.265f},dark);
                quad(a,b,c,d);
            }
        }
        draw();
    }
    void box(Vec center,Vec across,Vec along,float width,float height,float length,DWORD rgb) {
        Vec a=across*width,b=Vec{0,height,0},c=along*length;
        auto corner=[&](int x,int y,int z){return center+a*float(x)+b*float(y)+c*float(z);};
        auto face=[&](Vec p,Vec q,Vec r,Vec s,DWORD tint){
            quad(project(p,tint),project(q,tint),project(r,tint),project(s,tint));
        };
        const DWORD side=0xff000000u|((rgb&0xfefefeu)>>1);
        face(corner(-1,1,-1),corner(1,1,-1),corner(-1,1,1),corner(1,1,1),color(rgb,1));
        for(int sign:{-1,1}) {
            face(corner(sign,-1,-1),corner(sign,-1,1),corner(sign,1,-1),corner(sign,1,1),side);
            face(corner(-1,-1,sign),corner(1,-1,sign),corner(-1,1,sign),corner(1,1,sign),side);
        }
    }
    void rider(float time,bool turning) {
        const Vec p=path(time,turning),forward=heading(time,turning),side={forward.z,0,-forward.x};
        box(p,side,forward,.29f,.045f,.82f,0x907de6);
        box(p+Vec{0,.045f,0},side,forward,.22f,.027f,.68f,0x24304a);
        box(p-forward*.75f,side,forward,.24f,.055f,.06f,0xbbf6ff);
        for(float stance:{-.34f,.32f}) {
            const Vec foot=p+forward*stance+side*(stance*.2f);
            box(foot+Vec{0,.15f,0},side,forward,.11f,.08f,.18f,0x9cabc4);
            box(foot+Vec{0,.35f,0},side,forward,.105f,.19f,.105f,0x26364f);
        }
        box(p+Vec{0,.72f,0},side,forward,.26f,.24f,.14f,0x6077ba);
        box(p+Vec{0,1.06f,0}+forward*.075f,side,forward,.16f,.15f,.155f,0xe3e9fb);
        box(p+Vec{0,1.08f,0}+forward*.17f,side,forward,.15f,.055f,.08f,0x2c416e);
        box(p+Vec{0,.73f,0}-side*.29f+forward*.17f,side,forward,.085f,.11f,.32f,0x536caa);
        box(p+Vec{0,.67f,0}+side*.30f-forward*.14f,side,forward,.085f,.11f,.28f,0x536caa);
        draw();
    }
    Vec wake(float time,bool turning) const {
        return path(time,turning)-heading(time,turning)*.74f-Vec{0,.035f,0};
    }
    void sprite(Vec p,float radius,DWORD tint) {
        Vec a=right_*radius,b=up_*radius;
        quad(project(p-a+b,tint,0,0),project(p+a+b,tint,1,0),
             project(p-a-b,tint,0,1),project(p+a-b,tint,1,1));
    }
    struct Strip {
        float width, offset, wave, lift, intensity;
        DWORD rgb;
        Mask mask;
    };
    void strip(float time,bool turning,float lifetime,Strip style) {
        constexpr int Segments=100;
        Vertex left{},right{};
        for(int i=0;i<=Segments;++i) {
            const float age=float(i)/Segments,t=time-age*lifetime;
            const Vec forward=heading(t,turning), across={forward.z,0,-forward.x};
            const Vec p=wake(t,turning)+across*((style.offset+style.wave*std::sin(t*10))*smooth(age*7))
                +Vec{0,style.lift*smooth(age*8),0};
            const float width=style.width*(.72f+smooth(age*7)*.46f)*(1-.36f*age);
            const float strength=style.intensity*(1-smooth((age-.13f)/.87f));
            const DWORD tint=color(style.rgb,strength);
            const float phase=t*1.85f;
            const Vertex a=project(p-across*width,tint,0,phase),b=project(p+across*width,tint,1,phase);
            if(i)quad(left,right,a,b);
            left=a;
            right=b;
        }
        draw(masks_[size_t(style.mask)]);
    }
    void trail(float time,bool turning,Effect effect) {
        device_->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
        device_->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
        switch(effect) {
        case Effect::Mist:
            strip(time,turning,1.68f,{1.05f,0,.11f,.13f,.29f,0x5276ef,Mask::Vapor});
            strip(time,turning,1.54f,{.62f,.09f,.17f,.05f,.46f,0x54efed,Mask::Vapor});
            strip(time,turning,1.28f,{.29f,-.11f,-.21f,.04f,.35f,0xafe9ff,Mask::Vapor});
            break;
        case Effect::Ribbons:
            strip(time,turning,1.60f,{.69f,0,0,0,.13f,0x729eff,Mask::Halo});
            for(float side:{-1.f,1.f}) {
                const DWORD tint=side<0?0x69ecff:0xcf9aff;
                strip(time,turning,1.59f,{.17f,side*.23f,side*.30f,side*.13f,.37f,tint,Mask::Halo});
                strip(time,turning,1.59f,{.048f,side*.23f,side*.30f,side*.13f,.95f,tint,Mask::Halo});
            }
            break;
        case Effect::Stardust: {
            strip(time,turning,1.52f,{.76f,0,.06f,0,.19f,0x7276ff,Mask::Vapor});
            const int newest=int(std::floor(time*124));
            for(int i=newest-183;i<=newest;++i) {
                const float t=float(i)/124,age=(time-t)/1.49f;
                const unsigned seed=unsigned(i)*13u;
                const Vec forward=heading(t,turning), side={forward.z,0,-forward.x};
                const float spread=.12f+age*.82f;
                const Vec p=wake(t,turning)+side*((unit(seed)*2-1)*spread)
                    +Vec{0,(unit(seed+1)*2-1)*spread*.45f,0};
                const bool burst=unit(seed+2)>.94f;
                const float size=burst?.19f+unit(seed+3)*.13f:.029f+unit(seed+3)*.05f;
                const float opacity=smooth(age*32)*(1-smooth(age))*(.59f+unit(seed+4)*.41f);
                sprite(p,size,color(burst?0xc5ddff:0xa79aff,opacity));
            }
            draw(masks_[size_t(Mask::Spark)]);
            break;
        }
        case Effect::Count: break;
        }
        sprite(wake(time,turning),.35f,color(effect==Effect::Stardust?0x929bff:0x99eaff,.43f));
        draw(masks_[size_t(Mask::Spark)]);
        device_->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
        device_->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
    }

public:
    explicit Scene(IDirect3DDevice9 *device):device_(device) { vertices_.reserve(14000); }
    bool initialize() {
        constexpr int Size=128;
        for(size_t i=0;i<masks_.size();++i) {
            auto &texture=masks_[i];
            if(FAILED(device_->CreateTexture(Size,Size,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,texture.out(),nullptr)))return false;
            D3DLOCKED_RECT locked{};
            if(FAILED(texture->LockRect(0,&locked,nullptr,0)))return false;
            for(int y=0;y<Size;++y)for(int x=0;x<Size;++x) {
                const float u=(x+.5f)*2/Size-1,v=(y+.5f)*2/Size-1;
                float density=0;
                switch(Mask(i)) {
                case Mask::Vapor: {
                    const float phase=v*Pi;
                    const float swirl=u+.12f*std::sin(phase)+.07f*std::sin(3*phase+u*2);
                    density=std::exp(-swirl*swirl*3.2f)*smooth((1-std::abs(u))*3)
                        *(.79f+.16f*std::sin(phase*2+u*5)+.05f*std::cos(phase*4-u*5));
                    break;
                }
                case Mask::Halo:
                    density=std::exp(-u*u*3.7f)*smooth((1-std::abs(u))*4);
                    break;
                case Mask::Spark: {
                    const float r=u*u+v*v;
                    const float core=std::exp(-r*16);
                    const float flare=std::exp(-std::min(u*u,v*v)*980)*std::exp(-std::max(u*u,v*v)*3.8f);
                    density=std::min(1.f,core*.88f+flare*.42f)*smooth((1-std::sqrt(r))*5);
                    break;
                }
                case Mask::Count: break;
                }
                auto *row=reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits)+y*locked.Pitch);
                row[x]=color(0xffffff,density);
            }
            if(FAILED(texture->UnlockRect(0)))return false;
        }
        device_->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
        device_->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
        device_->SetRenderState(D3DRS_LIGHTING,FALSE);
        device_->SetRenderState(D3DRS_ZENABLE,TRUE);
        device_->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
        device_->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);
        device_->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
        device_->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        device_->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
        device_->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
        device_->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
        device_->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
        device_->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
        device_->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
        return true;
    }
    bool render(float time,bool turning,Effect effect) {
        const Vec p=path(time,turning);
        eye_={-.9f,12.7f,p.z-15.8f};
        forward_=normal(Vec{-.4f,.2f,p.z-6.6f}-eye_);
        right_=normal(cross(Vec{0,1,0},forward_));
        up_=cross(forward_,right_);
        ok_=SUCCEEDED(device_->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff262c38,1,0));
        if(FAILED(device_->BeginScene()))return false;
        ground(time);
        rider(time,turning);
        trail(time,turning,effect);
        return SUCCEEDED(device_->EndScene())&&ok_;
    }
};

using Screenshot=bool(*)(IDirect3DDevice9 *,IDirect3DSurface9 *,const wchar_t *);
inline int run(bool capture,WNDPROC windowProcedure,Screenshot screenshot) {
    const HINSTANCE instance=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc=windowProcedure;
    wc.hInstance=instance;
    wc.lpszClassName=L"FENBSkatePreview";
    wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
    if(!RegisterClassW(&wc))return 2;
    RECT bounds{0,0,Width,Height};
    constexpr DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    AdjustWindowRect(&bounds,style,FALSE);
    HWND window=CreateWindowW(wc.lpszClassName,L"FENB skate preview",style,CW_USEDEFAULT,CW_USEDEFAULT,
                              bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,nullptr);
    if(!window)return 2;

    // Use the Windows runtime directly, even when a proxy sits beside the preview.
    HMODULE library=LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    using Create=IDirect3D9 *(WINAPI *)(UINT);
    auto create=library?reinterpret_cast<Create>(GetProcAddress(library,"Direct3DCreate9")):nullptr;
    Com<IDirect3D9> root;
    root.p=create?create(D3D_SDK_VERSION):nullptr;
    if(!root.p)return 3;
    D3DPRESENT_PARAMETERS parameters{};
    parameters.Windowed=TRUE;
    parameters.hDeviceWindow=window;
    parameters.SwapEffect=D3DSWAPEFFECT_DISCARD;
    parameters.BackBufferWidth=Width;
    parameters.BackBufferHeight=Height;
    parameters.BackBufferFormat=D3DFMT_X8R8G8B8;
    parameters.EnableAutoDepthStencil=TRUE;
    parameters.AutoDepthStencilFormat=D3DFMT_D16;
    parameters.PresentationInterval=D3DPRESENT_INTERVAL_ONE;
    Com<IDirect3DDevice9> device;
    if(FAILED(root->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                 &parameters,device.out())))return 4;
    Scene scene(device);
    if(!scene.initialize())return 5;
    constexpr const wchar_t *Names[]={L"Mist",L"Light ribbons",L"Stardust"};
    if(capture) {
        CreateDirectoryW(L"artifacts",nullptr);
        CreateDirectoryW(L"artifacts/skate-preview",nullptr);
        Com<IDirect3DSurface9> target;
        if(FAILED(device->GetRenderTarget(0,target.out())))return 6;
        constexpr const wchar_t *Files[]={L"mist",L"ribbons",L"stardust"};
        for(int i=0;i<int(Effect::Count);++i) for(bool turning:{false,true}) {
            wchar_t filename[160]{};
            swprintf(filename,std::size(filename),L"artifacts/skate-preview/%ls-%ls.png",Files[i],
                     turning?L"turn":L"straight");
            if(!scene.render(.97f,turning,Effect(i))||!screenshot(device,target,filename))return 6;
            wprintf(L"Captured %ls\n",filename);
        }
        DestroyWindow(window);
        return 0;
    }

    Effect effect=Effect::Mist;
    bool paused=false,turning=true,titleDirty=true;
    ShowWindow(window,SW_SHOW);
    ULONGLONG previous=GetTickCount64();
    float time=.97f;
    while(IsWindow(window)) {
        MSG message{};
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
            if(message.message==WM_QUIT)return 0;
            if(message.message==WM_KEYDOWN && !(message.lParam&(1L<<30))) {
                if(message.wParam>='1'&&message.wParam<='3')effect=Effect(message.wParam-'1');
                if(message.wParam==VK_SPACE)paused=!paused;
                if(message.wParam=='T')turning=!turning;
                if(message.wParam==VK_ESCAPE)return 0;
                titleDirty=true;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        const ULONGLONG now=GetTickCount64();
        if(!paused)time+=float(std::min<ULONGLONG>(now-previous,50))*.001f;
        previous=now;
        if(titleDirty) {
            wchar_t title[140]{};
            swprintf(title,std::size(title),L"FENB | %ls | 1/2/3 styles, T turns, Space pause",Names[size_t(effect)]);
            SetWindowTextW(window,title);
            titleDirty=false;
        }
        if(!scene.render(time,turning,effect)||FAILED(device->Present(nullptr,nullptr,nullptr,nullptr)))return 7;
    }
    return 0;
}
}
