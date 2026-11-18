// Isolated 3x3 metre study: refreshed vanilla-style alpha strips and native comparison.
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
struct Card {Vec root;float width,height,yaw,bend,twist,seasonGate,tone;bool mirror;};
Card DetailCard(){return {{0,-.006f,0},.40f,.46f,0,.12f,.08f,0,1,false};}
using LoadTexture=HRESULT(WINAPI *)(IDirect3DDevice9*,LPCWSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,DWORD,DWORD,D3DCOLOR,void*,PALETTEENTRY*,IDirect3DTexture9**);
constexpr float WinterCoverage=.90f;
const char *GrassPS=R"(
sampler2D Art:register(s0);
sampler2D Snow:register(s1);
float4 Season:register(c0);
float4 ArtworkUV:register(c1);
float4 main(float4 light:COLOR0,float2 uv:TEXCOORD0):COLOR0 {
 float4 art=tex2D(Art,uv*ArtworkUV.xy+ArtworkUV.zw);
 float alpha=Season.y>.5?art.a:smoothstep(.18,.80,art.a);clip(alpha-(Season.y>.5?.035:.30));
 float3 color=art.rgb;
 if(Season.y<.5)color=saturate(color*float3(.88,1.04,.94));
 if(Season.x>.5){
  float value=dot(color,float3(.2126,.7152,.0722));
  float3 stem=lerp(float3(.24,.27,.17),float3(.55,.58,.42),value);
  float3 snow=tex2D(Snow,uv*float2(2,1.3)).rgb*(.78+.22*value);
  float coating=smoothstep(.60,.95,1-uv.y)*(.65+.35*smoothstep(.3,.7,value));
  color=lerp(stem,snow,coating);
 }
 float root=smoothstep(0,.065,1-uv.y);
 return float4(color*light.rgb,alpha*root);
})";
class Preview {
    HWND window_=nullptr;HMODULE d3dx_=nullptr;
    Com<IDirect3D9> d3d_;Com<IDirect3DDevice9> d_;
    Com<IDirect3DTexture9> ground_[2],art_[2],snow_;Com<IDirect3DPixelShader9> grassPS_;
    std::vector<Card> cards_;std::filesystem::path output_;
    unsigned samples_=0;D3DMATRIX view_{},projection_{};
    float artworkUV_[2][4]{{1,1,0,0},{1,1,0,0}};
    void load(Com<IDirect3DTexture9>&t,const std::filesystem::path &path){
        auto fn=reinterpret_cast<LoadTexture>(GetProcAddress(d3dx_,"D3DXCreateTextureFromFileExW"));Check(fn,"D3DX texture API");
        Check(SUCCEEDED(fn(d_,path.c_str(),UINT(-1),UINT(-1),0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,3,5,0,nullptr,nullptr,t.out())),"native texture read");
    }
    void alphaMips(IDirect3DTexture9 *texture,float *uv){
        D3DSURFACE_DESC desc{};texture->GetLevelDesc(0,&desc);
        unsigned width=desc.Width,height=desc.Height;std::vector<DWORD> pixels(width*height);
        D3DLOCKED_RECT lock{};Check(SUCCEEDED(texture->LockRect(0,&lock,nullptr,D3DLOCK_READONLY)),"alpha source lock");
        for(unsigned y=0;y<height;++y)memcpy(pixels.data()+y*width,(BYTE*)lock.pBits+y*lock.Pitch,width*4);texture->UnlockRect(0);
        unsigned minX=width,minY=height,maxX=0,maxY=0,occupied=0;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)if((pixels[y*width+x]>>24)>=128){minX=std::min(minX,x);maxX=std::max(maxX,x);minY=std::min(minY,y);maxY=std::max(maxY,y);++occupied;}
        Check(occupied>100&&occupied<width*height*.65f,"transparent grass artwork");
        uv[0]=float(maxX-minX)/width;uv[1]=float(maxY-minY)/height;uv[2]=(minX+.5f)/width;uv[3]=(minY+.5f)/height;
        printf("ARTWORK size=%ux%u occupiedPixels=%u bounds=%u,%u..%u,%u\n",width,height,occupied,minX,minY,maxX,maxY);
        for(unsigned level=0;level<texture->GetLevelCount();++level){
            // Extend RGB only into empty texels of the owned texture, preserving alpha.
            auto padded=pixels;std::vector<unsigned char> valid(pixels.size());
            for(size_t i=0;i<pixels.size();++i)valid[i]=(pixels[i]>>24)>8;
            for(unsigned pass=0;pass<4;++pass){auto next=padded;auto nextValid=valid;
                for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){size_t i=y*width+x;if(valid[i])continue;unsigned rgb[3]{},n=0;
                    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){int px=int(x)+dx,py=int(y)+dy;if(px<0||py<0||px>=int(width)||py>=int(height))continue;size_t j=py*width+px;if(!valid[j])continue;
                        for(unsigned c=0;c<3;++c)rgb[c]+=(padded[j]>>(c*8))&255;++n;}
                    if(n){next[i]=(padded[i]&0xff000000)|(rgb[0]/n)|((rgb[1]/n)<<8)|((rgb[2]/n)<<16);nextValid[i]=1;}}
                padded.swap(next);valid.swap(nextValid);}
            Check(SUCCEEDED(texture->LockRect(level,&lock,nullptr,0)),"alpha mip lock");
            for(unsigned y=0;y<height;++y)memcpy((BYTE*)lock.pBits+y*lock.Pitch,padded.data()+y*width,width*4);texture->UnlockRect(level);
            if(level+1==texture->GetLevelCount())break;
            unsigned w=std::max(1u,width/2),h=std::max(1u,height/2);std::vector<DWORD> mip(w*h);
            for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){unsigned alpha=0,rgb[3]{};
                for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){DWORD p=pixels[std::min(height-1,y*2+dy)*width+std::min(width-1,x*2+dx)];unsigned a=p>>24;alpha+=a;for(unsigned c=0;c<3;++c)rgb[c]+=((p>>(c*8))&255)*a;}
                mip[y*w+x]=((alpha+2)/4)<<24;for(unsigned c=0;c<3;++c)mip[y*w+x]|=(alpha?rgb[c]/alpha:0)<<(c*8);}
            pixels.swap(mip);width=w;height=h;
        }
    }
    std::vector<Vertex> mesh(const Card &c,bool winter)const{
        constexpr unsigned Segments=4,Columns=4;
        const float h=c.height*(winter?.78f:1.f),w=c.width*(winter?.94f:1.f);
        const Vec sun=Unit({-.30f,.86f,-.40f});
        std::vector<Vertex> vertices,triangles;
        for(unsigned segment=0;segment<=Segments;++segment){
            float t=float(segment)/Segments,angle=c.yaw+c.twist*t;
            Vec side{std::cos(angle),0,std::sin(angle)},normal{-side.z,0,side.x};
            Vec center=c.root+Vec{0,h*t-(winter?.028f:0),0}+normal*(h*c.bend*t*t);
            float light=(winter?.88f:.91f)+(winter?.12f:.30f)*std::abs(Dot(Unit(normal+Vec{0,.35f,0}),sun));
            light*=c.tone*(.84f+.16f*Smooth(0,.65f,t));
            for(unsigned column=0;column<=Columns;++column){
                float across=float(column)/Columns;
                Vec position=center+side*((across-.5f)*w)+normal*(w*.14f*std::sin(across*6.2831853f+c.yaw)*(1-t*.5f));
                vertices.push_back({position.x,position.y,position.z,Color(light,light,light),c.mirror?1-across:across,1-t});
            }
        }
        for(unsigned segment=0;segment<Segments;++segment){
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
        load(art_[0],materials/L"vanilla-meadow.png");load(art_[1],client/L"Model/Texture/Obj_MaCoPrGr07.dds");load(snow_,materials/L"snow-surface.png");
        alphaMips(art_[0],artworkUV_[0]);alphaMips(art_[1],artworkUV_[1]);
        Com<ID3DBlob> code,error;HRESULT hr=D3DCompile(GrassPS,strlen(GrassPS),"meadow-study",nullptr,nullptr,"main","ps_2_a",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,code.out(),error.out());
        if(error.p)fputs((char*)error->GetBufferPointer(),stderr);Check(SUCCEEDED(hr),"grass shader compile");Check(SUCCEEDED(d_->CreatePixelShader((DWORD*)code->GetBufferPointer(),grassPS_.out())),"grass shader");
        projection_={};projection_._11=1.4f;projection_._22=1.4f*Width/Height;projection_._33=100/99.9f;projection_._34=1;projection_._43=-.1f*projection_._33;
        Random r;constexpr unsigned Grid=24;
        for(unsigned z=0;z<Grid;++z)for(unsigned x=0;x<Grid;++x){
            float px=-1.5f+(x+.12f+r.next()*.76f)*3/Grid,pz=-1.5f+(z+.12f+r.next()*.76f)*3/Grid;
            const float edge=Smooth(0,.35f,std::min(1.5f-std::abs(px),1.5f-std::abs(pz)));
            const float lumps=.5f+.28f*std::sin(px*2.4f+pz*.7f)+.22f*std::cos(pz*3.2f-px*.9f);
            if(r.next()>(.20f+.64f*std::clamp(lumps,0.f,1.f))*edge)continue;
            Card c{};c.root={px,-.006f,pz};float length=r.next();
            c.height=length<.2f?.17f+length*.35f:.25f+(length-.2f)*.40f;
            c.width=.33f+r.next()*.18f;
            c.yaw=r.next()*6.2831853f;c.bend=.06f+r.next()*.16f;c.twist=(r.next()-.5f)*.22f;c.mirror=r.next()>.5f;c.seasonGate=r.next();c.tone=.84f+r.next()*.25f;cards_.push_back(c);
        }
    }
    ~Preview(){if(d3dx_)FreeLibrary(d3dx_);if(window_)DestroyWindow(window_);}
    std::vector<DWORD> frame(bool winter,bool close,bool grass,bool detail=false,bool native=false){
        view_=detail?Look({.02f,.30f,-.90f},{0,.23f,.035f}):close?Look({.15f,.78f,-2.8f},{0,.23f,0}):Look({1.9f,2.0f,-2.6f},{0,.12f,0});setup();
        Check(SUCCEEDED(d_->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,winter?0xffc4ccd7:0xff8ba4ad,1,0)),"clear");Check(SUCCEEDED(d_->BeginScene()),"BeginScene");
        d_->SetPixelShader(nullptr);d_->SetTexture(0,ground_[winter?1:0]);d_->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);d_->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
        DWORD floorColor=winter?Color(1.f,.98f,.95f):Color(.86f,.87f,.80f);
        const Vertex floor[]={{-64,0,-64,floorColor,-22.4f,-22.4f},{-64,0,64,floorColor,-22.4f,22.4f},{64,0,-64,floorColor,22.4f,-22.4f},{64,0,64,floorColor,22.4f,22.4f}};
        Check(SUCCEEDED(d_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,floor,sizeof(Vertex))),"ground draw");
        if(grass){
            d_->SetPixelShader(grassPS_);d_->SetTexture(0,art_[native?1:0]);d_->SetTexture(1,snow_);
            const float season[]={winter?1.f:0.f,native?1.f:0.f,0,0};d_->SetPixelShaderConstantF(0,season,1);
            d_->SetPixelShaderConstantF(1,artworkUV_[native?1:0],1);
            d_->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);d_->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);d_->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);d_->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
            for(unsigned sampler:{0u,1u}){d_->SetSamplerState(sampler,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);d_->SetSamplerState(sampler,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);d_->SetSamplerState(sampler,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);d_->SetSamplerState(sampler,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);d_->SetSamplerState(sampler,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);}
            auto selected=detail?std::vector<Card>{DetailCard()}:cards_;
            std::stable_sort(selected.begin(),selected.end(),[this](const Card&a,const Card&b){auto depth=[this](Vec p){return p.x*view_._13+p.y*view_._23+p.z*view_._33;};return depth(a.root)>depth(b.root);});
            for(const auto &card:selected){if(winter&&card.seasonGate>WinterCoverage)continue;auto vertices=mesh(card,winter);
                Check(SUCCEEDED(d_->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(vertices.size()/3),vertices.data(),sizeof(Vertex))),"grass strip draw");}
        }
        Check(SUCCEEDED(d_->EndScene()),"EndScene");return read();
    }
    void capture(bool winter,bool close,bool detail=false,bool native=false){
        auto bare=frame(winter,close,false,detail,native),grass=frame(winter,close,true,detail,native);
        std::wstring name=std::wstring(native?L"native-":L"")+(winter?L"winter-":L"summer-")+std::wstring(detail?L"strip-detail":close?L"close":L"game");save(bare,name+L"-ground.png");save(grass,name+L".png");
        unsigned changed=0,cards=0;for(size_t i=0;i<grass.size();++i)changed+=(grass[i]&0xffffff)!=(bare[i]&0xffffff);
        std::vector<DWORD> coverage(grass.size());for(size_t i=0;i<grass.size();++i)coverage[i]=(grass[i]&0xffffff)!=(bare[i]&0xffffff)?0xffffffff:0xff000000;
        save(coverage,name+L"-coverage.png");
        for(const auto &c:cards_)if(!winter||c.seasonGate<=WinterCoverage)++cards;
        if(detail)cards=1;
        Check(changed>1000&&cards>0,"nonempty grass study");
        printf("CAPTURE %ls patch=3x3 strips=%u triangles=%u changedPixels=%u MSAA=%u\n",name.c_str(),cards,cards*32,changed,samples_);
        std::ofstream log(output_/L"study.jsonl",std::ios::app);log<<"{\"season\":\""<<(winter?"winter":"summer")<<"\",\"artwork\":\""<<(native?"native":"refreshed")<<"\",\"camera\":\""<<(detail?"strip-detail":close?"close":"game")<<"\",\"patchMeters\":[3,3],\"strips\":"<<cards<<",\"triangles\":"<<cards*32<<",\"changedPixels\":"<<changed<<",\"msaa\":"<<samples_<<"}\n";
    }
};
}
int wmain(int argc,wchar_t **argv){
    if(argc!=4){puts("Usage: meadow-preview <read-only-client-artwork-root> <output-artifacts-directory> <painted-material-directory>");return 2;}
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)return 3;
    int result=0;try{Preview p(argv[1],argv[2],argv[3]);p.capture(false,true);p.capture(false,false);p.capture(true,true);p.capture(true,false);p.capture(false,true,true);p.capture(true,true,true);p.capture(false,true,false,true);p.capture(false,false,false,true);}
    catch(const std::exception &e){fprintf(stderr,"FAIL: %s\n",e.what());result=1;}Gdiplus::GdiplusShutdown(token);return result;
}
