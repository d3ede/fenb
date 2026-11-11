#include <windows.h>
#include <cstdio>
#include <string>
namespace {
std::wstring output;
int saved=0;
BOOL CALLBACK Names(HMODULE module,LPCWSTR type,LPWSTR name,LONG_PTR){
    HRSRC resource=FindResourceW(module,name,type);if(!resource)return TRUE;
    DWORD size=SizeofResource(module,resource);if(!size||size>2*1024*1024)return TRUE;
    auto* data=static_cast<const char*>(LockResource(LoadResource(module,resource)));if(!data)return TRUE;
    std::string text(data,size);
    if(text.find("vs.1.")==std::string::npos&&text.find("vs_")==std::string::npos&&text.find("technique")==std::string::npos&&text.find("ps_")==std::string::npos)return TRUE;
    // Text shader resources only; do not extract executable code or unrelated client content.
    size_t printable=0;for(unsigned char c:text)if(c>=32||c=='\n'||c=='\r'||c=='\t')++printable;
    if(printable<double(size)*.9)return TRUE;
    wchar_t label[80]{};
    if(IS_INTRESOURCE(name))swprintf(label,80,L"resource-%u",unsigned(reinterpret_cast<ULONG_PTR>(name)));
    else swprintf(label,80,L"resource-%d",saved);
    auto file=output+L"\\"+label+L".txt";FILE* f=_wfopen(file.c_str(),L"wb");
    if(f){fwrite(data,1,size,f);fclose(f);++saved;printf("%ls: %lu bytes\n",label,size);}
    return TRUE;
}
BOOL CALLBACK Types(HMODULE module,LPWSTR type,LONG_PTR){EnumResourceNamesW(module,type,Names,0);return TRUE;}
}
int wmain(int argc,wchar_t** argv){
    if(argc!=3)return 2;output=argv[2];CreateDirectoryW(output.c_str(),nullptr);
    auto module=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if(!module)return 3;EnumResourceTypesW(module,Types,0);FreeLibrary(module);
    printf("Text shader resources saved: %d\n",saved);return 0;
}
