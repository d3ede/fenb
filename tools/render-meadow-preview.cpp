// Isolated visual study: native Flyff alpha artwork, never the tuft mesh.
// The client directory is read-only. Captures and metadata go to artifacts/.
#include "common.hpp"
#include <d3dcompiler.h>
#include <gdiplus.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <stdexcept>

namespace {
constexpr unsigned Width=1400,Height=900;
void Check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
struct Vec {float x,y,z;Vec operator+(Vec b)const{return {x+b.x,y+b.y,z+b.z};}Vec operator-(Vec b)const{return {x-b.x,y-b.y,z-b.z};}Vec operator*(float a)const{return {x*a,y*a,z*a};}};
float Dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec Cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vec Unit(Vec a){return a*(1/std::sqrt(std::max(1e-8f,Dot(a,a))));}
D3DMATRIX Identity(){D3DMATRIX m{};m._11=m._22=m._33=m._44=1;return m;}
D3DMATRIX Look(Vec eye,Vec at){Vec z=Unit(at-eye),x=Unit(Cross({0,1,0},z)),y=Cross(z,x);return {x.x,y.x,z.x,0,x.y,y.y,z.y,0,x.z,y.z,z.z,0,-Dot(x,eye),-Dot(y,eye),-Dot(z,eye),1};}
float Smooth(float a,float b,float v){float t=std::clamp((v-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
struct Random {unsigned state=0x5a97c913u;float next(){state^=state<<13;state^=state>>17;state^=state<<5;return (state>>8)*(1.f/16777216);}};
struct Vertex {float x,y,z;DWORD color;float u,v;};
constexpr DWORD Fvf=D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1;
DWORD Color(float r,float g,float b){auto c=[](float a){return unsigned(std::clamp(a,0.f,1.f)*255+.5f);};return D3DCOLOR_XRGB(c(r),c(g),c(b));}
struct Card {Vec root;float width,height,yaw,bend,twist,seasonGate;bool mirror;};
using LoadTexture=HRESULT(WINAPI *)(IDirect3DDevice9*,LPCWSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,DWORD,DWORD,D3DCOLOR,void*,PALETTEENTRY*,IDirect3DTexture9**);
const wchar_t *Artwork=L"Model/Texture/Obj_MaCoPrGr07.dds";
const char *GrassPS=R"(
sampler2D Art:register(s0);
float4 Season:register(c0);
float4 main(float4 light:COLOR0,float2 uv:TEXCOORD0):COLOR0 {
 // Retain filtered edges of the thin painted stems. A hard .5 threshold
 // loses their tips and makes the surviving base look disproportionately wide.
 float4 a=tex2D(Art,uv);clip(a.a-.035);
 float3 color=saturate(a.rgb*float3(1.18,1.15,.92));
 if(Season.x>.5){
  float luma=dot(color,float3(.2126,.7152,.0722));
  float3 dry=lerp(float3(.56,.54,.48),float3(.76,.75,.69),sqrt(saturate(luma)));
  float frost=smoothstep(.48,.92,luma)*(.30+.70*(1-uv.y));
  color=lerp(dry,float3(.76,.79,.80),frost*.72);
 }
 return float4(color*lerp(light.rgb,lerp(.86,1,light.rgb),Season.x),a.a);
})";
class Preview {
    HWND window_=nullptr;HMODULE d3dx_=nullptr;
    Com<IDirect3D9> d3d_;Com<IDirect3DDevice9> d_;
    Com<IDirect3DTexture9> ground_[2],art_;Com<IDirect3DPixelShader9> grassPS_;
    std::vector<Card> cards_;std::filesystem::path output_;
    unsigned samples_=0;D3DMATRIX view_{},projection_{};
    void load(Com<IDirect3DTexture9>&t,const std::filesystem::path &path){
        auto fn=reinterpret_cast<LoadTexture>(GetProcAddress(d3dx_,"D3DXCreateTextureFromFileExW"));Check(fn,"D3DX texture API");
        Check(SUCCEEDED(fn(d_,path.c_str(),UINT(-1),UINT(-1),0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,1,1,0,nullptr,nullptr,t.out())),"native texture read");
    }
    void bleed(IDirect3DTexture9 *texture){
        for(unsigned level=0;level<texture->GetLevelCount();++level){
            D3DSURFACE_DESC desc{};texture->GetLevelDesc(level,&desc);D3DLOCKED_RECT lock{};Check(SUCCEEDED(texture->LockRect(level,&lock,nullptr,0)),"owned artwork lock");
            std::vector<DWORD> pixels(desc.Width*desc.Height);std::vector<unsigned char> valid(pixels.size());
            for(unsigned y=0;y<desc.Height;++y)memcpy(pixels.data()+y*desc.Width,(BYTE*)lock.pBits+y*lock.Pitch,desc.Width*4);
            for(size_t i=0;i<pixels.size();++i)valid[i]=(pixels[i]>>24)>=16;
            for(unsigned pass=0;pass<6;++pass){auto next=pixels;auto filled=valid;
                for(unsigned y=0;y<desc.Height;++y)for(unsigned x=0;x<desc.Width;++x){size_t i=y*desc.Width+x;if(valid[i])continue;unsigned rgb[3]{},n=0;
                    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){int a=int(x)+dx,b=int(y)+dy;if(a<0||b<0||a>=int(desc.Width)||b>=int(desc.Height))continue;size_t j=b*desc.Width+a;if(!valid[j])continue;
                        for(unsigned c=0;c<3;++c)rgb[c]+=(pixels[j]>>(c*8))&255;++n;}
                    if(n){next[i]=(pixels[i]&0xff000000)|(rgb[0]/n)|((rgb[1]/n)<<8)|((rgb[2]/n)<<16);filled[i]=1;}}
                pixels.swap(next);valid.swap(filled);
            }
            for(unsigned y=0;y<desc.Height;++y)memcpy((BYTE*)lock.pBits+y*lock.Pitch,pixels.data()+y*desc.Width,desc.Width*4);
            texture->UnlockRect(level);
        }
    }
    std::vector<Vertex> mesh(const Card &c,bool winter)const{
        const float seasonalHeight=winter?.48f:1.f,seasonalWidth=winter?.72f:1.f;
        const float h=c.height*seasonalHeight,w=c.width*seasonalWidth;constexpr unsigned Segments=4;
        const Vec sun=Unit({-.30f,.86f,-.40f});std::vector<Vertex> v;
        for(unsigned s=0;s<=Segments;++s){float t=float(s)/Segments;
            const float a=c.yaw+c.twist*t;Vec side{std::cos(a),0,std::sin(a)},normal{-std::sin(a),0,std::cos(a)};
            Vec center=c.root+Vec{0,h*t-h*.045f*t*t*t,0}+normal*(h*c.bend*t*t);
            Vec lightNormal=Unit({normal.x,.4f,normal.z});
            float light=(.90f+.20f*std::abs(Dot(lightNormal,sun)))*(.88f+.12f*Smooth(0,.65f,t));
            const float warm=std::max(0.f,Dot(lightNormal,sun));
            DWORD tint=Color(light*(.92f+.08f*warm),light,light*(.86f+.08f*(1-warm)));
            for(unsigned edge=0;edge<2;++edge){Vec p=center+side*((edge?1.f:-1.f)*w*.5f);
                v.push_back({p.x,p.y,p.z,tint,c.mirror?1.f-edge:float(edge),1-t});}
        }
        std::vector<Vertex> triangles;for(unsigned s=0;s<Segments;++s){auto a=s*2,b=a+2;triangles.insert(triangles.end(),{v[a],v[a+1],v[b],v[b],v[a+1],v[b+1]});}return triangles;
    }
    void setup(){
        auto id=Identity();d_->SetTransform(D3DTS_WORLD,&id);d_->SetTransform(D3DTS_VIEW,&view_);d_->SetTransform(D3DTS_PROJECTION,&projection_);
        d_->SetVertexShader(nullptr);d_->SetFVF(Fvf);
        for(auto state:{D3DRS_LIGHTING,D3DRS_ALPHABLENDENABLE,D3DRS_ALPHATESTENABLE,D3DRS_FOGENABLE,D3DRS_STENCILENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_SRGBWRITEENABLE})d_->SetRenderState(state,FALSE);
        d_->SetRenderState(D3DRS_ZENABLE,TRUE);d_->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);d_->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
        d_->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);d_->SetRenderState(D3DRS_COLORWRITEENABLE,15);d_->SetRenderState(D3DRS_MULTISAMPLEMASK,0xffffffff);
        d_->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);d_->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);d_->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        d_->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);d_->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
        d_->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);d_->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);d_->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        for(auto key:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER})d_->SetSamplerState(0,key,D3DTEXF_LINEAR);
        d_->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);d_->SetSamplerState(0,D3DSAMP_MAXMIPLEVEL,0);d_->SetSamplerState(0,D3DSAMP_MIPMAPLODBIAS,0);
    }
    std::vector<DWORD> read(){
        Com<IDirect3DSurface9> src,resolved,read;d_->GetRenderTarget(0,src.out());
        if(samples_){Check(SUCCEEDED(d_->CreateRenderTarget(Width,Height,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,resolved.out(),nullptr)),"resolve target");Check(SUCCEEDED(d_->StretchRect(src,nullptr,resolved,nullptr,D3DTEXF_NONE)),"resolve");std::swap(src.p,resolved.p);}
        Check(SUCCEEDED(d_->CreateOffscreenPlainSurface(Width,Height,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,read.out(),nullptr)),"read surface");Check(SUCCEEDED(d_->GetRenderTargetData(src,read)),"read pixels");
        D3DLOCKED_RECT lock{};Check(SUCCEEDED(read->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"read lock");std::vector<DWORD> p(Width*Height);
        for(unsigned y=0;y<Height;++y)memcpy(p.data()+y*Width,(BYTE*)lock.pBits+y*lock.Pitch,Width*4);read->UnlockRect();return p;
    }
    void save(const std::vector<DWORD>&pixels,const std::wstring &file){
        const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0,0,0xf8,0x1e,0xf3,0x2e}};
        Gdiplus::Bitmap image(Width,Height,Width*4,PixelFormat32bppRGB,(BYTE*)pixels.data());Check(image.Save((output_/file).c_str(),&png,nullptr)==Gdiplus::Ok,"PNG save");
    }
public:
    Preview(std::filesystem::path client,std::filesystem::path output):output_(std::move(output)){
        std::filesystem::create_directories(output_);std::ofstream(output_/L"study.jsonl",std::ios::trunc);
        WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FENBMeadowStudy";RegisterClassW(&wc);
        window_=CreateWindowW(wc.lpszClassName,L"FENB meadow study",WS_OVERLAPPEDWINDOW,0,0,Width,Height,nullptr,nullptr,wc.hInstance,nullptr);
        d3d_.p=Direct3DCreate9(D3D_SDK_VERSION);Check(d3d_.p,"D3D9");D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.hDeviceWindow=window_;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
        pp.BackBufferWidth=Width;pp.BackBufferHeight=Height;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24X8;
        if(SUCCEEDED(d3d_->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,pp.BackBufferFormat,TRUE,D3DMULTISAMPLE_8_SAMPLES,nullptr))){pp.MultiSampleType=D3DMULTISAMPLE_8_SAMPLES;samples_=8;}
        Check(SUCCEEDED(d3d_->CreateDevice(0,D3DDEVTYPE_HAL,window_,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,d_.out())),"D3D device");
        d3dx_=LoadLibraryExW(L"d3dx9_43.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);Check(d3dx_,"system D3DX9");
        load(ground_[0],client/L"World/Texture/ground_lawn001.dds");load(ground_[1],client/L"World/Texture/ground_n_ice02.dds");
        load(art_,client/Artwork);bleed(art_);
        Com<ID3DBlob> code,error;HRESULT hr=D3DCompile(GrassPS,strlen(GrassPS),"meadow-study",nullptr,nullptr,"main","ps_2_a",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,code.out(),error.out());
        if(error.p)fputs((char*)error->GetBufferPointer(),stderr);Check(SUCCEEDED(hr),"grass shader compile");Check(SUCCEEDED(d_->CreatePixelShader((DWORD*)code->GetBufferPointer(),grassPS_.out())),"grass shader");
        projection_={};projection_._11=1.4f;projection_._22=1.4f*Width/Height;projection_._33=100/99.9f;projection_._34=1;projection_._43=-.1f*projection_._33;
        Random r;constexpr unsigned Grid=18;
        for(unsigned z=0;z<Grid;++z)for(unsigned x=0;x<Grid;++x){
            float px=-1.5f+(x+.12f+r.next()*.76f)*3/Grid,pz=-1.5f+(z+.12f+r.next()*.76f)*3/Grid;
            const float edge=Smooth(0,.35f,std::min(1.5f-std::abs(px),1.5f-std::abs(pz)));
            const float lumps=.5f+.28f*std::sin(px*2.4f+pz*.7f)+.22f*std::cos(pz*3.2f-px*.9f);
            if(r.next()>(.12f+.68f*std::clamp(lumps,0.f,1.f))*edge)continue;
            Card c{};c.root={px,.008f,pz};const bool shortCard=r.next()>=.90f;
            float length=r.next();
            const float tier=length<.2f?.14f+.08f*(length/.2f):length<.8f?.23f+.15f*((length-.2f)/.6f):.40f+.13f*((length-.8f)/.2f);
            c.height=shortCard?.18f+.20f*length:tier;
            c.width=shortCard?.23f+.13f*r.next():c.height*(.9f+r.next()*.3f);
            c.yaw=r.next()*6.2831853f;c.bend=.06f+r.next()*.18f;c.twist=(r.next()-.5f)*.18f;c.mirror=r.next()>.5f;c.seasonGate=r.next();cards_.push_back(c);
        }
    }
    ~Preview(){if(d3dx_)FreeLibrary(d3dx_);if(window_)DestroyWindow(window_);}
    std::vector<DWORD> frame(bool winter,bool close,bool grass){
        view_=close?Look({.15f,.78f,-2.8f},{0,.23f,0}):Look({1.9f,2.0f,-2.6f},{0,.12f,0});setup();
        Check(SUCCEEDED(d_->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,winter?0xffc4ccd7:0xff8ba4ad,1,0)),"clear");Check(SUCCEEDED(d_->BeginScene()),"BeginScene");
        d_->SetPixelShader(nullptr);d_->SetTexture(0,ground_[winter?1:0]);d_->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);d_->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
        DWORD floorColor=winter?Color(.91f,.94f,.98f):Color(.86f,.87f,.80f);
        const Vertex floor[]={{-64,0,-64,floorColor,-22.4f,-22.4f},{-64,0,64,floorColor,-22.4f,22.4f},{64,0,-64,floorColor,22.4f,-22.4f},{64,0,64,floorColor,22.4f,22.4f}};
        Check(SUCCEEDED(d_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,floor,sizeof(Vertex))),"ground draw");
        if(grass){d_->SetPixelShader(grassPS_);const float season[]={winter?1.f:0.f,0,0,0};d_->SetPixelShaderConstantF(0,season,1);
            d_->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);d_->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);d_->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
            d_->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);d_->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
            auto sorted=cards_;std::stable_sort(sorted.begin(),sorted.end(),[this](const Card&a,const Card&b){auto depth=[this](Vec p){return p.x*view_._13+p.y*view_._23+p.z*view_._33+view_._43;};return depth(a.root)>depth(b.root);});
            d_->SetTexture(0,art_);
            for(const auto &c:sorted){if(winter&&c.seasonGate>.28f)continue;auto v=mesh(c,winter);Check(SUCCEEDED(d_->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(v.size()/3),v.data(),sizeof(Vertex))),"grass draw");}}
        Check(SUCCEEDED(d_->EndScene()),"EndScene");return read();
    }
    void capture(bool winter,bool close){
        auto bare=frame(winter,close,false),grass=frame(winter,close,true);
        std::wstring name=(winter?L"winter-":L"summer-")+std::wstring(close?L"close":L"game");save(bare,name+L"-ground.png");save(grass,name+L".png");
        unsigned changed=0,cards=0;for(size_t i=0;i<grass.size();++i)changed+=(grass[i]&0xffffff)!=(bare[i]&0xffffff);
        for(const auto &c:cards_)cards+=!winter||c.seasonGate<=.28f;
        printf("STUDY cards=%u changed=%u\n",cards,changed);Check(changed>1000&&cards>5,"nonempty grass study");
        printf("CAPTURE %ls patch=3x3 cards=%u triangles=%u changedPixels=%u winter=%d MSAA=%u\n",name.c_str(),cards,cards*8,changed,winter,samples_);
        std::ofstream log(output_/L"study.jsonl",std::ios::app);log<<"{\"season\":\""<<(winter?"winter":"summer")<<"\",\"camera\":\""<<(close?"close":"game")<<"\",\"patchMeters\":[3,3],\"cards\":"<<cards<<",\"triangles\":"<<cards*8<<",\"changedPixels\":"<<changed<<",\"msaa\":"<<samples_<<"}\n";
    }
};
}
int wmain(int argc,wchar_t **argv){
    if(argc!=3){puts("Usage: meadow-preview <read-only-client-artwork-root> <output-artifacts-directory>");return 2;}
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)return 3;
    int result=0;try{Preview p(argv[1],argv[2]);p.capture(false,true);p.capture(false,false);p.capture(true,true);p.capture(true,false);}
    catch(const std::exception &e){fprintf(stderr,"FAIL: %s\n",e.what());result=1;}Gdiplus::GdiplusShutdown(token);return result;
}
