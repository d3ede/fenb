// Offline PNG-to-DXT5 compiler. D3DX/GDI+ are authoring dependencies only.
#include "common.hpp"
#include <gdiplus.h>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace {
void Check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
float Smooth(float a,float b,float x){float t=std::clamp((x-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
constexpr unsigned Size=1024;
using SurfaceLoad=HRESULT(WINAPI *)(IDirect3DSurface9*,const PALETTEENTRY*,const RECT*,const void*,D3DFORMAT,UINT,const PALETTEENTRY*,const RECT*,DWORD,D3DCOLOR);
using Save=HRESULT(WINAPI *)(LPCWSTR,int,IDirect3DBaseTexture9*,const PALETTEENTRY*);
}
int wmain(int argc,wchar_t **argv){
    if(argc!=3){fputs("Usage: grass-artc <source.png> <output.dds>\n",stderr);return 1;}
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;
    if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)return 2;
    int result=0;
    try{
        Gdiplus::Bitmap image(argv[1]);Check(image.GetLastStatus()==Gdiplus::Ok,"PNG decode");
        const unsigned sourceWidth=image.GetWidth(),sourceHeight=image.GetHeight();
        Check(sourceWidth>0&&sourceHeight>0&&sourceWidth<=4096&&sourceHeight<=4096,"source bounds");
        Gdiplus::Rect rectangle(0,0,sourceWidth,sourceHeight);Gdiplus::BitmapData lock{};
        Check(image.LockBits(&rectangle,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&lock)==Gdiplus::Ok,"PNG pixels");
        std::vector<DWORD> source(sourceWidth*sourceHeight);
        for(unsigned y=0;y<sourceHeight;++y)memcpy(source.data()+y*sourceWidth,(BYTE*)lock.Scan0+y*lock.Stride,sourceWidth*4);
        image.UnlockBits(&lock);
        unsigned left=sourceWidth,top=sourceHeight,right=0,bottom=0;
        for(unsigned y=0;y<sourceHeight;++y)for(unsigned x=0;x<sourceWidth;++x)if((source[y*sourceWidth+x]>>24)>=128){left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}
        Check(right>left&&bottom>top,"nonempty alpha silhouette");
        std::vector<DWORD> pixels(Size*Size);
        for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x){
            float px=left+float(x)*(right-left)/(Size-1),py=top+float(y)*(bottom-top)/(Size-1);
            unsigned ix=unsigned(px),iy=unsigned(py);float fx=px-ix,fy=py-iy,alpha=0,rgb[3]{};
            for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){DWORD p=source[std::min(sourceHeight-1,iy+dy)*sourceWidth+std::min(sourceWidth-1,ix+dx)];float w=(dx?fx:1-fx)*(dy?fy:1-fy),a=(p>>24)/255.f;alpha+=a*w;for(unsigned c=0;c<3;++c)rgb[c]+=((p>>(c*8))&255)*a*w;}
            float a=Smooth(.18f,.80f,alpha);DWORD out=DWORD(a*255+.5f)<<24;
            if(alpha>0)for(unsigned c=0;c<3;++c)out|=DWORD(std::clamp(rgb[c]/alpha,0.f,255.f)+.5f)<<(c*8);
            pixels[y*Size+x]=out;
        }
        WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FENBGrassArtCompiler";RegisterClassW(&wc);
        HWND window=CreateWindowW(wc.lpszClassName,L"Offline grass compiler",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,wc.hInstance,nullptr);
        Com<IDirect3D9> d3d;d3d.p=Direct3DCreate9(D3D_SDK_VERSION);Check(d3d.p,"D3D9");
        D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.hDeviceWindow=window;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.BackBufferWidth=pp.BackBufferHeight=32;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
        Com<IDirect3DDevice9> device;HRESULT hr=d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,device.out());
        if(FAILED(hr))hr=d3d->CreateDevice(0,D3DDEVTYPE_NULLREF,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,device.out());
        if(FAILED(hr))fprintf(stderr,"D3D CreateDevice: 0x%08lx\n",hr);Check(SUCCEEDED(hr),"D3D device");
        HMODULE library=LoadLibraryExW(L"d3dx9_43.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);Check(library,"system D3DX");
        auto upload=reinterpret_cast<SurfaceLoad>(GetProcAddress(library,"D3DXLoadSurfaceFromMemory"));
        auto save=reinterpret_cast<Save>(GetProcAddress(library,"D3DXSaveTextureToFileW"));Check(upload&&save,"D3DX APIs");
        Com<IDirect3DTexture9> texture;Check(SUCCEEDED(device->CreateTexture(Size,Size,0,0,D3DFMT_DXT5,D3DPOOL_MANAGED,texture.out(),nullptr)),"DXT5 texture");
        const float reference=float(std::count_if(pixels.begin(),pixels.end(),[](DWORD p){return (p>>24)>=128;}))/pixels.size();
        for(unsigned level=0,extent=Size;level<texture->GetLevelCount();++level){
            auto padded=pixels;std::vector<bool> valid(pixels.size());for(size_t i=0;i<pixels.size();++i)valid[i]=(pixels[i]>>24)>8;
            for(unsigned pass=0;pass<4;++pass){auto next=padded;auto filled=valid;
                for(unsigned y=0;y<extent;++y)for(unsigned x=0;x<extent;++x){size_t i=y*extent+x;if(valid[i])continue;unsigned sum[3]{},n=0;
                    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){int a=int(x)+dx,b=int(y)+dy;if(a<0||b<0||a>=int(extent)||b>=int(extent))continue;size_t j=b*extent+a;if(!valid[j])continue;for(unsigned c=0;c<3;++c)sum[c]+=(padded[j]>>(c*8))&255;++n;}
                    if(n){next[i]=(padded[i]&0xff000000)|(sum[0]/n)|((sum[1]/n)<<8)|((sum[2]/n)<<16);filled[i]=true;}}
                padded.swap(next);valid.swap(filled);}
            Com<IDirect3DSurface9> surface;Check(SUCCEEDED(texture->GetSurfaceLevel(level,surface.out())),"mip surface");RECT bounds{0,0,LONG(extent),LONG(extent)};
            Check(SUCCEEDED(upload(surface,nullptr,nullptr,padded.data(),D3DFMT_A8R8G8B8,extent*4,nullptr,&bounds,1,0)),"mip compression");
            if(extent==1)break;
            unsigned e=extent/2;std::vector<DWORD> mip(e*e);
            for(unsigned y=0;y<e;++y)for(unsigned x=0;x<e;++x){unsigned alpha=0,rgb[3]{};for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){DWORD p=pixels[(y*2+dy)*extent+x*2+dx];unsigned a=p>>24;alpha+=a;for(unsigned c=0;c<3;++c)rgb[c]+=((p>>(c*8))&255)*a;}
                DWORD out=((alpha+2)/4)<<24;for(unsigned c=0;c<3;++c)out|=(alpha?rgb[c]/alpha:0)<<(c*8);mip[y*e+x]=out;}
            // Retain leaf coverage at the runtime .5 cutout threshold after minification.
            if(e>=16){float lo=0,hi=8;for(unsigned step=0;step<18;++step){float scale=(lo+hi)*.5f;unsigned covered=0;for(DWORD p:mip)covered+=((p>>24)*scale)>=128;float fraction=float(covered)/mip.size();if(fraction<reference)lo=scale;else hi=scale;}
                for(auto &p:mip)p=(p&0xffffff)|(unsigned(std::min(255.f,(p>>24)*hi)+.5f)<<24);}
            pixels.swap(mip);extent=e;
        }
        Check(SUCCEEDED(save(argv[2],4,texture,nullptr)),"DDS save");
        printf("Grass artwork: %ux%u source, %ux%u DXT5, %lu mips, %.2f%% cutout coverage\n",sourceWidth,sourceHeight,Size,Size,texture->GetLevelCount(),reference*100);
        FreeLibrary(library);DestroyWindow(window);
    }catch(const std::exception &e){fprintf(stderr,"FAIL: %s\n",e.what());result=3;}
    Gdiplus::GdiplusShutdown(token);return result;
}
