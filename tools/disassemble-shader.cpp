#include <windows.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <vector>
int main(int argc,char** argv){
    if(argc!=3)return 2;FILE* in=fopen(argv[1],"rb");if(!in)return 3;fseek(in,0,SEEK_END);auto n=ftell(in);rewind(in);
    if(n<=0||n>65536){fclose(in);return 4;}std::vector<unsigned char> code(n);fread(code.data(),1,n,in);fclose(in);
    ID3DBlob* text=nullptr;HRESULT hr=D3DDisassemble(code.data(),code.size(),0,nullptr,&text);if(FAILED(hr))return 5;
    FILE* out=fopen(argv[2],"wb");if(!out){text->Release();return 6;}fwrite(text->GetBufferPointer(),1,text->GetBufferSize(),out);fclose(out);text->Release();return 0;
}
