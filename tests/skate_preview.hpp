#pragma once
#include "skate_trail.hpp"
#include <array>
#include <cstdio>

namespace skate_preview {
constexpr UINT Width = 1000, Height = 650;
using skate_trail::Vec;
using skate_trail::dot;
using skate_trail::color;
enum class Effect { Mist, Ribbons, Stardust, Count };
inline Vec cross(Vec a, Vec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Vec normal(Vec v) { return v*(1.f/std::sqrt(dot(v,v))); }
inline float unit(unsigned seed) {
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    return float(seed & 0xffffu)/65535.f;
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
    skate_trail::Renderer trail_;
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
public:
    explicit Scene(IDirect3DDevice9 *device):device_(device),trail_(device) {
        vertices_.reserve(14000);
        device_->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
        device_->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
        device_->SetRenderState(D3DRS_LIGHTING,FALSE);
        device_->SetRenderState(D3DRS_ZENABLE,TRUE);
        device_->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        device_->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
    }
    bool render(float time,bool turning,Effect effect,DWORD rgb=0,bool custom=false) {
        const Vec p=path(time,turning);
        eye_={-.9f,12.7f,p.z-15.8f};
        forward_=normal(Vec{-.4f,.2f,p.z-6.6f}-eye_);
        right_=normal(cross(Vec{0,1,0},forward_));
        up_=cross(forward_,right_);
        ok_=SUCCEEDED(device_->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff262c38,1,0));
        if(FAILED(device_->BeginScene()))return false;
        ground(time);
        rider(time,turning);
        D3DMATRIX view{};
        view._11=right_.x; view._21=right_.y; view._31=right_.z; view._41=-dot(eye_,right_);
        view._12=up_.x; view._22=up_.y; view._32=up_.z; view._42=-dot(eye_,up_);
        view._13=forward_.x; view._23=forward_.y; view._33=forward_.z; view._43=-dot(eye_,forward_);
        view._44=1;
        D3DMATRIX projection{};
        projection._11=2.f*705/Width;
        projection._22=2.f*705/Height;
        projection._33=1.002f; projection._34=1; projection._43=-.2f;
        device_->SetTransform(D3DTS_VIEW,&view);
        device_->SetTransform(D3DTS_PROJECTION,&projection);
        std::array<skate_trail::Point,128> points{};
        for(size_t i=0;i<points.size();++i) {
            const float age=float(i)/(points.size()-1);
            const float t=time-age*1.64f;
            const Vec forward=heading(t,turning);
            points[i]={wake(t,turning),{forward.z,0,-forward.x},1};
        }
        ok_ &= trail_.draw(points,skate_trail::Style(int(effect)+1),rgb,custom);
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
        if(!scene.render(.97f,true,Effect::Ribbons,0xff651f,true)||
           !screenshot(device,target,L"artifacts/skate-preview/ribbons-custom-orange.png"))return 6;
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
