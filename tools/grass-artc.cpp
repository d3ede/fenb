// Offline PNG decode and atlas-aware mip generation. No runtime decoder is used.
#include "common.hpp"
#include <gdiplus.h>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <stdexcept>
namespace {
void Check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
}
int wmain(int argc,wchar_t **argv){
    if(argc!=3){fputs("Usage: grass-artc <source.png> <output.dds>\n",stderr);return 1;}
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;
    if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)return 2;
    int result=0;
    try{
        Gdiplus::Bitmap image(argv[1]);Check(image.GetLastStatus()==Gdiplus::Ok,"PNG decode");
        const unsigned width=image.GetWidth(),height=image.GetHeight();
        Check(width==256&&height==128,"expected the supplied 256x128 atlas");
        Gdiplus::Rect rectangle(0,0,width,height);Gdiplus::BitmapData lock{};
        Check(image.LockBits(&rectangle,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&lock)==Gdiplus::Ok,"PNG pixels");
        std::vector<DWORD> pixels(width*height);
        for(unsigned y=0;y<height;++y)memcpy(pixels.data()+y*width,(BYTE*)lock.Scan0+ptrdiff_t(y)*lock.Stride,width*4);
        image.UnlockBits(&lock);
        const unsigned boundaries[]={0,width/2,width*3/4,width};float reference[3]{};
        for(unsigned region=0;region<3;++region){unsigned covered=0;for(unsigned y=0;y<height;++y)for(unsigned x=boundaries[region];x<boundaries[region+1];++x)covered+=(pixels[y*width+x]>>24)>=128;
            reference[region]=float(covered)/((boundaries[region+1]-boundaries[region])*height);}
        // Uncompressed ARGB keeps the complete supplied base-level RGBA exact.
        DWORD header[32]{};header[0]=0x20534444;header[1]=124;header[2]=0x2100f;header[3]=height;header[4]=width;header[5]=width*4;header[7]=9;
        header[19]=32;header[20]=0x41;header[22]=32;header[23]=0x00ff0000;header[24]=0x0000ff00;header[25]=0x000000ff;header[26]=0xff000000;header[27]=0x401008;
        std::ofstream output(std::filesystem::path(argv[2]),std::ios::binary|std::ios::trunc);Check(bool(output),"DDS output");output.write(reinterpret_cast<char*>(header),sizeof(header));
        for(unsigned level=0,w=width,h=height;level<9;++level){
            output.write(reinterpret_cast<char*>(pixels.data()),pixels.size()*sizeof(DWORD));
            if(level==8)break;
            unsigned nextW=std::max(1u,w/2),nextH=std::max(1u,h/2);std::vector<DWORD> mip(nextW*nextH);
            for(unsigned y=0;y<nextH;++y)for(unsigned x=0;x<nextW;++x){unsigned alpha=0,rgb[3]{};
                for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){DWORD p=pixels[std::min(h-1,y*2+dy)*w+std::min(w-1,x*2+dx)];unsigned a=p>>24;alpha+=a;for(unsigned c=0;c<3;++c)rgb[c]+=((p>>(c*8))&255)*a;}
                DWORD out=((alpha+2)/4)<<24;for(unsigned c=0;c<3;++c)out|=(alpha?rgb[c]/alpha:0)<<(c*8);mip[y*nextW+x]=out;}
            // Preserve each UV island separately until it becomes sub-texel.
            if(nextW>=4){const unsigned edges[]={0,nextW/2,nextW*3/4,nextW};
                for(unsigned region=0;region<3;++region){float lo=0,hi=8;
                    if(reference[region]>0)for(unsigned step=0;step<18;++step){float scale=(lo+hi)*.5f;unsigned covered=0;for(unsigned y=0;y<nextH;++y)for(unsigned x=edges[region];x<edges[region+1];++x)covered+=((mip[y*nextW+x]>>24)*scale)>=128;
                        float fraction=float(covered)/((edges[region+1]-edges[region])*nextH);if(fraction<reference[region])lo=scale;else hi=scale;}
                    else hi=0;
                    for(unsigned y=0;y<nextH;++y)for(unsigned x=edges[region];x<edges[region+1];++x){DWORD &p=mip[y*nextW+x];p=(p&0xffffff)|(unsigned(std::min(255.f,(p>>24)*hi)+.5f)<<24);}}}
            pixels.swap(mip);w=nextW;h=nextH;
        }
        Check(bool(output),"DDS write");printf("Grass atlas: %ux%u, exact RGBA base, 9 atlas-aware mips\n",width,height);
    }catch(const std::exception &e){fprintf(stderr,"FAIL: %s\n",e.what());result=3;}
    Gdiplus::GdiplusShutdown(token);return result;
}
