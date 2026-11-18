// Isolated 3x3 metre study: independently rooted leaves and attached snow meshes.
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
struct Blade {Vec root;float width,height,yaw,bend,twist,seasonGate;unsigned variant;bool mirror;};
Blade DetailBlade(){return {{0,.004f,0},.022f,.32f,1.5707963f,.65f,.35f,0,2,false};}
using LoadTexture=HRESULT(WINAPI *)(IDirect3DDevice9*,LPCWSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,DWORD,DWORD,D3DCOLOR,void*,PALETTEENTRY*,IDirect3DTexture9**);
constexpr float WinterCoverage=.84f;
const char *GrassPS=R"(
sampler2D Art:register(s0);
float4 Season:register(c0);
float4 main(float4 light:COLOR0,float2 uv:TEXCOORD0):COLOR0 {
 float3 color=tex2D(Art,uv).rgb;
 if(Season.x<.5)color=saturate(color*float3(1.25,1.35,1.03));
 if(Season.x>.5&&Season.x<1.5){
  float value=dot(color,float3(.2126,.7152,.0722));
  color=lerp(float3(.20,.24,.15),float3(.58,.60,.40),value);
 }
 return float4(color*light.rgb,light.a);
})";
class Preview {
    HWND window_=nullptr;HMODULE d3dx_=nullptr;
    Com<IDirect3D9> d3d_;Com<IDirect3DDevice9> d_;
    Com<IDirect3DTexture9> ground_[2],art_[2];Com<IDirect3DPixelShader9> grassPS_;
    std::vector<Blade> blades_;std::filesystem::path output_;
    unsigned samples_=0;D3DMATRIX view_{},projection_{};
    void load(Com<IDirect3DTexture9>&t,const std::filesystem::path &path){
        auto fn=reinterpret_cast<LoadTexture>(GetProcAddress(d3dx_,"D3DXCreateTextureFromFileExW"));Check(fn,"D3DX texture API");
        Check(SUCCEEDED(fn(d_,path.c_str(),UINT(-1),UINT(-1),0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,3,5,0,nullptr,nullptr,t.out())),"native texture read");
    }
    std::vector<Vertex> mesh(const Blade &c,bool winter,bool snow=false)const{
        constexpr unsigned Segments=8,Columns=2;
        const float h=c.height*(winter?.72f:1.f),w=c.width*(winter?.92f:1.f);
        const Vec sun=Unit({-.30f,.86f,-.40f});
        std::vector<Vertex> vertices,triangles;
        for(unsigned segment=0;segment<=Segments;++segment){
            float t=float(segment)/Segments,angle=c.yaw+c.twist*t;
            Vec outward{std::cos(angle),0,std::sin(angle)},side{-outward.z,0,outward.x};
            Vec center=c.root+Vec{0,h*(t-c.bend*.22f*t*t*t)-(winter?.045f:0),0}+outward*(h*c.bend*t*t);
            Vec tangent=Unit(Vec{0,h*(1-c.bend*.66f*t*t),0}+outward*(h*c.bend*2*t));
            Vec normal=Unit(Cross(side,tangent));
            float width=w*(.72f+1.3f*t)*std::pow(std::max(0.f,1-t),.85f);
            float snowStart=c.variant==0?.625f:.375f;
            if(snow)width=width*(1.5f+.35f*std::sin(t*25+c.yaw)*std::sin(t*25+c.yaw))+w*.18f*Smooth(.65f,.85f,t)*(1-Smooth(.92f,1,t));
            for(unsigned column=0;column<=Columns;++column){
                float across=float(column)-1;
                Vec faceNormal=Unit(normal+side*(across<0?.55f:across>0?-.55f:0));
                float direct=std::abs(Dot(faceNormal,sun));
                float lighting=(.62f+.48f*direct)*(.78f+.22f*Smooth(0,.65f,t));
                if(snow)lighting=.77f+.23f*direct;
                DWORD color=Color(lighting*(snow?.96f:1.f),lighting,lighting*(snow?1.f:.90f));
                if(snow)color=(color&0xffffff)|(DWORD(Smooth(snowStart,snowStart+.15f,t)*255)<<24);
                Vec position=center+side*(across*width*.5f)+normal*(width*.14f*(1-std::abs(across))+(snow?.0055f:0));
                float u=snow?(across+1)*.25f+c.variant*.11f:.03f+c.variant*.20f+(c.mirror?1-(across+1)*.5f:(across+1)*.5f)*.15f;
                vertices.push_back({position.x,position.y,position.z,color,u,1-t*.94f});
            }
        }
        for(unsigned segment=0;segment<Segments;++segment){
            if(snow&&float(segment)/Segments<(c.variant==0?.625f:.375f))continue;
            for(unsigned column=0;column<Columns;++column){unsigned first=segment*(Columns+1)+column,last=first+Columns+1;
                triangles.insert(triangles.end(),{vertices[first],vertices[first+1],vertices[last],vertices[last],vertices[first+1],vertices[last+1]});}
        }
        return triangles;
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
    void save(const std::vector<DWORD>&pixels,const std::wstring &file,unsigned width=Width,unsigned height=Height,bool alpha=false){
        const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0,0,0xf8,0x1e,0xf3,0x2e}};
        Gdiplus::Bitmap image(width,height,width*4,alpha?PixelFormat32bppARGB:PixelFormat32bppRGB,(BYTE*)pixels.data());Check(image.Save((output_/file).c_str(),&png,nullptr)==Gdiplus::Ok,"PNG save");
    }
public:
    Preview(std::filesystem::path client,std::filesystem::path output,std::filesystem::path materials):output_(std::move(output)){
        std::filesystem::create_directories(output_);std::ofstream(output_/L"study.jsonl",std::ios::trunc);
        WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FENBMeadowStudy";RegisterClassW(&wc);
        window_=CreateWindowW(wc.lpszClassName,L"FENB meadow study",WS_OVERLAPPEDWINDOW,0,0,Width,Height,nullptr,nullptr,wc.hInstance,nullptr);
        d3d_.p=Direct3DCreate9(D3D_SDK_VERSION);Check(d3d_.p,"D3D9");D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.hDeviceWindow=window_;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
        pp.BackBufferWidth=Width;pp.BackBufferHeight=Height;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24X8;
        if(SUCCEEDED(d3d_->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,pp.BackBufferFormat,TRUE,D3DMULTISAMPLE_8_SAMPLES,nullptr))){pp.MultiSampleType=D3DMULTISAMPLE_8_SAMPLES;samples_=8;}
        Check(SUCCEEDED(d3d_->CreateDevice(0,D3DDEVTYPE_HAL,window_,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,d_.out())),"D3D device");
        d3dx_=LoadLibraryExW(L"d3dx9_43.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);Check(d3dx_,"system D3DX9");
        load(ground_[0],client/L"World/Texture/ground_lawn001.dds");load(ground_[1],client/L"World/Texture/ground_n_ice02.dds");
        load(art_[0],materials/L"leaf-surface.png");load(art_[1],materials/L"snow-surface.png");
        Com<ID3DBlob> code,error;HRESULT hr=D3DCompile(GrassPS,strlen(GrassPS),"meadow-study",nullptr,nullptr,"main","ps_2_a",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,code.out(),error.out());
        if(error.p)fputs((char*)error->GetBufferPointer(),stderr);Check(SUCCEEDED(hr),"grass shader compile");Check(SUCCEEDED(d_->CreatePixelShader((DWORD*)code->GetBufferPointer(),grassPS_.out())),"grass shader");
        projection_={};projection_._11=1.4f;projection_._22=1.4f*Width/Height;projection_._33=100/99.9f;projection_._34=1;projection_._43=-.1f*projection_._33;
        Random r;constexpr unsigned Grid=42;
        for(unsigned z=0;z<Grid;++z)for(unsigned x=0;x<Grid;++x){
            float px=-1.5f+(x+.12f+r.next()*.76f)*3/Grid,pz=-1.5f+(z+.12f+r.next()*.76f)*3/Grid;
            const float edge=Smooth(0,.35f,std::min(1.5f-std::abs(px),1.5f-std::abs(pz)));
            const float lumps=.5f+.28f*std::sin(px*2.4f+pz*.7f)+.22f*std::cos(pz*3.2f-px*.9f);
            if(r.next()>(.12f+.52f*std::clamp(lumps,0.f,1.f))*edge)continue;
            Blade c{};c.root={px,.004f,pz};float length=r.next();
            c.height=length<.25f?.12f+length*.48f:length<.85f?.25f+(length-.25f)*.35f:.46f+(length-.85f)*.9f;
            c.width=.009f+r.next()*.013f;
            c.yaw=r.next()*6.2831853f;c.bend=.35f+r.next()*.45f;c.twist=(r.next()-.5f)*.60f;c.mirror=r.next()>.5f;c.seasonGate=r.next();c.variant=(x+z*3)%4;blades_.push_back(c);
        }
    }
    ~Preview(){if(d3dx_)FreeLibrary(d3dx_);if(window_)DestroyWindow(window_);}
    std::vector<DWORD> frame(bool winter,bool close,bool grass,bool detail=false){
        view_=detail?Look({.045f,.19f,-.48f},{0,.14f,.035f}):close?Look({.15f,.78f,-2.8f},{0,.23f,0}):Look({1.9f,2.0f,-2.6f},{0,.12f,0});setup();
        Check(SUCCEEDED(d_->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,winter?0xffc4ccd7:0xff8ba4ad,1,0)),"clear");Check(SUCCEEDED(d_->BeginScene()),"BeginScene");
        d_->SetPixelShader(nullptr);d_->SetTexture(0,ground_[winter?1:0]);d_->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);d_->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
        DWORD floorColor=winter?Color(1.f,.98f,.95f):Color(.86f,.87f,.80f);
        const Vertex floor[]={{-64,0,-64,floorColor,-22.4f,-22.4f},{-64,0,64,floorColor,-22.4f,22.4f},{64,0,-64,floorColor,22.4f,-22.4f},{64,0,64,floorColor,22.4f,22.4f}};
        Check(SUCCEEDED(d_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,floor,sizeof(Vertex))),"ground draw");
        if(grass){
            d_->SetPixelShader(grassPS_);d_->SetTexture(0,art_[0]);
            const float season[]={winter?1.f:0.f,0,0,0};d_->SetPixelShaderConstantF(0,season,1);
            std::vector<Vertex> leaves,snow;
            const auto selected=detail?std::vector<Blade>{DetailBlade()}:blades_;
            for(const auto &blade:selected){if(winter&&blade.seasonGate>WinterCoverage)continue;
                auto vertices=mesh(blade,winter);leaves.insert(leaves.end(),vertices.begin(),vertices.end());
                if(winter){vertices=mesh(blade,true,true);snow.insert(snow.end(),vertices.begin(),vertices.end());}}
            Check(SUCCEEDED(d_->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(leaves.size()/3),leaves.data(),sizeof(Vertex))),"leaf draw");
            if(winter){
                d_->SetTexture(0,art_[1]);const float coating[]={2,0,0,0};d_->SetPixelShaderConstantF(0,coating,1);
                d_->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);d_->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);d_->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);d_->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
                Check(SUCCEEDED(d_->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(snow.size()/3),snow.data(),sizeof(Vertex))),"attached snow draw");
            }
        }
        Check(SUCCEEDED(d_->EndScene()),"EndScene");return read();
    }
    void capture(bool winter,bool close,bool detail=false){
        auto bare=frame(winter,close,false,detail),grass=frame(winter,close,true,detail);
        std::wstring name=(winter?L"winter-":L"summer-")+std::wstring(detail?L"leaf-detail":close?L"close":L"game");save(bare,name+L"-ground.png");save(grass,name+L".png");
        unsigned changed=0,leaves=0,snowTriangles=0;for(size_t i=0;i<grass.size();++i)changed+=(grass[i]&0xffffff)!=(bare[i]&0xffffff);
        for(const auto &c:blades_)if(!winter||c.seasonGate<=WinterCoverage){++leaves;if(winter)snowTriangles+=unsigned(mesh(c,true,true).size()/3);}
        if(detail){leaves=1;snowTriangles=winter?unsigned(mesh(DetailBlade(),true,true).size()/3):0;}
        printf("STUDY leaves=%u changed=%u\n",leaves,changed);Check(changed>1000&&leaves>0,"nonempty grass study");
        printf("CAPTURE %ls patch=3x3 leaves=%u triangles=%u changedPixels=%u winter=%d MSAA=%u\n",name.c_str(),leaves,leaves*32+snowTriangles,changed,winter,samples_);
        std::ofstream log(output_/L"study.jsonl",std::ios::app);log<<"{\"season\":\""<<(winter?"winter":"summer")<<"\",\"camera\":\""<<(detail?"leaf-detail":close?"close":"game")<<"\",\"patchMeters\":[3,3],\"leaves\":"<<leaves<<",\"triangles\":"<<leaves*32+snowTriangles<<",\"changedPixels\":"<<changed<<",\"msaa\":"<<samples_<<"}\n";
    }
};
}
int wmain(int argc,wchar_t **argv){
    if(argc!=4){puts("Usage: meadow-preview <read-only-client-artwork-root> <output-artifacts-directory> <painted-material-directory>");return 2;}
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)return 3;
    int result=0;try{Preview p(argv[1],argv[2],argv[3]);p.capture(false,true);p.capture(false,false);p.capture(true,true);p.capture(true,false);p.capture(false,true,true);p.capture(true,true,true);}
    catch(const std::exception &e){fprintf(stderr,"FAIL: %s\n",e.what());result=1;}Gdiplus::GdiplusShutdown(token);return result;
}
